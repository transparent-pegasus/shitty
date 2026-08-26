/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "shitty_vt.h"

#include <lib/vterm/pty.h>
#include <lib/vterm/vterm.h>
#include <lib/vterm/screen.h>
#include <lib/vterm/vt_host.h>
#include <lib/vterm/grapheme.h>
#include <lib/vterm/vt_config.h>
#include <lib/vterm/vt_geometry.h>
#include <lib/vterm/terminal_types.h>
#include <lib/vterm/cell_extra_store.h>

#include <std/str/view.h>
#include <std/ios/input.h>
#include <std/ios/output.h>
#include <std/lib/buffer.h>
#include <std/ptr/scoped.h>
#include <std/mem/obj_pool.h>
#include <std/mem/small_obj_allocator.h>

#include <new>
#include <string.h>
#include <plt/fiber.h>
#include <plt/window.h>
#include <plt/platform.h>
#include <plt/clipboard.h>
#include <plt/platform_headless.h>

using namespace stl;

namespace {
    struct EmbedClipboard;
    struct EmbedHost;
    struct ReplyPty;
}

// The embedder object behind the opaque C handle: every piece
// Vterm::create wants, owned here, plus the capture buffers the C
// calls drain. Everything lives in one pool, the handle included; the
// terminal is made last, so it dies first when free() drops the pool.
struct shitty_vt {
    stl::ObjPool* pool = nullptr;
    plt::Platform* platform = nullptr;
    VtGeometry geometry;
    VtConfig config;
    VtConfigSlot slot;
    VtCellExtras extras;
    stl::SmallObjAllocator* smallObjects = nullptr;
    EmbedHost* host = nullptr;
    ReplyPty* pty = nullptr;
    Vterm* terminal = nullptr;
    // The embedder's struct, never copied. Null while the terminal is
    // being constructed - nothing the constructor publishes is the
    // application's doing, so no callback fires before shitty_vt_new
    // returns - and null for good when the embedder passed none.
    const shitty_vt_callbacks* callbacks = nullptr;
};

namespace {
    // A caller-owned Input over a private copy of the selection: the
    // read may park its fiber, and the selection can move underneath a
    // parked reader. The caller releases with plain delete, which hands
    // the object back to the shared allocator.
    struct SelectionInput final: public Input {
        SelectionInput(stl::SmallObjAllocator* allocator, const void* data, size_t len);

        void operator delete(SelectionInput* input, std::destroying_delete_t) noexcept;

        size_t readImpl(void* data, size_t len) override;

        stl::SmallObjAllocator* allocator;
        Buffer bytes_;
        size_t offset_ = 0;
    };

    struct ClipboardOutput final: public Output {
        ClipboardOutput(stl::SmallObjAllocator* allocator, EmbedClipboard& owner);

        void operator delete(ClipboardOutput* output, std::destroying_delete_t) noexcept;

        size_t writeImpl(const void* data, size_t len) override;
        void finishImpl() override;

        stl::SmallObjAllocator* allocator;
        EmbedClipboard& owner;
        Buffer staged_;
    };

    // One selection: accumulates writes, publishes on finish, and
    // reports the publication to the embedder.
    struct EmbedClipboard final: public plt::Clipboard {
        EmbedClipboard(shitty_vt& vt, int which);

        Input* read() override;
        Output* write() override;

        void publish(const void* data, size_t len);

        shitty_vt& vt;
        int which;
        Buffer content_;
    };

    struct EmbedHost final: public VtHost {
        explicit EmbedHost(shitty_vt& vt);

        plt::Clipboard* primary() override;
        plt::Clipboard* secondary() override;
        plt::WindowInfo info() override;
        void requestFrame() override;
        void requestResize(u32 width, u32 height) override;
        void requestMaximized(bool maximized) override;
        void requestFullscreen(bool fullscreen) override;
        void requestIconify() override;
        void requestRestore() override;
        void requestMove(i32 x, i32 y) override;
        void requestFocus() override;
        void requestAttention() override;
        void requestPointerIcon(plt::PointerIcon icon) override;
        void requestOpenUri(stl::StringView uri) override;
        bool uriSchemeAllowed(stl::StringView scheme) override;
        void titleChanged(const VtermTitleChanged& event) override;
        void resized() override;

        shitty_vt& vt;
        EmbedClipboard primary_;
        EmbedClipboard secondary_;
    };

    // The pty face: one reusable chunk toward the terminal, and every
    // send lands in the reply buffer take_replies drains. No child; the
    // read side never delivers.
    struct ReplyPty final: public PtyHandle {
        explicit ReplyPty(plt::Scheduler& scheduler);

        void resize(const PtySize& size) override;
        void engage() override;
        Chunk* allocate(size_t len) override;
        void send(Chunk* chunk, size_t len) override;
        Chunk* acquire() override;
        void release(Chunk* chunks) override;

        size_t take(uint8_t* out, size_t cap);

        struct ReplyChunk final: public Chunk {
            void* data() override;
            size_t length() override;
            Chunk* next() override;

            Buffer payload_;
            size_t used_ = 0;
            bool loaned_ = false;
        };

        plt::Scheduler& scheduler;
        Buffer replies_;
        size_t drained_ = 0;
        ReplyChunk chunk_;
    };
}

SelectionInput::SelectionInput(SmallObjAllocator* allocator_, const void* data, size_t len)
    : allocator(allocator_)
    , bytes_(data, len)
{
}

void SelectionInput::operator delete(SelectionInput* input, std::destroying_delete_t) noexcept {
    SmallObjAllocator* const owner = input->allocator;
    owner->release(input);
}

size_t SelectionInput::readImpl(void* data, size_t len) {
    const size_t left = bytes_.used() - offset_;
    const size_t count = len < left ? len : left;
    memcpy(data, (const u8*)(bytes_.data()) + offset_, count);
    offset_ += count;
    return count;
}

ClipboardOutput::ClipboardOutput(SmallObjAllocator* allocator_, EmbedClipboard& owner_)
    : allocator(allocator_)
    , owner(owner_)
{
}

void ClipboardOutput::operator delete(ClipboardOutput* output, std::destroying_delete_t) noexcept {
    SmallObjAllocator* const owner = output->allocator;
    owner->release(output);
}

size_t ClipboardOutput::writeImpl(const void* data, size_t len) {
    staged_.append(data, len);
    return len;
}

void ClipboardOutput::finishImpl() {
    owner.publish(staged_.data(), staged_.used());
}

EmbedClipboard::EmbedClipboard(shitty_vt& vt_, int which_)
    : vt(vt_)
    , which(which_)
{
}

Input* EmbedClipboard::read() {
    return vt.smallObjects->make<SelectionInput>(vt.smallObjects, content_.data(), content_.used());
}

Output* EmbedClipboard::write() {
    return vt.smallObjects->make<ClipboardOutput>(vt.smallObjects, *this);
}

void EmbedClipboard::publish(const void* data, size_t len) {
    content_.reset();
    content_.append(data, len);
    if (vt.callbacks != nullptr && vt.callbacks->clipboard_set != nullptr) {
        vt.callbacks->clipboard_set(vt.callbacks->user, which, (const uint8_t*)(data), len);
    }
}

EmbedHost::EmbedHost(shitty_vt& vt_)
    : vt(vt_)
    , primary_(vt_, 0)
    , secondary_(vt_, 1)
{
}

plt::Clipboard* EmbedHost::primary() {
    return &primary_;
}

plt::Clipboard* EmbedHost::secondary() {
    return &secondary_;
}

plt::WindowInfo EmbedHost::info() {
    plt::WindowInfo info;
    info.width = vt.geometry.pixelWidth;
    info.height = vt.geometry.pixelHeight;
    info.screenPixelWidth = vt.geometry.pixelWidth;
    info.screenPixelHeight = vt.geometry.pixelHeight;
    info.focused = true;
    return info;
}

void EmbedHost::requestFrame() {
    if (vt.callbacks != nullptr && vt.callbacks->damaged != nullptr) {
        vt.callbacks->damaged(vt.callbacks->user);
    }
}

void EmbedHost::requestResize(u32 width, u32 height) {
    if (vt.callbacks == nullptr || vt.callbacks->resize_request == nullptr) {
        return;
    }
    // The cell is one pixel square, so the pixel request is already in
    // cells.
    const u32 limit = 0xffff;
    vt.callbacks->resize_request(vt.callbacks->user, (u16)(width < limit ? width : limit), (u16)(height < limit ? height : limit));
}

void EmbedHost::requestMaximized(bool) {
}

void EmbedHost::requestFullscreen(bool) {
}

void EmbedHost::requestIconify() {
}

void EmbedHost::requestRestore() {
}

void EmbedHost::requestMove(i32, i32) {
}

void EmbedHost::requestFocus() {
}

void EmbedHost::requestAttention() {
    if (vt.callbacks != nullptr && vt.callbacks->bell != nullptr) {
        vt.callbacks->bell(vt.callbacks->user);
    }
}

void EmbedHost::requestPointerIcon(plt::PointerIcon) {
}

void EmbedHost::requestOpenUri(StringView uri) {
    if (vt.callbacks != nullptr && vt.callbacks->open_uri != nullptr) {
        vt.callbacks->open_uri(vt.callbacks->user, (const uint8_t*)(uri.data()), uri.length());
    }
}

bool EmbedHost::uriSchemeAllowed(StringView scheme) {
    // The parsed-option default of the full terminal.
    static const StringView allowed[] = {
        StringView(u8"http"),
        StringView(u8"https"),
        StringView(u8"file"),
    };
    for (const StringView& candidate : allowed) {
        if (scheme.length() != candidate.length()) {
            continue;
        }
        bool match = true;
        for (size_t index = 0; index < scheme.length(); ++index) {
            const u8 byte = scheme[index];
            const u8 folded = byte >= 'A' && byte <= 'Z' ? (u8)(byte + ('a' - 'A')) : byte;
            if (folded != candidate[index]) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

void EmbedHost::titleChanged(const VtermTitleChanged& event) {
    if (vt.callbacks != nullptr && vt.callbacks->title_changed != nullptr) {
        vt.callbacks->title_changed(vt.callbacks->user, (const uint8_t*)(event.title.data()), event.title.length());
    }
}

void EmbedHost::resized() {
    if (vt.terminal != nullptr) {
        vt.terminal->windowResized();
    }
}

void* ReplyPty::ReplyChunk::data() {
    return payload_.mutData();
}

size_t ReplyPty::ReplyChunk::length() {
    return used_;
}

PtyHandle::Chunk* ReplyPty::ReplyChunk::next() {
    return nullptr;
}

ReplyPty::ReplyPty(plt::Scheduler& scheduler_)
    : scheduler(scheduler_)
{
}

void ReplyPty::resize(const PtySize&) {
}

void ReplyPty::engage() {
}

PtyHandle::Chunk* ReplyPty::allocate(size_t len) {
    if (chunk_.loaned_) {
        return nullptr;
    }
    chunk_.payload_.reset();
    chunk_.payload_.grow(len);
    chunk_.payload_.seekAbsolute(len);
    chunk_.used_ = len;
    chunk_.loaned_ = true;
    return &chunk_;
}

void ReplyPty::send(Chunk* chunk, size_t len) {
    if (chunk != &chunk_) {
        return;
    }
    chunk_.loaned_ = false;
    replies_.append(chunk_.payload_.data(), len);
}

PtyHandle::Chunk* ReplyPty::acquire() {
    scheduler.current()->park();
    return nullptr;
}

void ReplyPty::release(Chunk*) {
}

size_t ReplyPty::take(uint8_t* out, size_t cap) {
    const size_t left = replies_.used() - drained_;
    const size_t count = cap < left ? cap : left;
    memcpy(out, (const uint8_t*)(replies_.data()) + drained_, count);
    drained_ += count;
    if (drained_ == replies_.used()) {
        replies_.reset();
        drained_ = 0;
    }
    return count;
}

namespace {
    // The VGA palette, the same 16 colors the full terminal defaults to.
    constexpr Color ansiDefaults[AnsiPalette::colorCount] = {
        {0x00, 0x00, 0x00},
        {0xaa, 0x00, 0x00},
        {0x00, 0xaa, 0x00},
        {0xaa, 0x55, 0x00},
        {0x00, 0x00, 0xaa},
        {0xaa, 0x00, 0xaa},
        {0x00, 0xaa, 0xaa},
        {0xaa, 0xaa, 0xaa},
        {0x55, 0x55, 0x55},
        {0xff, 0x55, 0x55},
        {0x55, 0xff, 0x55},
        {0xff, 0xff, 0x55},
        {0x55, 0x55, 0xff},
        {0xff, 0x55, 0xff},
        {0x55, 0xff, 0xff},
        {0xff, 0xff, 0xff},
    };

    static void fillConfig(VtConfig& config, u16 saveLines) {
        config.saveLines = saveLines;
        config.brandName = StringView(u8"shitty-vt");
        config.fg = {0xff, 0xff, 0xff};
        config.bg = {0x00, 0x00, 0x00};
        config.cr = config.fg;
        for (size_t index = 0; index < AnsiPalette::colorCount; ++index) {
            config.palette[index] = ansiDefaults[index];
        }
    }

    // The pending frame, forced out if everything was already consumed:
    // the walk and the cursor both read presentation state off it.
    static const TerminalUpdate* currentUpdate(shitty_vt* vt) {
        const TerminalUpdate* update = vt->terminal->output();
        if (update == nullptr) {
            vt->terminal->expose();
            update = vt->terminal->output();
        }
        return update;
    }

    static u32 packAttributes(const TerminalCell& cell) {
        u32 attributes = 0;
        attributes |= cell.bold ? SHITTY_VT_ATTR_BOLD : 0;
        attributes |= cell.faint ? SHITTY_VT_ATTR_FAINT : 0;
        attributes |= cell.italic ? SHITTY_VT_ATTR_ITALIC : 0;
        attributes |= cell.blink ? SHITTY_VT_ATTR_BLINK : 0;
        attributes |= cell.inverse ? SHITTY_VT_ATTR_INVERSE : 0;
        attributes |= cell.conceal ? SHITTY_VT_ATTR_CONCEAL : 0;
        attributes |= cell.strike ? SHITTY_VT_ATTR_STRIKE : 0;
        attributes |= cell.overline ? SHITTY_VT_ATTR_OVERLINE : 0;
        return attributes;
    }

    // One row of cells handed to an embedder's callback. The row number
    // is passed through rather than derived, so a caller reading the
    // history by index reports that index.
    static void emitRow(shitty_vt* vt, const TerminalColors* colors, const ScreenRowRef& source, u16 row, shitty_vt_cell_fn fn, void* user) {
        if (source.cells == nullptr) {
            return;
        }
        CellExtraStore* const extras = vt->extras.store;
        for (u16 column = 0; column < vt->geometry.columns; ++column) {
            const TerminalCell& cell = source.cells[column];
            if (cell.dwidth_cont) {
                continue;
            }
            shitty_vt_cell out{};
            u32 single = 0;
            if (cell.hasExtra()) {
                const GraphemeView grapheme = extras->grapheme(cell);
                out.grapheme = grapheme.data();
                out.grapheme_len = grapheme.count;
            } else if (cell.uc_pt != 0) {
                single = cell.uc_pt;
                out.grapheme = &single;
                out.grapheme_len = 1;
            }
            if (colors != nullptr) {
                out.foreground = colors->resolveForeground(cell).packed();
                out.background = colors->resolveBackground(cell).packed();
                out.underline_color = colors->resolve(extras->underlineColor(cell)).packed();
            }
            out.attributes = (u16)(packAttributes(cell));
            out.underline_style = (u8)(cell.underline_style);
            out.width = cell.dwidth ? 2 : 1;
            fn(user, row, column, &out);
        }
    }

}

shitty_vt* shitty_vt_new(uint16_t columns, uint16_t rows, uint16_t save_lines, const shitty_vt_callbacks* callbacks) {
    if (columns == 0 || rows == 0) {
        return nullptr;
    }
    try {
        ScopedPtr<ObjPool> pool{ObjPool::fromMemoryRaw()};
        shitty_vt* const vt = pool->make<shitty_vt>();
        vt->pool = pool.ptr;
        vt->platform = plt::createHeadlessPlatform(*pool.ptr);
        fillConfig(vt->config, save_lines);
        vt->slot.config = &vt->config;
        vt->geometry.setCellPixelSize(1, 1);
        vt->geometry.resize(columns, rows, nullptr);
        vt->extras.store = CellExtraStore::create(vt->extras, *pool.ptr, 0);
        vt->smallObjects = SmallObjAllocator::create(pool.ptr);
        vt->host = pool->make<EmbedHost>(*vt);
        vt->pty = pool->make<ReplyPty>(*vt->platform->scheduler());
        vt->terminal = Vterm::create(*pool.ptr, vt->geometry, vt->slot, vt->extras, *vt->smallObjects, *vt->platform->scheduler(), *vt->host, *vt->pty, nullptr);
        // A library terminal has nothing to lose focus to; applications
        // that ask for focus events learn of changes when the embedder
        // grows an input surface.
        vt->terminal->focus(true);
        // Armed last: construction publishes a reset title and a frame
        // request, and neither is the application speaking (issue 98).
        vt->callbacks = callbacks;
        pool.drop();
        return vt;
    } catch (...) {
        return nullptr;
    }
}

void shitty_vt_free(shitty_vt* vt) {
    if (vt == nullptr) {
        return;
    }
    delete vt->pool;
}

void shitty_vt_feed(shitty_vt* vt, const uint8_t* bytes, size_t len) {
    if (bytes == nullptr || len == 0) {
        return;
    }
    vt->terminal->feedPty(StringView((const u8*)(bytes), len));
}

void shitty_vt_resize(shitty_vt* vt, uint16_t columns, uint16_t rows) {
    if (columns == 0 || rows == 0) {
        return;
    }
    vt->geometry.resize(columns, rows, vt->host);
}

size_t shitty_vt_take_replies(shitty_vt* vt, uint8_t* out, size_t cap) {
    return vt->pty->take(out, cap);
}

void shitty_vt_each_cell(shitty_vt* vt, shitty_vt_cell_fn fn, void* user) {
    if (fn == nullptr) {
        return;
    }
    const TerminalUpdate* update = currentUpdate(vt);
    if (update == nullptr || update->shapes == nullptr) {
        return;
    }
    Screen* const screen = update->shapes;
    for (u16 row = 0; row < vt->geometry.rows; ++row) {
        emitRow(vt, update->colors, screen->viewRow(row), row, fn, user);
    }
    vt->terminal->consume();
}

uint32_t shitty_vt_scroll(shitty_vt* vt, int32_t rows) {
    return vt->terminal->scrollView(rows);
}

uint32_t shitty_vt_scroll_to(shitty_vt* vt, uint32_t offset) {
    return vt->terminal->scrollViewTo(offset);
}

uint32_t shitty_vt_scroll_offset(const shitty_vt* vt) {
    const TerminalUpdate* update = currentUpdate(const_cast<shitty_vt*>(vt));
    if (update == nullptr || update->shapes == nullptr) {
        return 0;
    }
    return update->shapes->info().viewOffset;
}

uint32_t shitty_vt_history_rows(const shitty_vt* vt) {
    const TerminalUpdate* update = currentUpdate(const_cast<shitty_vt*>(vt));
    if (update == nullptr || update->shapes == nullptr) {
        return 0;
    }
    return update->shapes->info().historyRows;
}

uint32_t shitty_vt_total_rows(const shitty_vt* vt) {
    const TerminalUpdate* update = currentUpdate(const_cast<shitty_vt*>(vt));
    if (update == nullptr || update->shapes == nullptr) {
        return 0;
    }
    const ScreenInfo info = update->shapes->info();
    return info.historyRows + info.rows;
}

void shitty_vt_row_cells(shitty_vt* vt, uint32_t index, shitty_vt_cell_fn fn, void* user) {
    if (fn == nullptr) {
        return;
    }
    const TerminalUpdate* update = currentUpdate(vt);
    if (update == nullptr || update->shapes == nullptr) {
        return;
    }
    Screen* const screen = update->shapes;
    const ScreenInfo info = screen->info();
    if (index >= info.historyRows + info.rows) {
        return;
    }
    // Logical row 0 is the top of the live screen and the history runs
    // negative from there, so an oldest-first index sits historyRows
    // above it. viewRow subtracts the current offset, so add it back and
    // the read is independent of where the user has scrolled.
    const i32 logical = (i32)(index) - (i32)(info.historyRows);
    const i32 view = logical + (i32)(info.viewOffset);
    emitRow(vt, update->colors, screen->viewRow(view), (u16)(index), fn, user);
    vt->terminal->consume();
}

void shitty_vt_memory_usage(const shitty_vt* vt, shitty_vt_memory* out) {
    if (out == nullptr) {
        return;
    }
    *out = shitty_vt_memory{};
    const TerminalUpdate* update = currentUpdate(const_cast<shitty_vt*>(vt));
    if (update == nullptr || update->shapes == nullptr) {
        return;
    }
    const ScreenInfo info = update->shapes->info();
    const u32 allocated = update->shapes->materializedRows();
    out->allocated_rows = allocated;
    out->capacity_rows = (u32)(info.rows) + info.saveLines;
    out->columns = info.columns;
    out->cell_size = (u32)(sizeof(TerminalCell));
    out->cell_bytes = (u64)(allocated)*info.columns * sizeof(TerminalCell);
}

void shitty_vt_set_save_lines(shitty_vt* vt, uint16_t save_lines) {
    if (vt->config.saveLines == save_lines) {
        return;
    }
    vt->config.saveLines = save_lines;
    // The terminal re-reads the configuration and rebuilds whatever the
    // change invalidated, which for this setting is the primary screen.
    vt->terminal->configChanged();
}

shitty_vt_cursor shitty_vt_cursor_state(const shitty_vt* vt) {
    shitty_vt_cursor result{};
    const TerminalUpdate* update = currentUpdate(const_cast<shitty_vt*>(vt));
    if (update == nullptr) {
        return result;
    }
    result.column = update->cursor.posX;
    result.row = update->cursor.posY;
    result.style = (u8)(update->cursor.style);
    result.visible = update->cursor.style != TerminalCursor::Style::hidden ? 1 : 0;
    return result;
}

uint32_t shitty_vt_modes(const shitty_vt* vt) {
    const VtermState state = vt->terminal->state();
    u32 modes = 0;
    modes |= state.alternateScreen ? SHITTY_VT_MODE_ALT_SCREEN : 0;
    modes |= state.bracketedPaste ? SHITTY_VT_MODE_BRACKETED_PASTE : 0;
    modes |= state.applicationCursorKeys ? SHITTY_VT_MODE_APP_CURSOR_KEYS : 0;
    modes |= state.applicationKeypad ? SHITTY_VT_MODE_APP_KEYPAD : 0;
    modes |= state.focusEvents ? SHITTY_VT_MODE_FOCUS_EVENTS : 0;
    modes |= state.autoWrap ? SHITTY_VT_MODE_AUTO_WRAP : 0;
    modes |= state.originMode ? SHITTY_VT_MODE_ORIGIN : 0;
    modes |= state.insertMode ? SHITTY_VT_MODE_INSERT : 0;
    modes |= state.showCursor ? SHITTY_VT_MODE_CURSOR_VISIBLE : 0;
    modes |= state.screenReverse ? SHITTY_VT_MODE_SCREEN_REVERSE : 0;
    modes |= state.synchronizedOutput ? SHITTY_VT_MODE_SYNCHRONIZED_OUTPUT : 0;
    modes |= state.mouseTracking != MouseTrackingMode::Disabled ? SHITTY_VT_MODE_MOUSE_CLICK : 0;
    modes |= state.mouseTracking == MouseTrackingMode::VT200_ButtonEvent ? SHITTY_VT_MODE_MOUSE_DRAG : 0;
    modes |= state.mouseTracking == MouseTrackingMode::VT200_AnyEvent ? SHITTY_VT_MODE_MOUSE_MOTION : 0;
    modes |= state.mouseEncoding == MouseTrackingEnc::SGR || state.mouseEncoding == MouseTrackingEnc::SGRPixels ? SHITTY_VT_MODE_MOUSE_SGR : 0;
    modes |= state.alternateScroll ? SHITTY_VT_MODE_ALTERNATE_SCROLL : 0;
    return modes;
}
