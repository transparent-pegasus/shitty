#include "platform_x11.h"

#include "drop.h"
#include "fiber.h"
#include "input.h"
#include "mutex.h"
#include "poller.h"
#include "window.h"
#include "platform.h"
#include "loop_wake.h"
#include "poller_loop.h"

#include <std/sys/crt.h>
#include <std/ios/input.h>
#include <std/sys/throw.h>
#include <std/alg/minmax.h>
#include <std/ios/output.h>
#include <std/lib/buffer.h>
#include <std/thr/poll_fd.h>
#include <std/thr/runable.h>
#include <std/mem/obj_pool.h>
#include <std/mem/small_obj_allocator.h>

#include <new>
#include <array>
#include <cerrno>
#include <string>
#include <vector>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <utility>
#include <unistd.h>
#include <algorithm>
#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>

using namespace stl;
using namespace plt;

extern char** environ;

namespace {
    struct PlatformImpl;
    struct WindowImpl;
    struct StreamInput;
    struct DndSession;

    constexpr u64 selectionTransferTimeoutUs = 30'000'000;
    constexpr size_t selectionReadChunkSize = 64 * 1024;
    constexpr size_t preferredSelectionWriteChunkSize = 64 * 1024;
    constexpr u32 xdndVersion = 5;
    constexpr size_t pointerIconCount = (size_t)(PointerIcon::DisappearingItem) + 1;

    struct Atoms {
        xcb_atom_t wmProtocols = XCB_ATOM_NONE;
        xcb_atom_t wmDeleteWindow = XCB_ATOM_NONE;
        xcb_atom_t wmTakeFocus = XCB_ATOM_NONE;
        xcb_atom_t wmState = XCB_ATOM_NONE;
        xcb_atom_t wmChangeState = XCB_ATOM_NONE;
        xcb_atom_t wmClass = XCB_ATOM_NONE;
        xcb_atom_t wmName = XCB_ATOM_NONE;
        xcb_atom_t wmNormalHints = XCB_ATOM_NONE;
        xcb_atom_t wmSizeHints = XCB_ATOM_NONE;
        xcb_atom_t motifWmHints = XCB_ATOM_NONE;
        xcb_atom_t utf8String = XCB_ATOM_NONE;
        xcb_atom_t netWmName = XCB_ATOM_NONE;
        xcb_atom_t netWmPid = XCB_ATOM_NONE;
        xcb_atom_t netWmState = XCB_ATOM_NONE;
        xcb_atom_t netWmStateMaximizedVert = XCB_ATOM_NONE;
        xcb_atom_t netWmStateMaximizedHorz = XCB_ATOM_NONE;
        xcb_atom_t netWmStateFullscreen = XCB_ATOM_NONE;
        xcb_atom_t netWmStateHidden = XCB_ATOM_NONE;
        xcb_atom_t netWmStateDemandsAttention = XCB_ATOM_NONE;
        xcb_atom_t netActiveWindow = XCB_ATOM_NONE;
        xcb_atom_t netCloseWindow = XCB_ATOM_NONE;
        xcb_atom_t netWmPing = XCB_ATOM_NONE;
        xcb_atom_t netFrameExtents = XCB_ATOM_NONE;
        xcb_atom_t clipboard = XCB_ATOM_NONE;
        xcb_atom_t targets = XCB_ATOM_NONE;
        xcb_atom_t incr = XCB_ATOM_NONE;
        xcb_atom_t timestamp = XCB_ATOM_NONE;
        xcb_atom_t multiple = XCB_ATOM_NONE;
        xcb_atom_t atomPair = XCB_ATOM_NONE;
        xcb_atom_t textUtf8 = XCB_ATOM_NONE;
        xcb_atom_t textPlain = XCB_ATOM_NONE;
        xcb_atom_t selectionData = XCB_ATOM_NONE;
        xcb_atom_t resourceManager = XCB_ATOM_NONE;
        xcb_atom_t xkbRulesNames = XCB_ATOM_NONE;
        xcb_atom_t xdndAware = XCB_ATOM_NONE;
        xcb_atom_t xdndEnter = XCB_ATOM_NONE;
        xcb_atom_t xdndPosition = XCB_ATOM_NONE;
        xcb_atom_t xdndStatus = XCB_ATOM_NONE;
        xcb_atom_t xdndTypeList = XCB_ATOM_NONE;
        xcb_atom_t xdndActionCopy = XCB_ATOM_NONE;
        xcb_atom_t xdndActionMove = XCB_ATOM_NONE;
        xcb_atom_t xdndDrop = XCB_ATOM_NONE;
        xcb_atom_t xdndLeave = XCB_ATOM_NONE;
        xcb_atom_t xdndFinished = XCB_ATOM_NONE;
        xcb_atom_t xdndSelection = XCB_ATOM_NONE;
    };

    struct TaskBlock {
        TaskBlock* next = nullptr;
        alignas(16) u8 stack[64 * 1024];
    };

    template <typename F>
    struct FiberTask;

    struct SelectionState {
        Buffer content;
        xcb_atom_t atom = XCB_ATOM_NONE;
        xcb_timestamp_t timestamp = XCB_CURRENT_TIME;
        bool owned = false;
    };

    struct OutgoingSelection final: public TimerCallback {
        explicit OutgoingSelection(PlatformImpl* platform);

        void ready() override;

        PlatformImpl* platform;
        xcb_window_t requestor = XCB_WINDOW_NONE;
        xcb_atom_t property = XCB_ATOM_NONE;
        xcb_atom_t target = XCB_ATOM_NONE;
        Buffer content;
        size_t offset = 0;
        u32 originalEventMask = 0;
    };

    struct ClipboardImpl final: public Clipboard {
        Input* read() override;
        Output* write() override;

        PlatformImpl* platform = nullptr;
        bool primary = false;
    };

    struct StreamOutput final: public Output {
        StreamOutput(PlatformImpl& platform, bool primary);

        void operator delete(StreamOutput* output, std::destroying_delete_t) noexcept;

        size_t writeImpl(const void* data, size_t size) override;
        void finishImpl() override;

        PlatformImpl& platform;
        Buffer accumulated;
        bool primary;
        bool finished = false;
    };

    struct StreamInput final: public Input {
        StreamInput(PlatformImpl& platform, Buffer&& local);
        StreamInput(PlatformImpl& platform, xcb_atom_t selection, const std::vector<xcb_atom_t>& targets, bool* drained = nullptr, xcb_timestamp_t time = XCB_CURRENT_TIME, StreamInput** activeSlot = nullptr);
        ~StreamInput() noexcept override;

        void operator delete(StreamInput* input, std::destroying_delete_t) noexcept;

        size_t readImpl(void* data, size_t len) override;
        void selectionNotify(const xcb_selection_notify_event_t& event);
        void propertyNotify(const xcb_property_notify_event_t& event);
        void requestNextTarget();
        void readProperty();
        void wake();
        void failTransfer();

        PlatformImpl& platform;
        Buffer buffered;
        size_t offset = 0;
        std::vector<xcb_atom_t> targets;
        size_t targetIndex = 0;
        xcb_window_t requestor = XCB_WINDOW_NONE;
        xcb_atom_t selection = XCB_ATOM_NONE;
        xcb_atom_t target = XCB_ATOM_NONE;
        xcb_timestamp_t time = XCB_CURRENT_TIME;
        Fiber* waiter = nullptr;
        bool* drained = nullptr;
        StreamInput** activeSlot = nullptr;
        bool incremental = false;
        bool propertyPending = false;
        bool complete = false;
        bool success = false;
    };

    struct DndOfferView final: public DropOffer {
        size_t formats() const override;
        StringView format(size_t index) const override;

        const std::vector<std::string>* names = nullptr;
    };

    struct DndDrop final: public Drop {
        DropOffer* what() override;
        Input* read(StringView mime) override;

        PlatformImpl* platform = nullptr;
        DndSession* session = nullptr;
        DndOfferView* offer = nullptr;
        const std::vector<xcb_atom_t>* atoms = nullptr;
        bool started = false;
        bool drained = false;
        xcb_timestamp_t time = XCB_CURRENT_TIME;
    };

    struct DndSession {
        WindowImpl* window = nullptr;
        WindowImpl* transferWindow = nullptr;
        Fiber* fiber = nullptr;
        StreamInput* input = nullptr;
        xcb_window_t target = XCB_WINDOW_NONE;
        xcb_window_t source = XCB_WINDOW_NONE;
        u32 version = 0;
        std::vector<xcb_atom_t> types;
        std::vector<std::string> names;
        xcb_timestamp_t time = XCB_CURRENT_TIME;
        xcb_atom_t proposedAction = XCB_ATOM_NONE;
        i32 rootX = 0;
        i32 rootY = 0;
        bool motionPending = false;
        bool dropPending = false;
        bool leavePending = false;
        bool accepted = false;
        xcb_atom_t acceptedType = XCB_ATOM_NONE;
        DropAction acceptedAction = DropAction::None;
    };

    struct FrameTimer final: public TimerCallback {
        explicit FrameTimer(WindowImpl* window);

        void ready() override;

        WindowImpl* window;
    };

    struct ResizeTimer final: public TimerCallback {
        explicit ResizeTimer(WindowImpl* window);

        void ready() override;

        WindowImpl* window;
    };

    struct WindowImpl final: public Window {
        WindowImpl(PlatformImpl& platform, const WindowOptions& options);
        ~WindowImpl() noexcept;

        void requestShow() override;
        void requestClose() override;
        void requestFrame() override;
        void requestTitle(StringView title) override;
        void requestAttention() override;
        void requestRestore() override;
        void requestIconify() override;
        void requestMove(i32 x, i32 y) override;
        void requestFocus() override;
        void requestMaximized(bool maximized) override;
        void requestFullscreen(bool fullscreen) override;
        void requestResize(u32 width, u32 height) override;
        void requestMinimumSize(u32 width, u32 height) override;
        void requestResizeUnit(u32 width, u32 height, u32 baseWidth, u32 baseHeight) override;
        Clipboard* primary() override;
        Clipboard* secondary() override;
        void requestPointerIcon(PointerIcon icon) override;
        void requestOpenUri(StringView uri) override;
        void requestTextInputRect(i32 x, i32 y, u32 width, u32 height) override;
        WindowInfo info() const override;
        bool inLiveResize() const override;
        RenderContext renderContext() const override;

        void dispatchFrame();
        void resizeEnded();
        void configure(const xcb_configure_notify_event_t& event);
        void updateState();
        void updateInitialState();
        void updateInitialHints();
        void setFocused(bool focused);
        void setIconified(bool iconified);
        void updateNormalHints();
        void updateCursor();
        void sendState(u32 action, xcb_atom_t first, xcb_atom_t second = XCB_ATOM_NONE);
        void sendRootMessage(xcb_atom_t type, const std::array<u32, 5>& data);
        void xdndEnter(const xcb_client_message_event_t& event);
        void xdndPosition(const xcb_client_message_event_t& event);
        void xdndDrop(const xcb_client_message_event_t& event);
        void xdndLeave(const xcb_client_message_event_t& event);

        PlatformImpl& platform;
        xcb_window_t window = XCB_WINDOW_NONE;
        InputSink* input = nullptr;
        WindowEvents* events = nullptr;
        FrameCallback* frame = nullptr;
        DropTarget* dropTarget = nullptr;
        ClipboardImpl primarySelection;
        ClipboardImpl clipboardSelection;
        FrameTimer frameTimer{this};
        ResizeTimer resizeTimer{this};
        WindowInfo info_;
        u32 minimumWidth = 1;
        u32 minimumHeight = 1;
        u32 resizeUnitWidth = 1;
        u32 resizeUnitHeight = 1;
        u32 resizeBaseWidth = 0;
        u32 resizeBaseHeight = 0;
        i32 pointerX = 0;
        i32 pointerY = 0;
        i32 textInputX = 0;
        i32 textInputY = 0;
        u32 textInputWidth = 0;
        u32 textInputHeight = 0;
        PointerIcon pointerIcon = PointerIcon::Text;
        bool shown = false;
        bool closeRequested = false;
        bool frameRequested = false;
        bool frameScheduled = false;
        bool liveResize = false;
        bool requestedAttention = false;
        bool requestedMaximized = false;
        bool requestedFullscreen = false;
        bool requestedIconified = false;
        bool focusOnShow = false;
        u32 frameRetries = 0;
    };

    struct PlatformImpl final: public Platform, public PollCallback {
        explicit PlatformImpl(ObjPool& owner);
        ~PlatformImpl() noexcept;

        Window* createWindow(ObjPool& owner, const WindowOptions& options) override;
        LoopWake* createLoopWake(ObjPool& owner, TimerCallback& callback) override;
        Poller* poller() override;
        Scheduler* scheduler() override;
        void run() override;
        void stop() override;
        void ready(PollFD event) override;

        void dispatchEvents();
        void dispatchEvent(xcb_generic_event_t* event);
        void flush();
        void changeProperty8(xcb_window_t window, xcb_atom_t property, xcb_atom_t type, const void* data, size_t length);
        void arm();
        WindowImpl* findWindow(xcb_window_t window) const;
        StreamInput* findInput(xcb_window_t requestor) const;
        void removeWindow(WindowImpl* window);
        void removeInput(StreamInput* input);
        void rebuildKeymap();
        void syncKeyboard();
        void syncModifiers(u16 coreState);
        void keyboard(WindowImpl& window, const xcb_key_press_event_t& event, bool pressed, bool repeated = false);
        void releasePressedKeys(WindowImpl* window);
        u16 modifiers() const;
        InputKey inputKey(xkb_keysym_t symbol) const;
        u32 keymapCodepoint(xkb_keycode_t key, xkb_layout_index_t layout, xkb_level_index_t level = 0) const;
        size_t composeFeed(xkb_keysym_t symbol, u32 codepoint, u32* codepoints, size_t capacity);
        void setSelection(bool primary, StringView content);
        xcb_timestamp_t serverTimestamp();
        SelectionState& selection(bool primary);
        SelectionState* selection(xcb_atom_t atom);
        void selectionRequest(const xcb_selection_request_event_t& event);
        bool writeSelectionProperty(xcb_window_t requestor, xcb_atom_t property, xcb_atom_t target, const SelectionState& state);
        bool writeSelectionTarget(xcb_window_t requestor, xcb_atom_t property, xcb_atom_t target, const SelectionState& state, bool allowIncr);
        void outgoingProperty(const xcb_property_notify_event_t& event);
        void cancelOutgoing(xcb_window_t requestor);
        void removeOutgoing(OutgoingSelection* transfer);
        StreamInput* createSelectionInput(xcb_atom_t selection, const std::vector<xcb_atom_t>& targets, bool* drained = nullptr, xcb_timestamp_t time = XCB_CURRENT_TIME, StreamInput** activeSlot = nullptr);
        std::string atomName(xcb_atom_t atom);
        float contentScale() const;
        xcb_cursor_t cursor(PointerIcon icon);
        void beginDnd(WindowImpl& window, const xcb_client_message_event_t& event);
        void runDndSession(DndSession& session);
        void sendDndStatus(DndSession& session);
        void sendDndFinished(DndSession& session, bool success);
        void endDnd(DndSession& session, bool notifyLeft);
        void cancelDropTransfers(WindowImpl* window);

        template <typename F>
        void spawnTask(F body) {
            TaskBlock* const block = takeTaskBlock();
            scheduler_->spawn(*allocator_->make<FiberTask<F>>(*this, block, static_cast<F&&>(body)), block->stack, sizeof(block->stack));
        }

        TaskBlock* takeTaskBlock();
        void recycleTaskBlock(TaskBlock* block);

        ObjPool* owner_ = nullptr;
        PollerLoop* poller_ = nullptr;
        Scheduler* scheduler_ = nullptr;
        FiberMutex* selectionMutex_ = nullptr;
        SmallObjAllocator* allocator_ = nullptr;
        PollWaiter connectionWaiter_;
        xcb_connection_t* connection = nullptr;
        xcb_screen_t* screen = nullptr;
        xcb_window_t selectionWindow = XCB_WINDOW_NONE;
        xcb_font_t cursorFont = XCB_NONE;
        Atoms atoms;
        SelectionState primaryState;
        SelectionState clipboardState;
        std::vector<WindowImpl*> windows;
        std::vector<StreamInput*> inputs;
        std::vector<OutgoingSelection*> outgoing;
        std::array<xcb_cursor_t, pointerIconCount> cursors{};
        xkb_context* xkbContext = nullptr;
        xkb_keymap* keymap = nullptr;
        xkb_state* xkbState = nullptr;
        xkb_compose_table* composeTable = nullptr;
        xkb_compose_state* composeState = nullptr;
        std::array<bool, 256> pressedKeys{};
        std::array<bool, 256> enteredKeys{};
        WindowImpl* keyboardFocus = nullptr;
        DndSession* dndSession = nullptr;
        std::vector<DndSession*> dropTransfers;
        TaskBlock* taskBlocks_ = nullptr;
        xcb_timestamp_t lastTimestamp = XCB_CURRENT_TIME;
        xcb_timestamp_t timestampResult = XCB_CURRENT_TIME;
        Fiber* timestampWaiter = nullptr;
        u32 timestampSerial = 0;
        size_t selectionWriteChunkSize = 0;
        float contentScale_ = 1.0f;
        bool stopped = false;
    };

    template <typename F>
    struct FiberTask final: public Runable {
        FiberTask(PlatformImpl& platform_, TaskBlock* block_, F body_)
            : platform(platform_)
            , block(block_)
            , body(static_cast<F&&>(body_)) {
        }

        void run() override {
            body();
            PlatformImpl& owner = platform;
            TaskBlock* const spent = block;
            owner.allocator_->release(this);
            owner.recycleTaskBlock(spent);
        }

        PlatformImpl& platform;
        TaskBlock* block;
        F body;
    };
}

namespace {
    struct AtomBinding {
        xcb_atom_t Atoms::* member;
        const char* name;
    };

    constexpr AtomBinding atomBindings[] = {
        {&Atoms::wmProtocols, "WM_PROTOCOLS"},
        {&Atoms::wmDeleteWindow, "WM_DELETE_WINDOW"},
        {&Atoms::wmTakeFocus, "WM_TAKE_FOCUS"},
        {&Atoms::wmState, "WM_STATE"},
        {&Atoms::wmChangeState, "WM_CHANGE_STATE"},
        {&Atoms::wmClass, "WM_CLASS"},
        {&Atoms::wmName, "WM_NAME"},
        {&Atoms::wmNormalHints, "WM_NORMAL_HINTS"},
        {&Atoms::wmSizeHints, "WM_SIZE_HINTS"},
        {&Atoms::motifWmHints, "_MOTIF_WM_HINTS"},
        {&Atoms::utf8String, "UTF8_STRING"},
        {&Atoms::netWmName, "_NET_WM_NAME"},
        {&Atoms::netWmPid, "_NET_WM_PID"},
        {&Atoms::netWmState, "_NET_WM_STATE"},
        {&Atoms::netWmStateMaximizedVert, "_NET_WM_STATE_MAXIMIZED_VERT"},
        {&Atoms::netWmStateMaximizedHorz, "_NET_WM_STATE_MAXIMIZED_HORZ"},
        {&Atoms::netWmStateFullscreen, "_NET_WM_STATE_FULLSCREEN"},
        {&Atoms::netWmStateHidden, "_NET_WM_STATE_HIDDEN"},
        {&Atoms::netWmStateDemandsAttention, "_NET_WM_STATE_DEMANDS_ATTENTION"},
        {&Atoms::netActiveWindow, "_NET_ACTIVE_WINDOW"},
        {&Atoms::netCloseWindow, "_NET_CLOSE_WINDOW"},
        {&Atoms::netWmPing, "_NET_WM_PING"},
        {&Atoms::netFrameExtents, "_NET_FRAME_EXTENTS"},
        {&Atoms::clipboard, "CLIPBOARD"},
        {&Atoms::targets, "TARGETS"},
        {&Atoms::incr, "INCR"},
        {&Atoms::timestamp, "TIMESTAMP"},
        {&Atoms::multiple, "MULTIPLE"},
        {&Atoms::atomPair, "ATOM_PAIR"},
        {&Atoms::textUtf8, "text/plain;charset=utf-8"},
        {&Atoms::textPlain, "text/plain"},
        {&Atoms::selectionData, "_PLT_SELECTION_DATA"},
        {&Atoms::resourceManager, "RESOURCE_MANAGER"},
        {&Atoms::xkbRulesNames, "_XKB_RULES_NAMES"},
        {&Atoms::xdndAware, "XdndAware"},
        {&Atoms::xdndEnter, "XdndEnter"},
        {&Atoms::xdndPosition, "XdndPosition"},
        {&Atoms::xdndStatus, "XdndStatus"},
        {&Atoms::xdndTypeList, "XdndTypeList"},
        {&Atoms::xdndActionCopy, "XdndActionCopy"},
        {&Atoms::xdndActionMove, "XdndActionMove"},
        {&Atoms::xdndDrop, "XdndDrop"},
        {&Atoms::xdndLeave, "XdndLeave"},
        {&Atoms::xdndFinished, "XdndFinished"},
        {&Atoms::xdndSelection, "XdndSelection"},
    };

    [[noreturn]]
    void fail(StringView message) {
        Errno(errno == 0 ? EINVAL : errno).raise(message);
    }

    u32 decodeUtf8One(const u8* bytes, size_t length, size_t* consumed) {
        *consumed = 1;
        if (bytes[0] < 0x80) {
            return bytes[0];
        }
        size_t count;
        u32 value;
        if ((bytes[0] & 0xe0) == 0xc0) {
            count = 2;
            value = bytes[0] & 0x1f;
        } else if ((bytes[0] & 0xf0) == 0xe0) {
            count = 3;
            value = bytes[0] & 0x0f;
        } else if ((bytes[0] & 0xf8) == 0xf0) {
            count = 4;
            value = bytes[0] & 0x07;
        } else {
            return 0;
        }
        if (length < count) {
            return 0;
        }
        for (size_t index = 1; index != count; ++index) {
            if ((bytes[index] & 0xc0) != 0x80) {
                return 0;
            }
            value = (value << 6) | (bytes[index] & 0x3f);
        }
        if ((count == 2 && value < 0x80) || (count == 3 && value < 0x800) || (count == 4 && value < 0x10000) || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
            return 0;
        }
        *consumed = count;
        return value;
    }

    size_t decodeUtf8(const u8* bytes, size_t length, u32* codepoints, size_t capacity) {
        size_t offset = 0;
        size_t count = 0;
        while (offset != length && count != capacity) {
            size_t consumed;
            const u32 value = decodeUtf8One(bytes + offset, length - offset, &consumed);
            offset += consumed;
            if (value != 0) {
                codepoints[count++] = value;
            }
        }
        return count;
    }

    Buffer utf8ToLatin1(StringView content) {
        Buffer result;
        size_t offset = 0;
        while (offset != content.length()) {
            size_t consumed;
            const u8 first = content.data()[offset];
            const u32 codepoint = decodeUtf8One(content.data() + offset, content.length() - offset, &consumed);
            offset += consumed;
            const u8 value = codepoint <= 0xff && (codepoint != 0 || first == 0) ? (u8)(codepoint) : (u8)('?');
            result.append(&value, 1);
        }
        return result;
    }

    void appendLatin1AsUtf8(Buffer& output, const u8* content, size_t length) {
        for (size_t index = 0; index != length; ++index) {
            const u8 value = content[index];
            if (value < 0x80) {
                output.append(&value, 1);
            } else {
                const u8 encoded[] = {
                    (u8)(0xc0 | (value >> 6)),
                    (u8)(0x80 | (value & 0x3f)),
                };
                output.append(encoded, sizeof(encoded));
            }
        }
    }

    bool sameString(StringView value, const std::string& text) {
        return value.length() == text.size() && (value.empty() || memcmp(value.data(), text.data(), value.length()) == 0);
    }

    xcb_timestamp_t eventTime(const xcb_generic_event_t* event) {
        switch (event->response_type & 0x7f) {
            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE:
                return ((const xcb_key_press_event_t*)(event))->time;
            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE:
                return ((const xcb_button_press_event_t*)(event))->time;
            case XCB_MOTION_NOTIFY:
                return ((const xcb_motion_notify_event_t*)(event))->time;
            case XCB_ENTER_NOTIFY:
            case XCB_LEAVE_NOTIFY:
                return ((const xcb_enter_notify_event_t*)(event))->time;
            case XCB_PROPERTY_NOTIFY:
                return ((const xcb_property_notify_event_t*)(event))->time;
            case XCB_SELECTION_CLEAR:
                return ((const xcb_selection_clear_event_t*)(event))->time;
            case XCB_SELECTION_REQUEST:
                return ((const xcb_selection_request_event_t*)(event))->time;
            case XCB_SELECTION_NOTIFY:
                return ((const xcb_selection_notify_event_t*)(event))->time;
            default:
                return XCB_CURRENT_TIME;
        }
    }
}

PlatformImpl::PlatformImpl(ObjPool& owner)
    : owner_(&owner)
    , poller_(PollerLoop::create(owner))
    , scheduler_(Scheduler::create(owner, *poller_))
    , allocator_(SmallObjAllocator::create(&owner)) {
    int screenNumber = 0;
    connection = xcb_connect(nullptr, &screenNumber);
    if (connection == nullptr || xcb_connection_has_error(connection) != 0) {
        fail(u8"xcb_connect failed");
    }
    const xcb_setup_t* const setup = xcb_get_setup(connection);
    xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(setup);
    for (int index = 0; index != screenNumber && iterator.rem != 0; ++index) {
        xcb_screen_next(&iterator);
    }
    if (iterator.rem == 0 || iterator.data == nullptr) {
        fail(u8"X11 default screen is unavailable");
    }
    screen = iterator.data;

    // ChangeProperty includes its fixed request and four-byte padding in the
    // server's advertised request limit.  Keep a little extra headroom for
    // BIG-REQUESTS' extended length word and unusual XCB implementations.
    const u64 maximumRequestBytes = (u64)(xcb_get_maximum_request_length(connection)) * 4;
    constexpr size_t requestHeadroom = sizeof(xcb_change_property_request_t) + 16;
    if (maximumRequestBytes <= requestHeadroom) {
        fail(u8"X11 maximum request length is too small");
    }
    const u64 usableRequestBytes = std::min((u64)(preferredSelectionWriteChunkSize), maximumRequestBytes - requestHeadroom);
    selectionWriteChunkSize = (size_t)(usableRequestBytes & ~(u64)(3));
    if (selectionWriteChunkSize < sizeof(xcb_atom_t)) {
        fail(u8"X11 maximum property payload is too small");
    }

    std::array<xcb_intern_atom_cookie_t, sizeof(atomBindings) / sizeof(atomBindings[0])> cookies;
    for (size_t index = 0; index != cookies.size(); ++index) {
        cookies[index] = xcb_intern_atom(connection, 0, (u16)(strlen(atomBindings[index].name)), atomBindings[index].name);
    }
    for (size_t index = 0; index != cookies.size(); ++index) {
        xcb_intern_atom_reply_t* const reply = xcb_intern_atom_reply(connection, cookies[index], nullptr);
        if (reply == nullptr) {
            fail(u8"X11 atom initialization failed");
        }
        atoms.*(atomBindings[index].member) = reply->atom;
        free(reply);
    }

    primaryState.atom = XCB_ATOM_PRIMARY;
    clipboardState.atom = atoms.clipboard;
    const u32 rootMask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(connection, screen->root, XCB_CW_EVENT_MASK, &rootMask);

    selectionWindow = xcb_generate_id(connection);
    const u32 selectionMask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, selectionWindow, screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, &selectionMask);

    xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (xkbContext == nullptr) {
        fail(u8"xkb_context_new failed");
    }
    rebuildKeymap();
    const char* locale = getenv("LC_ALL");
    if (locale == nullptr || *locale == 0) {
        locale = getenv("LC_CTYPE");
    }
    if (locale == nullptr || *locale == 0) {
        locale = getenv("LANG");
    }
    if (locale == nullptr || *locale == 0) {
        locale = "C";
    }
    composeTable = xkb_compose_table_new_from_locale(xkbContext, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (composeTable != nullptr) {
        composeState = xkb_compose_state_new(composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
    }
    selectionMutex_ = scheduler_->createMutex(owner);

    xcb_get_property_cookie_t resourceCookie = xcb_get_property(connection, 0, screen->root, atoms.resourceManager, XCB_GET_PROPERTY_TYPE_ANY, 0, 256 * 1024 / 4);
    xcb_get_property_reply_t* const resourceReply = xcb_get_property_reply(connection, resourceCookie, nullptr);
    if (resourceReply != nullptr) {
        const char* const data = (const char*)(xcb_get_property_value(resourceReply));
        const size_t length = (size_t)(xcb_get_property_value_length(resourceReply));
        const char needle[] = "Xft.dpi:";
        for (size_t offset = 0; offset + sizeof(needle) - 1 < length; ++offset) {
            if (memcmp(data + offset, needle, sizeof(needle) - 1) != 0) {
                continue;
            }
            const size_t valueOffset = offset + sizeof(needle) - 1;
            size_t valueLength = 0;
            while (valueOffset + valueLength != length && valueLength != 63 && data[valueOffset + valueLength] != '\n' && data[valueOffset + valueLength] != '\r') {
                ++valueLength;
            }
            char value[64];
            memcpy(value, data + valueOffset, valueLength);
            value[valueLength] = 0;
            char* end = nullptr;
            const double dpi = strtod(value, &end);
            if (end != value && dpi >= 48.0 && dpi <= 768.0) {
                contentScale_ = (float)(dpi / 96.0);
            }
            break;
        }
        free(resourceReply);
    }
    flush();
}

PlatformImpl::~PlatformImpl() noexcept {
    poller_->cancel(connectionWaiter_);
    if (dndSession != nullptr) {
        dndSession->window = nullptr;
        dndSession->leavePending = true;
        dndSession->fiber->wake();
    }
    // Pool teardown may already have released the runtime half of an
    // OwnedFiber while leaving its raw Fiber* in one of these wait slots.
    // Abandon final-teardown waits without resuming their stacks.
    timestampWaiter = nullptr;
    timestampResult = lastTimestamp;
    while (!inputs.empty()) {
        StreamInput* const input = inputs.back();
        input->complete = true;
        input->success = false;
        input->waiter = nullptr;
        // These point into the abandoned caller/drop fiber's stack.  Final
        // teardown must reclaim the stream without writing through them.
        input->drained = nullptr;
        input->activeSlot = nullptr;
        allocator_->release(input);
    }
    while (!outgoing.empty()) {
        removeOutgoing(outgoing.back());
    }
    if (connection != nullptr) {
        for (xcb_cursor_t handle : cursors) {
            if (handle != XCB_NONE) {
                xcb_free_cursor(connection, handle);
            }
        }
        if (cursorFont != XCB_NONE) {
            xcb_close_font(connection, cursorFont);
        }
        if (selectionWindow != XCB_WINDOW_NONE) {
            xcb_destroy_window(connection, selectionWindow);
        }
        xcb_flush(connection);
    }
    if (composeState != nullptr) {
        xkb_compose_state_unref(composeState);
    }
    if (composeTable != nullptr) {
        xkb_compose_table_unref(composeTable);
    }
    if (xkbState != nullptr) {
        xkb_state_unref(xkbState);
    }
    if (keymap != nullptr) {
        xkb_keymap_unref(keymap);
    }
    if (xkbContext != nullptr) {
        xkb_context_unref(xkbContext);
    }
    if (connection != nullptr) {
        xcb_disconnect(connection);
    }
}

Window* PlatformImpl::createWindow(ObjPool& owner, const WindowOptions& options) {
    return owner.make<WindowImpl>(*this, options);
}

LoopWake* PlatformImpl::createLoopWake(ObjPool& owner, TimerCallback& callback) {
    return LoopWake::create(owner, *poller_, callback);
}

Poller* PlatformImpl::poller() {
    return poller_;
}

Scheduler* PlatformImpl::scheduler() {
    return scheduler_;
}

void PlatformImpl::arm() {
    connectionWaiter_.fd = {
        .fd = xcb_get_file_descriptor(connection),
        .flags = PollFlag::In,
    };
    connectionWaiter_.callback = this;
    poller_->arm(connectionWaiter_);
}

void PlatformImpl::flush() {
    if (xcb_flush(connection) <= 0 && xcb_connection_has_error(connection) != 0) {
        stop();
        return;
    }
    arm();
}

void PlatformImpl::changeProperty8(xcb_window_t window, xcb_atom_t property, xcb_atom_t type, const void* data, size_t length) {
    const u8* const bytes = (const u8*)(data);
    size_t offset = 0;
    do {
        const size_t count = std::min(selectionWriteChunkSize, length - offset);
        xcb_change_property(connection, offset == 0 ? XCB_PROP_MODE_REPLACE : XCB_PROP_MODE_APPEND, window, property, type, 8, (u32)(count), count == 0 ? nullptr : bytes + offset);
        offset += count;
    } while (offset != length);
}

void PlatformImpl::ready(PollFD event) {
    if (event.flags & (PollFlag::Err | PollFlag::Hup) || !(event.flags & PollFlag::In)) {
        stop();
        return;
    }
    dispatchEvents();
    if (xcb_connection_has_error(connection) != 0) {
        stop();
        return;
    }
    flush();
}

void PlatformImpl::run() {
    while (!stopped) {
        dispatchEvents();
        poller_->dispatchTimers();
        if (stopped) {
            break;
        }
        flush();
        if (stopped) {
            break;
        }
        poller_->wait(poller_->nextDeadline());
    }
    stopped = false;
}

void PlatformImpl::stop() {
    stopped = true;
}

TaskBlock* PlatformImpl::takeTaskBlock() {
    TaskBlock* block = taskBlocks_;
    if (block != nullptr) {
        taskBlocks_ = block->next;
        block->next = nullptr;
        return block;
    }
    return owner_->make<TaskBlock>();
}

void PlatformImpl::recycleTaskBlock(TaskBlock* block) {
    block->next = taskBlocks_;
    taskBlocks_ = block;
}

WindowImpl* PlatformImpl::findWindow(xcb_window_t handle) const {
    for (WindowImpl* window : windows) {
        if (window->window == handle) {
            return window;
        }
    }
    return nullptr;
}

StreamInput* PlatformImpl::findInput(xcb_window_t requestor) const {
    for (StreamInput* input : inputs) {
        if (input->requestor == requestor) {
            return input;
        }
    }
    return nullptr;
}

void PlatformImpl::removeWindow(WindowImpl* window) {
    const auto found = std::find(windows.begin(), windows.end(), window);
    if (found != windows.end()) {
        windows.erase(found);
    }
}

void PlatformImpl::removeInput(StreamInput* input) {
    const auto found = std::find(inputs.begin(), inputs.end(), input);
    if (found != inputs.end()) {
        inputs.erase(found);
    }
}

void PlatformImpl::dispatchEvents() {
    xcb_key_release_event_t pendingRelease{};
    bool havePendingRelease = false;
    for (;;) {
        xcb_generic_event_t* const event = xcb_poll_for_event(connection);
        if (event == nullptr) {
            break;
        }
        if (havePendingRelease) {
            const u8 type = event->response_type & 0x7f;
            const xcb_key_press_event_t* const press = type == XCB_KEY_PRESS ? (const xcb_key_press_event_t*)(event) : nullptr;
            if (press != nullptr && press->event == pendingRelease.event && press->detail == pendingRelease.detail && press->time == pendingRelease.time) {
                lastTimestamp = press->time;
                WindowImpl* const window = findWindow(press->event);
                if (window != nullptr) {
                    keyboard(*window, *press, true, true);
                }
                havePendingRelease = false;
                free(event);
                continue;
            }
            dispatchEvent((xcb_generic_event_t*)(&pendingRelease));
            havePendingRelease = false;
        }
        if ((event->response_type & 0x7f) == XCB_KEY_RELEASE) {
            pendingRelease = *(const xcb_key_release_event_t*)(event);
            havePendingRelease = true;
            free(event);
            continue;
        }
        dispatchEvent(event);
        free(event);
    }
    if (havePendingRelease) {
        dispatchEvent((xcb_generic_event_t*)(&pendingRelease));
    }
}

void PlatformImpl::dispatchEvent(xcb_generic_event_t* generic) {
    const u8 type = generic->response_type & 0x7f;
    const xcb_timestamp_t timestamp = eventTime(generic);
    if (timestamp != XCB_CURRENT_TIME) {
        lastTimestamp = timestamp;
    }
    switch (type) {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE: {
            const xcb_key_press_event_t& event = *(const xcb_key_press_event_t*)(generic);
            WindowImpl* const window = findWindow(event.event);
            if (window != nullptr) {
                keyboard(*window, event, type == XCB_KEY_PRESS);
            }
            break;
        }
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE: {
            const xcb_button_press_event_t& event = *(const xcb_button_press_event_t*)(generic);
            WindowImpl* const window = findWindow(event.event);
            if (window == nullptr || window->input == nullptr) {
                break;
            }
            syncModifiers(event.state);
            window->pointerX = event.event_x;
            window->pointerY = event.event_y;
            const bool pressed = type == XCB_BUTTON_PRESS;
            if (event.detail >= 4 && event.detail <= 7) {
                if (pressed) {
                    double x = 0;
                    double y = 0;
                    if (event.detail == 4) {
                        y = 1;
                    } else if (event.detail == 5) {
                        y = -1;
                    } else if (event.detail == 6) {
                        x = -1;
                    } else {
                        x = 1;
                    }
                    window->input->scroll({
                        .x = x,
                        .y = y,
                        .pixelX = window->pointerX,
                        .pixelY = window->pointerY,
                        .modifiers = modifiers(),
                        .time = event.time / 1000.0,
                    });
                    window->input->flush();
                }
                break;
            }
            PointerButton button;
            switch (event.detail) {
                case 1:
                    button = PointerButton::Primary;
                    break;
                case 2:
                    button = PointerButton::Middle;
                    break;
                case 3:
                    button = PointerButton::Secondary;
                    break;
                case 8:
                    button = PointerButton::Auxiliary1;
                    break;
                case 9:
                    button = PointerButton::Auxiliary2;
                    break;
                case 10:
                    button = PointerButton::Auxiliary3;
                    break;
                case 11:
                    button = PointerButton::Auxiliary4;
                    break;
                case 12:
                    button = PointerButton::Auxiliary5;
                    break;
                default:
                    break;
            }
            if ((event.detail >= 1 && event.detail <= 3) || (event.detail >= 8 && event.detail <= 12)) {
                window->input->pointerButton({
                    .button = button,
                    .pressed = pressed,
                    .pixelX = window->pointerX,
                    .pixelY = window->pointerY,
                    .modifiers = modifiers(),
                    .time = event.time / 1000.0,
                });
                window->input->flush();
            }
            break;
        }
        case XCB_MOTION_NOTIFY: {
            const xcb_motion_notify_event_t& event = *(const xcb_motion_notify_event_t*)(generic);
            WindowImpl* const window = findWindow(event.event);
            if (window != nullptr) {
                syncModifiers(event.state);
                window->pointerX = event.event_x;
                window->pointerY = event.event_y;
                if (window->input != nullptr) {
                    window->input->pointerMotion({
                        .pixelX = window->pointerX,
                        .pixelY = window->pointerY,
                        .modifiers = modifiers(),
                    });
                    window->input->flush();
                }
            }
            break;
        }
        case XCB_ENTER_NOTIFY:
        case XCB_LEAVE_NOTIFY: {
            const xcb_enter_notify_event_t& event = *(const xcb_enter_notify_event_t*)(generic);
            WindowImpl* const window = findWindow(event.event);
            if (window != nullptr && window->input != nullptr && event.mode == XCB_NOTIFY_MODE_NORMAL) {
                syncModifiers(event.state);
                if (type == XCB_ENTER_NOTIFY) {
                    window->pointerX = event.event_x;
                    window->pointerY = event.event_y;
                    window->updateCursor();
                }
                window->input->pointerPresence(type == XCB_ENTER_NOTIFY);
                window->input->flush();
            }
            break;
        }
        case XCB_FOCUS_IN:
        case XCB_FOCUS_OUT: {
            const xcb_focus_in_event_t& event = *(const xcb_focus_in_event_t*)(generic);
            WindowImpl* const window = findWindow(event.event);
            if (window != nullptr && event.mode != XCB_NOTIFY_MODE_GRAB && event.mode != XCB_NOTIFY_MODE_UNGRAB) {
                window->setFocused(type == XCB_FOCUS_IN);
            }
            break;
        }
        case XCB_EXPOSE: {
            const xcb_expose_event_t& event = *(const xcb_expose_event_t*)(generic);
            WindowImpl* const window = findWindow(event.window);
            if (window != nullptr && event.count == 0) {
                window->requestFrame();
            }
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            const xcb_configure_notify_event_t& event = *(const xcb_configure_notify_event_t*)(generic);
            WindowImpl* const window = findWindow(event.window);
            if (window != nullptr) {
                window->configure(event);
            }
            break;
        }
        case XCB_MAP_NOTIFY: {
            WindowImpl* const window = findWindow(((const xcb_map_notify_event_t*)(generic))->window);
            if (window != nullptr) {
                window->setIconified(false);
                window->requestFrame();
                if (window->focusOnShow) {
                    window->requestFocus();
                }
            }
            break;
        }
        case XCB_UNMAP_NOTIFY: {
            WindowImpl* const window = findWindow(((const xcb_unmap_notify_event_t*)(generic))->window);
            if (window != nullptr) {
                window->setIconified(true);
            }
            break;
        }
        case XCB_DESTROY_NOTIFY: {
            const xcb_window_t handle = ((const xcb_destroy_notify_event_t*)(generic))->window;
            cancelOutgoing(handle);
            WindowImpl* const window = findWindow(handle);
            if (window != nullptr) {
                window->requestClose();
            }
            break;
        }
        case XCB_PROPERTY_NOTIFY: {
            const xcb_property_notify_event_t& event = *(const xcb_property_notify_event_t*)(generic);
            if (event.window == selectionWindow && event.atom == atoms.timestamp && event.state == XCB_PROPERTY_NEW_VALUE) {
                timestampResult = event.time;
                if (timestampWaiter != nullptr) {
                    Fiber* const waiter = timestampWaiter;
                    timestampWaiter = nullptr;
                    waiter->wake();
                }
            }
            StreamInput* const input = findInput(event.window);
            if (input != nullptr) {
                input->propertyNotify(event);
            }
            outgoingProperty(event);
            WindowImpl* const window = findWindow(event.window);
            if (window != nullptr && event.atom == atoms.netWmState) {
                window->updateState();
            } else if (event.window == screen->root && event.atom == atoms.xkbRulesNames && event.state == XCB_PROPERTY_NEW_VALUE) {
                rebuildKeymap();
            }
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            const xcb_client_message_event_t& event = *(const xcb_client_message_event_t*)(generic);
            WindowImpl* const window = findWindow(event.window);
            if (window == nullptr) {
                break;
            }
            if (event.type == atoms.wmProtocols) {
                const xcb_atom_t protocol = event.data.data32[0];
                if (protocol == atoms.wmDeleteWindow) {
                    window->requestClose();
                } else if (protocol == atoms.wmTakeFocus) {
                    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT, window->window, event.data.data32[1]);
                } else if (protocol == atoms.netWmPing) {
                    xcb_client_message_event_t reply = event;
                    reply.window = screen->root;
                    xcb_send_event(connection, 0, screen->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (const char*)(&reply));
                }
            } else if (event.type == atoms.xdndEnter) {
                window->xdndEnter(event);
            } else if (event.type == atoms.xdndPosition) {
                window->xdndPosition(event);
            } else if (event.type == atoms.xdndDrop) {
                window->xdndDrop(event);
            } else if (event.type == atoms.xdndLeave) {
                window->xdndLeave(event);
            }
            break;
        }
        case XCB_SELECTION_REQUEST:
            selectionRequest(*(const xcb_selection_request_event_t*)(generic));
            break;
        case XCB_SELECTION_CLEAR: {
            const xcb_selection_clear_event_t& event = *(const xcb_selection_clear_event_t*)(generic);
            SelectionState* const state = selection(event.selection);
            if (state != nullptr) {
                state->owned = false;
            }
            break;
        }
        case XCB_SELECTION_NOTIFY: {
            const xcb_selection_notify_event_t& event = *(const xcb_selection_notify_event_t*)(generic);
            StreamInput* const input = findInput(event.requestor);
            if (input != nullptr) {
                input->selectionNotify(event);
            }
            break;
        }
        case XCB_MAPPING_NOTIFY:
            rebuildKeymap();
            break;
        default:
            break;
    }
}

void PlatformImpl::rebuildKeymap() {
    std::array<std::string, 5> values;
    xcb_get_property_cookie_t cookie = xcb_get_property(connection, 0, screen->root, atoms.xkbRulesNames, XCB_GET_PROPERTY_TYPE_ANY, 0, 16 * 1024 / 4);
    xcb_get_property_reply_t* const reply = xcb_get_property_reply(connection, cookie, nullptr);
    if (reply != nullptr) {
        const char* cursor = (const char*)(xcb_get_property_value(reply));
        size_t left = (size_t)(xcb_get_property_value_length(reply));
        for (std::string& value : values) {
            const void* const end = memchr(cursor, 0, left);
            if (end == nullptr) {
                break;
            }
            const size_t length = (const char*)(end)-cursor;
            value.assign(cursor, length);
            cursor += length + 1;
            left -= length + 1;
        }
        free(reply);
    }
    xkb_rule_names names{};
    names.rules = values[0].empty() ? nullptr : values[0].c_str();
    names.model = values[1].empty() ? nullptr : values[1].c_str();
    names.layout = values[2].empty() ? nullptr : values[2].c_str();
    names.variant = values[3].empty() ? nullptr : values[3].c_str();
    names.options = values[4].empty() ? nullptr : values[4].c_str();
    xkb_keymap* replacement = xkb_keymap_new_from_names(xkbContext, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (replacement == nullptr) {
        const xkb_rule_names fallback{};
        replacement = xkb_keymap_new_from_names(xkbContext, &fallback, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }
    if (replacement == nullptr) {
        if (keymap == nullptr) {
            fail(u8"X11 keyboard map initialization failed");
        }
        return;
    }
    xkb_state* const state = xkb_state_new(replacement);
    if (state == nullptr) {
        xkb_keymap_unref(replacement);
        if (xkbState == nullptr) {
            fail(u8"X11 keyboard state initialization failed");
        }
        return;
    }
    if (xkbState != nullptr) {
        xkb_state_unref(xkbState);
    }
    if (keymap != nullptr) {
        xkb_keymap_unref(keymap);
    }
    keymap = replacement;
    xkbState = state;
    pressedKeys.fill(false);
    enteredKeys.fill(false);
    if (composeState != nullptr) {
        xkb_compose_state_reset(composeState);
    }
}

void PlatformImpl::syncModifiers(u16 coreState) {
    if (xkbState == nullptr) {
        return;
    }
    const xkb_mod_mask_t coreModifiers = coreState & 0xff;
    const xkb_layout_index_t group = (coreState >> 13) & 3;
    xkb_state_update_mask(xkbState, coreModifiers, 0, 0, 0, 0, group);
}

void PlatformImpl::syncKeyboard() {
    if (keymap == nullptr) {
        return;
    }
    xkb_state* const replacement = xkb_state_new(keymap);
    if (replacement == nullptr) {
        return;
    }
    xkb_state_unref(xkbState);
    xkbState = replacement;
    pressedKeys.fill(false);
    enteredKeys.fill(false);
    const xcb_query_keymap_cookie_t keyCookie = xcb_query_keymap(connection);
    xcb_query_keymap_reply_t* const keys = xcb_query_keymap_reply(connection, keyCookie, nullptr);
    if (keys != nullptr) {
        for (size_t key = 0; key != enteredKeys.size(); ++key) {
            const bool down = ((u8)(keys->keys[key / 8]) & (1u << (key % 8))) != 0;
            enteredKeys[key] = down;
        }
        free(keys);
    }
    const xcb_query_pointer_cookie_t pointerCookie = xcb_query_pointer(connection, screen->root);
    xcb_query_pointer_reply_t* const pointer = xcb_query_pointer_reply(connection, pointerCookie, nullptr);
    if (pointer != nullptr) {
        syncModifiers(pointer->mask);
        free(pointer);
    }
}

u16 PlatformImpl::modifiers() const {
    if (xkbState == nullptr) {
        return 0;
    }
    auto active = [this](const char* name) {
        return xkb_state_mod_name_is_active(xkbState, name, XKB_STATE_MODS_EFFECTIVE) > 0;
    };
    u16 result = 0;
    if (active(XKB_MOD_NAME_SHIFT)) {
        result |= InputShift;
    }
    if (active(XKB_MOD_NAME_CTRL)) {
        result |= InputControl;
    }
    if (active(XKB_MOD_NAME_ALT)) {
        result |= InputAlt;
    }
    if (active(XKB_MOD_NAME_LOGO)) {
        result |= InputSuper;
    }
    if (active(XKB_MOD_NAME_CAPS)) {
        result |= InputCapsLock;
    }
    if (active(XKB_MOD_NAME_NUM)) {
        result |= InputNumLock;
    }
    if (active("Mod5")) {
        result |= InputAltGraph;
    }
    return result;
}

InputKey PlatformImpl::inputKey(xkb_keysym_t symbol) const {
    if (symbol >= XKB_KEY_F1 && symbol <= XKB_KEY_F35) {
        return (InputKey)((u8)(InputKey::F1) + symbol - XKB_KEY_F1);
    }
    if (symbol >= XKB_KEY_a && symbol <= XKB_KEY_z) {
        return InputKey::Printable;
    }
    if (symbol >= XKB_KEY_A && symbol <= XKB_KEY_Z) {
        return InputKey::Printable;
    }
    if ((symbol >= XKB_KEY_0 && symbol <= XKB_KEY_9) || (symbol >= XKB_KEY_space && symbol <= XKB_KEY_asciitilde)) {
        return InputKey::Printable;
    }
    switch (symbol) {
        case XKB_KEY_Escape:
            return InputKey::Escape;
        case XKB_KEY_Return:
            return InputKey::Enter;
        case XKB_KEY_BackSpace:
            return InputKey::Backspace;
        case XKB_KEY_Tab:
        case XKB_KEY_ISO_Left_Tab:
            return InputKey::Tab;
        case XKB_KEY_Insert:
            return InputKey::Insert;
        case XKB_KEY_Delete:
            return InputKey::Delete;
        case XKB_KEY_Home:
            return InputKey::Home;
        case XKB_KEY_End:
            return InputKey::End;
        case XKB_KEY_Up:
            return InputKey::Up;
        case XKB_KEY_Down:
            return InputKey::Down;
        case XKB_KEY_Left:
            return InputKey::Left;
        case XKB_KEY_Right:
            return InputKey::Right;
        case XKB_KEY_Page_Up:
            return InputKey::PageUp;
        case XKB_KEY_Page_Down:
            return InputKey::PageDown;
        case XKB_KEY_Clear:
            return InputKey::Clear;
        case XKB_KEY_KP_0:
        case XKB_KEY_KP_1:
        case XKB_KEY_KP_2:
        case XKB_KEY_KP_3:
        case XKB_KEY_KP_4:
        case XKB_KEY_KP_5:
        case XKB_KEY_KP_6:
        case XKB_KEY_KP_7:
        case XKB_KEY_KP_8:
        case XKB_KEY_KP_9:
            return (InputKey)((u8)(InputKey::Keypad0) + symbol - XKB_KEY_KP_0);
        case XKB_KEY_KP_Decimal:
            return InputKey::KeypadDecimal;
        case XKB_KEY_KP_Divide:
            return InputKey::KeypadDivide;
        case XKB_KEY_KP_Multiply:
            return InputKey::KeypadMultiply;
        case XKB_KEY_KP_Subtract:
            return InputKey::KeypadSubtract;
        case XKB_KEY_KP_Add:
            return InputKey::KeypadAdd;
        case XKB_KEY_KP_Enter:
            return InputKey::KeypadEnter;
        case XKB_KEY_KP_Equal:
            return InputKey::KeypadEqual;
        case XKB_KEY_KP_Separator:
            return InputKey::KeypadSeparator;
        case XKB_KEY_KP_F1:
            return InputKey::KeypadF1;
        case XKB_KEY_KP_F2:
            return InputKey::KeypadF2;
        case XKB_KEY_KP_F3:
            return InputKey::KeypadF3;
        case XKB_KEY_KP_F4:
            return InputKey::KeypadF4;
        case XKB_KEY_KP_Insert:
            return InputKey::KeypadInsert;
        case XKB_KEY_KP_Delete:
            return InputKey::KeypadDelete;
        case XKB_KEY_KP_Up:
            return InputKey::KeypadUp;
        case XKB_KEY_KP_Down:
            return InputKey::KeypadDown;
        case XKB_KEY_KP_Left:
            return InputKey::KeypadLeft;
        case XKB_KEY_KP_Right:
            return InputKey::KeypadRight;
        case XKB_KEY_KP_Home:
            return InputKey::KeypadHome;
        case XKB_KEY_KP_End:
            return InputKey::KeypadEnd;
        case XKB_KEY_KP_Page_Up:
            return InputKey::KeypadPageUp;
        case XKB_KEY_KP_Page_Down:
            return InputKey::KeypadPageDown;
        case XKB_KEY_KP_Begin:
            return InputKey::KeypadBegin;
        case XKB_KEY_KP_Space:
            return InputKey::KeypadSpace;
        case XKB_KEY_KP_Tab:
            return InputKey::KeypadTab;
        case XKB_KEY_Caps_Lock:
            return InputKey::CapsLock;
        case XKB_KEY_Scroll_Lock:
            return InputKey::ScrollLock;
        case XKB_KEY_Num_Lock:
            return InputKey::NumLock;
        case XKB_KEY_Print:
            return InputKey::PrintScreen;
        case XKB_KEY_Pause:
            return InputKey::Pause;
        case XKB_KEY_Menu:
            return InputKey::Menu;
        case XKB_KEY_Shift_L:
            return InputKey::LeftShift;
        case XKB_KEY_Control_L:
            return InputKey::LeftControl;
        case XKB_KEY_Alt_L:
            return InputKey::LeftAlt;
        case XKB_KEY_Super_L:
            return InputKey::LeftSuper;
        case XKB_KEY_Shift_R:
            return InputKey::RightShift;
        case XKB_KEY_Control_R:
            return InputKey::RightControl;
        case XKB_KEY_Alt_R:
        case XKB_KEY_ISO_Level3_Shift:
            return InputKey::RightAlt;
        case XKB_KEY_Super_R:
            return InputKey::RightSuper;
        case XKB_KEY_XF86AudioPlay:
            return InputKey::MediaPlay;
        case XKB_KEY_XF86AudioPause:
            return InputKey::MediaPause;
        case XKB_KEY_XF86AudioStop:
            return InputKey::MediaStop;
        case XKB_KEY_XF86AudioForward:
            return InputKey::MediaFastForward;
        case XKB_KEY_XF86AudioRewind:
            return InputKey::MediaRewind;
        case XKB_KEY_XF86AudioNext:
            return InputKey::MediaTrackNext;
        case XKB_KEY_XF86AudioPrev:
            return InputKey::MediaTrackPrevious;
        case XKB_KEY_XF86AudioRecord:
            return InputKey::MediaRecord;
        case XKB_KEY_XF86AudioLowerVolume:
            return InputKey::VolumeDown;
        case XKB_KEY_XF86AudioRaiseVolume:
            return InputKey::VolumeUp;
        case XKB_KEY_XF86AudioMute:
            return InputKey::VolumeMute;
        default:
            return xkb_keysym_to_utf32(symbol) != 0 ? InputKey::Printable : InputKey::Unknown;
    }
}

u32 PlatformImpl::keymapCodepoint(xkb_keycode_t key, xkb_layout_index_t layout, xkb_level_index_t level) const {
    if (keymap == nullptr) {
        return 0;
    }
    const xkb_keysym_t* symbols = nullptr;
    if (xkb_keymap_key_get_syms_by_level(keymap, key, layout, level, &symbols) <= 0) {
        return 0;
    }
    return xkb_keysym_to_utf32(symbols[0]);
}

size_t PlatformImpl::composeFeed(xkb_keysym_t symbol, u32 codepoint, u32* codepoints, size_t capacity) {
    if (composeState != nullptr && xkb_compose_state_feed(composeState, symbol) == XKB_COMPOSE_FEED_ACCEPTED) {
        switch (xkb_compose_state_get_status(composeState)) {
            case XKB_COMPOSE_COMPOSING:
                return 0;
            case XKB_COMPOSE_COMPOSED: {
                char buffer[64];
                const int length = xkb_compose_state_get_utf8(composeState, buffer, sizeof(buffer));
                xkb_compose_state_reset(composeState);
                if (length <= 0) {
                    return 0;
                }
                return decodeUtf8((const u8*)(buffer), std::min((size_t)(length), sizeof(buffer) - 1), codepoints, capacity);
            }
            case XKB_COMPOSE_CANCELLED:
                xkb_compose_state_reset(composeState);
                return 0;
            case XKB_COMPOSE_NOTHING:
                break;
        }
    }
    if (codepoint != 0 && capacity != 0) {
        codepoints[0] = codepoint;
        return 1;
    }
    return 0;
}

void PlatformImpl::keyboard(WindowImpl& window, const xcb_key_press_event_t& event, bool down, bool repeated) {
    if (xkbState == nullptr || event.detail >= pressedKeys.size()) {
        return;
    }
    const xkb_keycode_t keycode = event.detail;
    const bool entered = enteredKeys[event.detail];
    enteredKeys[event.detail] = false;
    repeated = repeated || (down && pressedKeys[event.detail]);
    syncModifiers(event.state);
    if (down && !repeated) {
        pressedKeys[event.detail] = true;
    }
    const xkb_keysym_t symbol = xkb_state_key_get_one_sym(xkbState, keycode);
    if (!down) {
        pressedKeys[event.detail] = false;
    }
    if (entered && !down) {
        return;
    }
    if (window.input == nullptr) {
        return;
    }
    const InputAction action = repeated ? InputAction::Repeat : (down ? InputAction::Press : InputAction::Release);
    const u32 codepoint = xkb_keysym_to_utf32(symbol);
    u32 composed[8];
    size_t composedCount = 0;
    if (action == InputAction::Press) {
        composedCount = composeFeed(symbol, codepoint, composed, sizeof(composed) / sizeof(composed[0]));
    } else if (action == InputAction::Repeat && (composeState == nullptr || xkb_compose_state_get_status(composeState) != XKB_COMPOSE_COMPOSING) && codepoint != 0) {
        composed[0] = codepoint;
        composedCount = 1;
    }
    const u16 activeModifiers = modifiers();
    const xkb_layout_index_t layout = xkb_state_key_get_layout(xkbState, keycode);
    window.input->key({
        .key = inputKey(symbol),
        .action = action,
        .modifiers = activeModifiers,
        .layoutCodepoint = layout == XKB_LAYOUT_INVALID ? 0 : keymapCodepoint(keycode, layout),
        .baseCodepoint = keymapCodepoint(keycode, 0),
        .shiftedCodepoint = layout == XKB_LAYOUT_INVALID ? 0 : keymapCodepoint(keycode, layout, 1),
    });
    if (action != InputAction::Release && !(activeModifiers & (InputControl | InputSuper))) {
        for (size_t index = 0; index != composedCount; ++index) {
            if (composed[index] >= 0x20 && composed[index] != 0x7f) {
                window.input->text({
                    .codepoint = composed[index],
                    .modifiers = activeModifiers,
                });
            }
        }
    }
    window.input->flush();
}

void PlatformImpl::releasePressedKeys(WindowImpl* window) {
    if (window == nullptr) {
        pressedKeys.fill(false);
        return;
    }
    for (size_t key = 0; key != pressedKeys.size(); ++key) {
        if (!pressedKeys[key]) {
            continue;
        }
        xcb_key_press_event_t event{};
        event.detail = (u8)(key);
        event.event = window->window;
        keyboard(*window, event, false);
    }
    pressedKeys.fill(false);
    enteredKeys.fill(false);
    if (composeState != nullptr) {
        xkb_compose_state_reset(composeState);
    }
}

OutgoingSelection::OutgoingSelection(PlatformImpl* platform_)
    : platform(platform_) {
}

void OutgoingSelection::ready() {
    platform->removeOutgoing(this);
}

StreamInput::StreamInput(PlatformImpl& platform_, Buffer&& local)
    : platform(platform_)
    , buffered(static_cast<Buffer&&>(local))
    , complete(true)
    , success(true) {
}

StreamInput::StreamInput(PlatformImpl& platform_, xcb_atom_t selection_, const std::vector<xcb_atom_t>& targets_, bool* drained_, xcb_timestamp_t time_, StreamInput** activeSlot_)
    : platform(platform_)
    , targets(targets_)
    , selection(selection_)
    , time(time_)
    , drained(drained_)
    , activeSlot(activeSlot_) {
    requestor = xcb_generate_id(platform.connection);
    const u32 mask = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_create_window(platform.connection, XCB_COPY_FROM_PARENT, requestor, platform.screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, &mask);
    platform.inputs.push_back(this);
    requestNextTarget();
}

StreamInput::~StreamInput() noexcept {
    if (activeSlot != nullptr && *activeSlot == this) {
        *activeSlot = nullptr;
    }
    if (requestor != XCB_WINDOW_NONE) {
        platform.removeInput(this);
        xcb_destroy_window(platform.connection, requestor);
        requestor = XCB_WINDOW_NONE;
    }
    if (drained != nullptr) {
        *drained = complete && success && offset == buffered.length();
    }
}

void StreamInput::operator delete(StreamInput* input, std::destroying_delete_t) noexcept {
    SmallObjAllocator* const allocator = input->platform.allocator_;
    allocator->release(input);
}

void StreamInput::wake() {
    if (waiter != nullptr) {
        Fiber* const fiber = waiter;
        waiter = nullptr;
        fiber->wake();
    }
}

void StreamInput::failTransfer() {
    complete = true;
    success = false;
    wake();
}

void StreamInput::requestNextTarget() {
    if (targetIndex == targets.size() || requestor == XCB_WINDOW_NONE) {
        failTransfer();
        return;
    }
    target = targets[targetIndex++];
    buffered.reset();
    offset = 0;
    incremental = false;
    xcb_delete_property(platform.connection, requestor, platform.atoms.selectionData);
    xcb_convert_selection(platform.connection, requestor, selection, target, platform.atoms.selectionData, time);
    platform.flush();
}

void StreamInput::selectionNotify(const xcb_selection_notify_event_t& event) {
    if (complete || event.selection != selection || event.target != target) {
        return;
    }
    if (event.property == XCB_ATOM_NONE) {
        requestNextTarget();
        return;
    }
    if (event.property != platform.atoms.selectionData) {
        failTransfer();
        return;
    }
    readProperty();
}

void StreamInput::propertyNotify(const xcb_property_notify_event_t& event) {
    if (!complete && incremental && event.atom == platform.atoms.selectionData && event.state == XCB_PROPERTY_NEW_VALUE) {
        if (offset != buffered.length()) {
            propertyPending = true;
        } else {
            buffered.reset();
            offset = 0;
            readProperty();
        }
    }
}

void StreamInput::readProperty() {
    if (complete) {
        return;
    }
    u32 longOffset = 0;
    xcb_atom_t propertyType = XCB_ATOM_NONE;
    size_t appended = 0;
    for (;;) {
        xcb_get_property_cookie_t cookie = xcb_get_property(platform.connection, 1, requestor, platform.atoms.selectionData, XCB_GET_PROPERTY_TYPE_ANY, longOffset, selectionReadChunkSize / 4);
        xcb_get_property_reply_t* const reply = xcb_get_property_reply(platform.connection, cookie, nullptr);
        if (reply == nullptr) {
            failTransfer();
            return;
        }
        if (propertyType == XCB_ATOM_NONE) {
            propertyType = reply->type;
        }
        const size_t length = (size_t)(xcb_get_property_value_length(reply));
        if (propertyType != platform.atoms.incr && length != 0) {
            if (propertyType == XCB_ATOM_STRING) {
                appendLatin1AsUtf8(buffered, (const u8*)(xcb_get_property_value(reply)), length);
            } else {
                buffered.append(xcb_get_property_value(reply), length);
            }
            appended += length;
        }
        const u32 bytesAfter = reply->bytes_after;
        free(reply);
        if (bytesAfter == 0 || propertyType == platform.atoms.incr) {
            break;
        }
        longOffset += (u32)((length + 3) / 4);
    }
    if (!incremental && propertyType == platform.atoms.incr) {
        incremental = true;
        buffered.reset();
        offset = 0;
        return;
    }
    if (propertyType == XCB_ATOM_NONE) {
        if (incremental) {
            failTransfer();
        } else {
            requestNextTarget();
        }
        return;
    }
    if (incremental && appended == 0) {
        complete = true;
        success = true;
    } else if (!incremental) {
        complete = true;
        success = true;
    }
    wake();
}

size_t StreamInput::readImpl(void* data, size_t length) {
    for (;;) {
        if (offset != buffered.length()) {
            const size_t count = std::min(length, buffered.length() - offset);
            memcpy(data, (const u8*)(buffered.data()) + offset, count);
            offset += count;
            if (offset == buffered.length() && !complete) {
                buffered.reset();
                offset = 0;
                if (propertyPending) {
                    propertyPending = false;
                    readProperty();
                }
            }
            return count;
        }
        if (complete) {
            if (drained != nullptr) {
                *drained = success;
            }
            return 0;
        }
        Fiber* const current = platform.scheduler_->current();
        if (current == nullptr) {
            failTransfer();
            return 0;
        }
        waiter = current;
        if (!current->parkFor(selectionTransferTimeoutUs)) {
            waiter = nullptr;
            failTransfer();
            return 0;
        }
    }
}

StreamOutput::StreamOutput(PlatformImpl& platform_, bool primary_)
    : platform(platform_)
    , primary(primary_) {
}

void StreamOutput::operator delete(StreamOutput* output, std::destroying_delete_t) noexcept {
    SmallObjAllocator* const allocator = output->platform.allocator_;
    allocator->release(output);
}

size_t StreamOutput::writeImpl(const void* data, size_t size) {
    accumulated.append(data, size);
    return size;
}

void StreamOutput::finishImpl() {
    if (!finished) {
        finished = true;
        platform.setSelection(primary, StringView(accumulated));
    }
}

SelectionState& PlatformImpl::selection(bool primary) {
    return primary ? primaryState : clipboardState;
}

SelectionState* PlatformImpl::selection(xcb_atom_t atom) {
    if (atom == XCB_ATOM_PRIMARY) {
        return &primaryState;
    }
    if (atom == atoms.clipboard) {
        return &clipboardState;
    }
    return nullptr;
}

void PlatformImpl::setSelection(bool primary, StringView content) {
    auto publish = [this, primary, content]() {
        SelectionState& state = selection(primary);
        state.content = Buffer(content);
        state.timestamp = serverTimestamp();
        state.owned = false;
        xcb_set_selection_owner(connection, selectionWindow, state.atom, state.timestamp);
        const xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(connection, state.atom);
        xcb_get_selection_owner_reply_t* const reply = xcb_get_selection_owner_reply(connection, cookie, nullptr);
        state.owned = reply != nullptr && reply->owner == selectionWindow;
        free(reply);
        flush();
    };
    if (scheduler_->current() != nullptr) {
        LockGuard guard(*selectionMutex_);
        publish();
    } else {
        publish();
    }
}

xcb_timestamp_t PlatformImpl::serverTimestamp() {
    ++timestampSerial;
    timestampResult = XCB_CURRENT_TIME;
    Fiber* const current = scheduler_->current();
    timestampWaiter = current;
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, selectionWindow, atoms.timestamp, XCB_ATOM_INTEGER, 32, 1, &timestampSerial);
    flush();
    if (current != nullptr) {
        if (!current->parkFor(5'000'000)) {
            timestampWaiter = nullptr;
        }
    } else {
        const xcb_get_input_focus_cookie_t cookie = xcb_get_input_focus(connection);
        xcb_get_input_focus_reply_t* const reply = xcb_get_input_focus_reply(connection, cookie, nullptr);
        free(reply);
        dispatchEvents();
    }
    timestampWaiter = nullptr;
    if (timestampResult != XCB_CURRENT_TIME) {
        return timestampResult;
    }
    return lastTimestamp;
}

Input* ClipboardImpl::read() {
    SelectionState& state = platform->selection(primary);
    if (state.owned) {
        return platform->allocator_->make<StreamInput>(*platform, Buffer(state.content));
    }
    const std::vector<xcb_atom_t> targets = {
        platform->atoms.utf8String,
        platform->atoms.textUtf8,
        platform->atoms.textPlain,
        XCB_ATOM_STRING,
    };
    return platform->createSelectionInput(state.atom, targets);
}

Output* ClipboardImpl::write() {
    return platform->allocator_->make<StreamOutput>(*platform, primary);
}

StreamInput* PlatformImpl::createSelectionInput(xcb_atom_t selectionAtom, const std::vector<xcb_atom_t>& targets, bool* drained, xcb_timestamp_t time, StreamInput** activeSlot) {
    StreamInput* const input = new (allocator_->allocate(sizeof(StreamInput))) StreamInput(*this, selectionAtom, targets, drained, time, activeSlot);
    if (activeSlot != nullptr) {
        *activeSlot = input;
    }
    return input;
}

bool PlatformImpl::writeSelectionProperty(xcb_window_t requestor, xcb_atom_t property, xcb_atom_t target, const SelectionState& state) {
    return writeSelectionTarget(requestor, property, target, state, true);
}

bool PlatformImpl::writeSelectionTarget(xcb_window_t requestor, xcb_atom_t property, xcb_atom_t target, const SelectionState& state, bool allowIncr) {
    if (property == XCB_ATOM_NONE) {
        return false;
    }
    if (target == atoms.targets) {
        const xcb_atom_t supported[] = {
            atoms.targets,
            atoms.timestamp,
            atoms.multiple,
            atoms.utf8String,
            atoms.textUtf8,
            atoms.textPlain,
            XCB_ATOM_STRING,
        };
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, requestor, property, XCB_ATOM_ATOM, 32, sizeof(supported) / sizeof(supported[0]), supported);
        return true;
    }
    if (target == atoms.timestamp) {
        const u32 timestamp = state.timestamp;
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, requestor, property, XCB_ATOM_INTEGER, 32, 1, &timestamp);
        return true;
    }
    if (target != atoms.utf8String && target != atoms.textUtf8 && target != atoms.textPlain && target != XCB_ATOM_STRING) {
        return false;
    }
    Buffer latin1;
    const Buffer* content = &state.content;
    if (target == XCB_ATOM_STRING) {
        latin1 = utf8ToLatin1(StringView(state.content));
        content = &latin1;
    }
    if (!allowIncr || content->length() <= selectionWriteChunkSize) {
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, requestor, property, target, 8, (u32)(content->length()), content->data());
        return true;
    }
    OutgoingSelection* const transfer = allocator_->make<OutgoingSelection>(this);
    transfer->requestor = requestor;
    transfer->property = property;
    transfer->target = target;
    transfer->content = *content;
    bool haveOriginalMask = false;
    for (OutgoingSelection* active : outgoing) {
        if (active->requestor == requestor) {
            transfer->originalEventMask = active->originalEventMask;
            haveOriginalMask = true;
            break;
        }
    }
    if (!haveOriginalMask) {
        const xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(connection, requestor);
        xcb_get_window_attributes_reply_t* const reply = xcb_get_window_attributes_reply(connection, cookie, nullptr);
        if (reply != nullptr) {
            transfer->originalEventMask = reply->your_event_mask;
            free(reply);
        }
    }
    outgoing.push_back(transfer);
    const u32 mask = transfer->originalEventMask | XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(connection, requestor, XCB_CW_EVENT_MASK, &mask);
    const u32 length = content->length() > UINT_MAX ? UINT_MAX : (u32)(content->length());
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, requestor, property, atoms.incr, 32, 1, &length);
    poller_->timeout(selectionTransferTimeoutUs, *transfer);
    return true;
}

void PlatformImpl::selectionRequest(const xcb_selection_request_event_t& event) {
    xcb_selection_notify_event_t notify{};
    notify.response_type = XCB_SELECTION_NOTIFY;
    notify.sequence = 0;
    notify.time = event.time;
    notify.requestor = event.requestor;
    notify.selection = event.selection;
    notify.target = event.target;
    notify.property = XCB_ATOM_NONE;
    const SelectionState* const state = selection(event.selection);
    const xcb_atom_t property = event.property == XCB_ATOM_NONE ? event.target : event.property;
    if (state != nullptr && state->owned) {
        if (event.target == atoms.multiple && event.property != XCB_ATOM_NONE) {
            std::vector<xcb_atom_t> pairs;
            u32 longOffset = 0;
            bool valid = true;
            for (;;) {
                const xcb_get_property_cookie_t cookie = xcb_get_property(connection, 0, event.requestor, event.property, XCB_GET_PROPERTY_TYPE_ANY, longOffset, selectionReadChunkSize / 4);
                xcb_get_property_reply_t* const reply = xcb_get_property_reply(connection, cookie, nullptr);
                if (reply == nullptr || reply->format != 32 || (reply->type != atoms.atomPair && reply->type != XCB_ATOM_ATOM)) {
                    free(reply);
                    valid = false;
                    break;
                }
                const size_t length = (size_t)(xcb_get_property_value_length(reply));
                if (length % sizeof(xcb_atom_t) != 0 || (length == 0 && reply->bytes_after != 0)) {
                    free(reply);
                    valid = false;
                    break;
                }
                const xcb_atom_t* const chunk = (const xcb_atom_t*)(xcb_get_property_value(reply));
                pairs.insert(pairs.end(), chunk, chunk + length / sizeof(xcb_atom_t));
                const u32 bytesAfter = reply->bytes_after;
                free(reply);
                if (bytesAfter == 0) {
                    break;
                }
                longOffset += (u32)(length / 4);
            }
            if (valid && pairs.size() % 2 == 0) {
                for (size_t index = 0; index != pairs.size(); index += 2) {
                    if (pairs[index + 1] == event.property || !writeSelectionTarget(event.requestor, pairs[index + 1], pairs[index], *state, true)) {
                        pairs[index + 1] = XCB_ATOM_NONE;
                    }
                }
                const size_t atomsPerRequest = selectionWriteChunkSize / sizeof(xcb_atom_t);
                size_t offset = 0;
                do {
                    const size_t count = std::min(atomsPerRequest, pairs.size() - offset);
                    xcb_change_property(connection, offset == 0 ? XCB_PROP_MODE_REPLACE : XCB_PROP_MODE_APPEND, event.requestor, event.property, atoms.atomPair, 32, (u32)(count), count == 0 ? nullptr : pairs.data() + offset);
                    offset += count;
                } while (offset != pairs.size());
                notify.property = event.property;
            }
        } else if (writeSelectionProperty(event.requestor, property, event.target, *state)) {
            notify.property = property;
        }
    }
    xcb_send_event(connection, 0, event.requestor, XCB_EVENT_MASK_NO_EVENT, (const char*)(&notify));
    flush();
}

void PlatformImpl::outgoingProperty(const xcb_property_notify_event_t& event) {
    if (event.state != XCB_PROPERTY_DELETE) {
        return;
    }
    for (size_t index = 0; index != outgoing.size(); ++index) {
        OutgoingSelection* const transfer = outgoing[index];
        if (transfer->requestor != event.window || transfer->property != event.atom) {
            continue;
        }
        poller_->timeout(selectionTransferTimeoutUs, *transfer);
        if (transfer->offset != transfer->content.length()) {
            const size_t count = std::min(selectionWriteChunkSize, transfer->content.length() - transfer->offset);
            xcb_change_property(connection, XCB_PROP_MODE_REPLACE, transfer->requestor, transfer->property, transfer->target, 8, (u32)(count), (const u8*)(transfer->content.data()) + transfer->offset);
            transfer->offset += count;
        } else {
            xcb_change_property(connection, XCB_PROP_MODE_REPLACE, transfer->requestor, transfer->property, transfer->target, 8, 0, nullptr);
            removeOutgoing(transfer);
        }
        flush();
        return;
    }
}

void PlatformImpl::removeOutgoing(OutgoingSelection* transfer) {
    const auto found = std::find(outgoing.begin(), outgoing.end(), transfer);
    if (found == outgoing.end()) {
        return;
    }
    const xcb_window_t requestor = transfer->requestor;
    const u32 originalEventMask = transfer->originalEventMask;
    poller_->cancel(*transfer);
    outgoing.erase(found);
    allocator_->release(transfer);
    bool stillActive = false;
    for (OutgoingSelection* active : outgoing) {
        stillActive = stillActive || active->requestor == requestor;
    }
    if (!stillActive && connection != nullptr) {
        xcb_change_window_attributes(connection, requestor, XCB_CW_EVENT_MASK, &originalEventMask);
        xcb_flush(connection);
    }
}

void PlatformImpl::cancelOutgoing(xcb_window_t requestor) {
    for (size_t index = 0; index != outgoing.size();) {
        OutgoingSelection* const transfer = outgoing[index];
        if (transfer->requestor == requestor) {
            removeOutgoing(transfer);
        } else {
            ++index;
        }
    }
}

FrameTimer::FrameTimer(WindowImpl* window_)
    : window(window_) {
}

void FrameTimer::ready() {
    window->dispatchFrame();
}

ResizeTimer::ResizeTimer(WindowImpl* window_)
    : window(window_) {
}

void ResizeTimer::ready() {
    window->resizeEnded();
}

WindowImpl::WindowImpl(PlatformImpl& platform_, const WindowOptions& options)
    : platform(platform_)
    , input(options.input)
    , events(options.events)
    , frame(options.frame)
    , dropTarget(options.drop) {
    primarySelection.platform = &platform;
    primarySelection.primary = true;
    clipboardSelection.platform = &platform;
    clipboardSelection.primary = false;
    info_.width = std::max(1u, std::min(options.width, 65'535u));
    info_.height = std::max(1u, std::min(options.height, 65'535u));
    info_.screenPixelWidth = platform.screen->width_in_pixels;
    info_.screenPixelHeight = platform.screen->height_in_pixels;
    info_.contentScale = platform.contentScale();
    minimumWidth = std::max(1u, options.minimumWidth);
    minimumHeight = std::max(1u, options.minimumHeight);

    window = xcb_generate_id(platform.connection);
    const u32 eventMask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;
    const u32 values[] = {
        platform.screen->black_pixel,
        eventMask,
    };
    xcb_create_window(platform.connection, platform.screen->root_depth, window, platform.screen->root, 0, 0, (u16)(info_.width), (u16)(info_.height), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, platform.screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
    platform.windows.push_back(this);

    const xcb_atom_t protocols[] = {
        platform.atoms.wmDeleteWindow,
        platform.atoms.wmTakeFocus,
        platform.atoms.netWmPing,
    };
    xcb_change_property(platform.connection, XCB_PROP_MODE_REPLACE, window, platform.atoms.wmProtocols, XCB_ATOM_ATOM, 32, sizeof(protocols) / sizeof(protocols[0]), protocols);
    const u32 pid = (u32)(getpid());
    xcb_change_property(platform.connection, XCB_PROP_MODE_REPLACE, window, platform.atoms.netWmPid, XCB_ATOM_CARDINAL, 32, 1, &pid);
    const u32 version = xdndVersion;
    xcb_change_property(platform.connection, XCB_PROP_MODE_REPLACE, window, platform.atoms.xdndAware, XCB_ATOM_ATOM, 32, 1, &version);

    Buffer wmClass;
    const StringView fallback(u8"application");
    const StringView instance = options.appId.empty() ? fallback : options.appId;
    const StringView applicationClass = options.appName.empty() ? instance : options.appName;
    const Buffer latin1Instance = utf8ToLatin1(instance);
    const Buffer latin1ApplicationClass = utf8ToLatin1(applicationClass);
    constexpr u8 separator = 0;
    wmClass.append(latin1Instance.data(), latin1Instance.length());
    wmClass.append(&separator, 1);
    wmClass.append(latin1ApplicationClass.data(), latin1ApplicationClass.length());
    wmClass.append(&separator, 1);
    platform.changeProperty8(window, platform.atoms.wmClass, XCB_ATOM_STRING, wmClass.data(), wmClass.length());
    const u32 motifHints[] = {
        1u << 1,
        0,
        options.decorations ? 1u : 0u,
        0,
        0,
    };
    xcb_change_property(platform.connection, XCB_PROP_MODE_REPLACE, window, platform.atoms.motifWmHints, platform.atoms.motifWmHints, 32, sizeof(motifHints) / sizeof(motifHints[0]), motifHints);
    updateNormalHints();
    requestTitle(options.title);
    platform.flush();
}

WindowImpl::~WindowImpl() noexcept {
    platform.poller_->cancel(frameTimer);
    platform.poller_->cancel(resizeTimer);
    if (platform.keyboardFocus == this) {
        platform.releasePressedKeys(this);
        platform.keyboardFocus = nullptr;
    }
    if (platform.dndSession != nullptr && platform.dndSession->window == this) {
        platform.dndSession->window = nullptr;
        platform.dndSession->leavePending = true;
        platform.dndSession->fiber->wake();
    }
    platform.cancelDropTransfers(this);
    platform.removeWindow(this);
    platform.cancelOutgoing(window);
    if (window != XCB_WINDOW_NONE) {
        xcb_destroy_window(platform.connection, window);
        platform.flush();
        window = XCB_WINDOW_NONE;
    }
}

void WindowImpl::requestShow() {
    if (!shown) {
        shown = true;
        xcb_map_window(platform.connection, window);
        platform.flush();
        requestFrame();
    }
}

void WindowImpl::requestClose() {
    if (!closeRequested) {
        closeRequested = true;
        if (events != nullptr) {
            events->close();
        }
    }
}

void WindowImpl::requestFrame() {
    frameRequested = true;
    if (!shown || frameScheduled || frame == nullptr || closeRequested) {
        return;
    }
    frameScheduled = true;
    platform.poller_->timeout(0, frameTimer);
}

void WindowImpl::dispatchFrame() {
    frameScheduled = false;
    if (!shown || !frameRequested || frame == nullptr || closeRequested || info_.iconified) {
        return;
    }
    frameRequested = false;
    if (!frame->frame(info_)) {
        if (frameRequested) {
            frameScheduled = true;
            ++frameRetries;
            platform.poller_->timeout(frameRetries > 1 ? 10'000 : 0, frameTimer);
        }
        return;
    }
    frameRetries = 0;
    if (frameRequested) {
        requestFrame();
    }
}

void WindowImpl::resizeEnded() {
    liveResize = false;
}

void WindowImpl::configure(const xcb_configure_notify_event_t& event) {
    const bool resized = info_.width != event.width || info_.height != event.height;
    info_.x = event.x;
    info_.y = event.y;
    info_.width = std::max(1u, (u32)(event.width));
    info_.height = std::max(1u, (u32)(event.height));
    if (resized) {
        liveResize = true;
        platform.poller_->timeout(150'000, resizeTimer);
        requestFrame();
    }
}

void WindowImpl::updateState() {
    xcb_get_property_cookie_t cookie = xcb_get_property(platform.connection, 0, window, platform.atoms.netWmState, XCB_ATOM_ATOM, 0, 1024);
    xcb_get_property_reply_t* const reply = xcb_get_property_reply(platform.connection, cookie, nullptr);
    if (reply == nullptr) {
        return;
    }
    bool horizontal = false;
    bool vertical = false;
    bool fullscreen = false;
    bool hidden = false;
    const size_t count = (size_t)(xcb_get_property_value_length(reply)) / sizeof(xcb_atom_t);
    const xcb_atom_t* const states = (const xcb_atom_t*)(xcb_get_property_value(reply));
    for (size_t index = 0; index != count; ++index) {
        horizontal = horizontal || states[index] == platform.atoms.netWmStateMaximizedHorz;
        vertical = vertical || states[index] == platform.atoms.netWmStateMaximizedVert;
        fullscreen = fullscreen || states[index] == platform.atoms.netWmStateFullscreen;
        hidden = hidden || states[index] == platform.atoms.netWmStateHidden;
    }
    free(reply);
    info_.maximized = horizontal && vertical;
    info_.fullscreen = fullscreen;
    info_.iconified = hidden;
    info_.tiled = (horizontal || vertical) && !info_.maximized;
}

void WindowImpl::updateInitialState() {
    std::array<xcb_atom_t, 4> states{};
    size_t count = 0;
    if (requestedAttention) {
        states[count++] = platform.atoms.netWmStateDemandsAttention;
    }
    if (requestedMaximized) {
        states[count++] = platform.atoms.netWmStateMaximizedVert;
        states[count++] = platform.atoms.netWmStateMaximizedHorz;
    }
    if (requestedFullscreen) {
        states[count++] = platform.atoms.netWmStateFullscreen;
    }
    if (count == 0) {
        xcb_delete_property(platform.connection, window, platform.atoms.netWmState);
    } else {
        xcb_change_property(platform.connection, XCB_PROP_MODE_REPLACE, window, platform.atoms.netWmState, XCB_ATOM_ATOM, 32, (u32)(count), states.data());
    }
    platform.flush();
}

void WindowImpl::updateInitialHints() {
    if (!requestedIconified) {
        xcb_delete_property(platform.connection, window, XCB_ATOM_WM_HINTS);
    } else {
        // ICCCM WM_HINTS: StateHint with initial_state=IconicState.
        std::array<u32, 9> hints{};
        hints[0] = 1u << 1;
        hints[2] = 3;
        xcb_change_property(platform.connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 32, (u32)(hints.size()), hints.data());
    }
    platform.flush();
}

void WindowImpl::setFocused(bool focused) {
    if (info_.focused == focused) {
        return;
    }
    info_.focused = focused;
    requestFrame();
    if (focused) {
        if (platform.keyboardFocus != nullptr && platform.keyboardFocus != this) {
            platform.keyboardFocus->setFocused(false);
        }
        platform.syncKeyboard();
        platform.keyboardFocus = this;
    } else if (platform.keyboardFocus == this) {
        platform.releasePressedKeys(this);
        platform.keyboardFocus = nullptr;
        if (input != nullptr) {
            input->preedit({}, -1, -1);
        }
    }
    if (input != nullptr) {
        input->focus(focused);
        input->flush();
    }
}

void WindowImpl::setIconified(bool iconified) {
    info_.iconified = iconified;
    if (!iconified && frameRequested) {
        requestFrame();
    }
}

void WindowImpl::updateNormalHints() {
    u32 hints[18]{};
    hints[0] = (1u << 4) | (1u << 6) | (1u << 8);
    hints[5] = minimumWidth;
    hints[6] = minimumHeight;
    hints[9] = resizeUnitWidth;
    hints[10] = resizeUnitHeight;
    hints[15] = resizeBaseWidth;
    hints[16] = resizeBaseHeight;
    xcb_change_property(platform.connection, XCB_PROP_MODE_REPLACE, window, platform.atoms.wmNormalHints, platform.atoms.wmSizeHints, 32, sizeof(hints) / sizeof(hints[0]), hints);
}

void WindowImpl::requestTitle(StringView title) {
    const Buffer latin1Title = utf8ToLatin1(title);
    platform.changeProperty8(window, platform.atoms.netWmName, platform.atoms.utf8String, title.data(), title.length());
    platform.changeProperty8(window, platform.atoms.wmName, XCB_ATOM_STRING, latin1Title.data(), latin1Title.length());
    platform.flush();
}

void WindowImpl::sendRootMessage(xcb_atom_t type, const std::array<u32, 5>& data) {
    xcb_client_message_event_t event{};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = window;
    event.type = type;
    memcpy(event.data.data32, data.data(), sizeof(event.data.data32));
    xcb_send_event(platform.connection, 0, platform.screen->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (const char*)(&event));
    platform.flush();
}

void WindowImpl::sendState(u32 action, xcb_atom_t first, xcb_atom_t second) {
    sendRootMessage(platform.atoms.netWmState, {action, first, second, 1, 0});
}

void WindowImpl::requestAttention() {
    requestedAttention = true;
    if (shown) {
        sendState(1, platform.atoms.netWmStateDemandsAttention);
    } else {
        updateInitialState();
    }
}

void WindowImpl::requestRestore() {
    requestedAttention = false;
    requestedMaximized = false;
    requestedFullscreen = false;
    requestedIconified = false;
    if (!shown) {
        updateInitialState();
        updateInitialHints();
        return;
    }
    updateInitialHints();
    // Mapping an already-viewable window is harmless.  Sending it
    // unconditionally also closes the race where WM_CHANGE_STATE has been
    // requested but its UnmapNotify has not reached us yet.
    if (shown) {
        xcb_map_window(platform.connection, window);
        info_.iconified = false;
    }
    sendState(0, platform.atoms.netWmStateFullscreen);
    sendState(0, platform.atoms.netWmStateMaximizedVert, platform.atoms.netWmStateMaximizedHorz);
    sendState(0, platform.atoms.netWmStateDemandsAttention);
}

void WindowImpl::requestIconify() {
    requestedIconified = true;
    if (shown) {
        sendRootMessage(platform.atoms.wmChangeState, {3, 0, 0, 0, 0});
    } else {
        updateInitialHints();
    }
}

void WindowImpl::requestMove(i32 x, i32 y) {
    const u32 values[] = {
        (u32)(x),
        (u32)(y),
    };
    xcb_configure_window(platform.connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    platform.flush();
}

void WindowImpl::requestFocus() {
    if (!shown) {
        focusOnShow = true;
        return;
    }
    focusOnShow = false;
    sendRootMessage(platform.atoms.netActiveWindow, {1, platform.lastTimestamp, 0, 0, 0});
}

void WindowImpl::requestMaximized(bool maximized) {
    requestedMaximized = maximized;
    if (shown) {
        sendState(maximized ? 1 : 0, platform.atoms.netWmStateMaximizedVert, platform.atoms.netWmStateMaximizedHorz);
    } else {
        updateInitialState();
    }
}

void WindowImpl::requestFullscreen(bool fullscreen) {
    requestedFullscreen = fullscreen;
    if (shown) {
        sendState(fullscreen ? 1 : 0, platform.atoms.netWmStateFullscreen);
    } else {
        updateInitialState();
    }
}

void WindowImpl::requestResize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        return;
    }
    const u32 values[] = {
        std::min(width, 65'535u),
        std::min(height, 65'535u),
    };
    xcb_configure_window(platform.connection, window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
    platform.flush();
}

void WindowImpl::requestMinimumSize(u32 width, u32 height) {
    minimumWidth = std::max(1u, width);
    minimumHeight = std::max(1u, height);
    updateNormalHints();
    platform.flush();
}

void WindowImpl::requestResizeUnit(u32 width, u32 height, u32 baseWidth, u32 baseHeight) {
    resizeUnitWidth = std::max(1u, width);
    resizeUnitHeight = std::max(1u, height);
    resizeBaseWidth = baseWidth;
    resizeBaseHeight = baseHeight;
    updateNormalHints();
    platform.flush();
}

Clipboard* WindowImpl::primary() {
    return &primarySelection;
}

Clipboard* WindowImpl::secondary() {
    return &clipboardSelection;
}

float PlatformImpl::contentScale() const {
    return contentScale_;
}

xcb_cursor_t PlatformImpl::cursor(PointerIcon icon) {
    const size_t index = (size_t)(icon);
    if (index >= cursors.size()) {
        return XCB_NONE;
    }
    if (cursors[index] != XCB_NONE) {
        return cursors[index];
    }
    if (cursorFont == XCB_NONE) {
        cursorFont = xcb_generate_id(connection);
        const char fontName[] = "cursor";
        xcb_open_font(connection, cursorFont, sizeof(fontName) - 1, fontName);
    }
    u16 glyph = 68;
    switch (icon) {
        case PointerIcon::Help:
            glyph = 92;
            break;
        case PointerIcon::Pointer:
        case PointerIcon::Alias:
        case PointerIcon::Copy:
        case PointerIcon::ZoomIn:
        case PointerIcon::ZoomOut:
            glyph = 60;
            break;
        case PointerIcon::Progress:
        case PointerIcon::Wait:
            glyph = 150;
            break;
        case PointerIcon::Cell:
        case PointerIcon::Crosshair:
            glyph = 34;
            break;
        case PointerIcon::Text:
        case PointerIcon::VerticalText:
            glyph = 152;
            break;
        case PointerIcon::Move:
        case PointerIcon::Grab:
        case PointerIcon::Grabbing:
        case PointerIcon::AllScroll:
        case PointerIcon::ResizeAll:
            glyph = 52;
            break;
        case PointerIcon::NoDrop:
        case PointerIcon::NotAllowed:
            glyph = 88;
            break;
        case PointerIcon::ResizeEast:
            glyph = 96;
            break;
        case PointerIcon::ResizeNorth:
            glyph = 138;
            break;
        case PointerIcon::ResizeNorthEast:
        case PointerIcon::ResizeNorthEastSouthWest:
            glyph = 136;
            break;
        case PointerIcon::ResizeNorthWest:
        case PointerIcon::ResizeNorthWestSouthEast:
            glyph = 134;
            break;
        case PointerIcon::ResizeSouth:
            glyph = 16;
            break;
        case PointerIcon::ResizeSouthEast:
            glyph = 14;
            break;
        case PointerIcon::ResizeSouthWest:
            glyph = 12;
            break;
        case PointerIcon::ResizeWest:
            glyph = 70;
            break;
        case PointerIcon::ResizeEastWest:
        case PointerIcon::ResizeColumn:
            glyph = 108;
            break;
        case PointerIcon::ResizeNorthSouth:
        case PointerIcon::ResizeRow:
            glyph = 116;
            break;
        default:
            break;
    }
    const xcb_cursor_t handle = xcb_generate_id(connection);
    xcb_create_glyph_cursor(connection, handle, cursorFont, cursorFont, glyph, glyph + 1, 0, 0, 0, UINT16_MAX, UINT16_MAX, UINT16_MAX);
    cursors[index] = handle;
    return handle;
}

void WindowImpl::updateCursor() {
    const xcb_cursor_t cursor = platform.cursor(pointerIcon);
    if (cursor != XCB_NONE) {
        xcb_change_window_attributes(platform.connection, window, XCB_CW_CURSOR, &cursor);
        platform.flush();
    }
}

void WindowImpl::requestPointerIcon(PointerIcon icon) {
    pointerIcon = icon;
    updateCursor();
}

void WindowImpl::requestOpenUri(StringView uri) {
    Buffer path;
    path.append(uri.data(), uri.length());
    char* const arguments[] = {
        (char*)("xdg-open"),
        path.cStr(),
        nullptr,
    };
    pid_t pid = -1;
    posix_spawnp(&pid, arguments[0], nullptr, nullptr, arguments, environ);
}

void WindowImpl::requestTextInputRect(i32 x, i32 y, u32 width, u32 height) {
    textInputX = x;
    textInputY = y;
    textInputWidth = width;
    textInputHeight = height;
}

WindowInfo WindowImpl::info() const {
    return info_;
}

bool WindowImpl::inLiveResize() const {
    return liveResize;
}

RenderContext WindowImpl::renderContext() const {
    return {
        .backend = RenderBackend::X11,
        .connection = platform.connection,
        .window = (void*)(uintptr_t)(window),
    };
}

std::string PlatformImpl::atomName(xcb_atom_t atom) {
    if (atom == XCB_ATOM_NONE) {
        return {};
    }
    const xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(connection, atom);
    xcb_get_atom_name_reply_t* const reply = xcb_get_atom_name_reply(connection, cookie, nullptr);
    if (reply == nullptr) {
        return {};
    }
    const std::string name(xcb_get_atom_name_name(reply), xcb_get_atom_name_name_length(reply));
    free(reply);
    return name;
}

size_t DndOfferView::formats() const {
    return names == nullptr ? 0 : names->size();
}

StringView DndOfferView::format(size_t index) const {
    if (names == nullptr || index >= names->size()) {
        return {};
    }
    const std::string& name = (*names)[index];
    return StringView((const u8*)(name.data()), name.size());
}

DropOffer* DndDrop::what() {
    return offer;
}

Input* DndDrop::read(StringView mime) {
    if (started || offer == nullptr || atoms == nullptr || offer->names == nullptr) {
        started = true;
        return platform->allocator_->make<StreamInput>(*platform, Buffer());
    }
    started = true;
    for (size_t index = 0; index != offer->names->size(); ++index) {
        if (sameString(mime, (*offer->names)[index])) {
            const std::vector<xcb_atom_t> target = {(*atoms)[index]};
            return platform->createSelectionInput(platform->atoms.xdndSelection, target, &drained, time, session == nullptr ? nullptr : &session->input);
        }
    }
    return platform->allocator_->make<StreamInput>(*platform, Buffer());
}

void PlatformImpl::beginDnd(WindowImpl& window, const xcb_client_message_event_t& event) {
    WindowImpl* const target = &window;
    if (dndSession != nullptr) {
        dndSession->leavePending = true;
        dndSession->fiber->wake();
    }
    if (std::find(windows.begin(), windows.end(), target) == windows.end()) {
        return;
    }
    const xcb_window_t targetWindow = target->window;
    const xcb_window_t source = event.data.data32[0];
    const u32 version = (event.data.data32[1] >> 24) & 0xff;
    std::vector<xcb_atom_t> types;
    if (event.data.data32[1] & 1) {
        const xcb_get_property_cookie_t cookie = xcb_get_property(connection, 0, source, atoms.xdndTypeList, XCB_ATOM_ATOM, 0, 4096);
        xcb_get_property_reply_t* const reply = xcb_get_property_reply(connection, cookie, nullptr);
        if (reply != nullptr && reply->format == 32 && reply->type == XCB_ATOM_ATOM) {
            const size_t count = (size_t)(xcb_get_property_value_length(reply)) / sizeof(xcb_atom_t);
            const xcb_atom_t* const offered = (const xcb_atom_t*)(xcb_get_property_value(reply));
            types.assign(offered, offered + count);
        }
        free(reply);
    } else {
        for (size_t index = 2; index != 5; ++index) {
            if (event.data.data32[index] != XCB_ATOM_NONE) {
                types.push_back(event.data.data32[index]);
            }
        }
    }
    std::vector<std::string> names;
    names.reserve(types.size());
    std::vector<xcb_atom_t> validTypes;
    validTypes.reserve(types.size());
    for (xcb_atom_t type : types) {
        std::string name = atomName(type);
        if (!name.empty()) {
            validTypes.push_back(type);
            names.push_back(static_cast<std::string&&>(name));
        }
    }
    spawnTask([this, targetWindow, source, version, types = static_cast<std::vector<xcb_atom_t>&&>(validTypes), names = static_cast<std::vector<std::string>&&>(names)]() mutable {
        WindowImpl* const target = findWindow(targetWindow);
        if (target == nullptr) {
            return;
        }
        DndSession session;
        session.window = target;
        session.target = targetWindow;
        session.source = source;
        session.version = version;
        session.types = static_cast<std::vector<xcb_atom_t>&&>(types);
        session.names = static_cast<std::vector<std::string>&&>(names);
        runDndSession(session);
    });
}

void PlatformImpl::runDndSession(DndSession& session) {
    session.fiber = scheduler_->current();
    dndSession = &session;
    while (session.window != nullptr) {
        while (!session.motionPending && !session.dropPending && !session.leavePending && session.window != nullptr) {
            session.fiber->park();
        }
        if (session.window == nullptr || session.leavePending) {
            endDnd(session, true);
            return;
        }
        if (session.motionPending) {
            session.motionPending = false;
            session.accepted = false;
            session.acceptedType = XCB_ATOM_NONE;
            session.acceptedAction = DropAction::None;
            if (session.window->dropTarget != nullptr) {
                DndOfferView offer;
                offer.names = &session.names;
                i32 x = session.rootX - session.window->info_.x;
                i32 y = session.rootY - session.window->info_.y;
                const xcb_translate_coordinates_cookie_t cookie = xcb_translate_coordinates(connection, screen->root, session.window->window, (i16)(session.rootX), (i16)(session.rootY));
                xcb_translate_coordinates_reply_t* const translated = xcb_translate_coordinates_reply(connection, cookie, nullptr);
                if (translated != nullptr) {
                    x = translated->dst_x;
                    y = translated->dst_y;
                    free(translated);
                }
                const DropReply reply = session.window->dropTarget->dragOver(offer, x, y);
                for (size_t index = 0; index != session.names.size(); ++index) {
                    if (!sameString(reply.mime, session.names[index])) {
                        continue;
                    }
                    xcb_atom_t action = XCB_ATOM_NONE;
                    if (reply.action == DropAction::Copy) {
                        action = atoms.xdndActionCopy;
                    } else if (reply.action == DropAction::Move) {
                        action = atoms.xdndActionMove;
                    }
                    if (action != XCB_ATOM_NONE && (session.proposedAction == XCB_ATOM_NONE || session.proposedAction == action)) {
                        session.accepted = true;
                        session.acceptedType = session.types[index];
                        session.acceptedAction = reply.action;
                    }
                    break;
                }
            }
            sendDndStatus(session);
        }
        if (session.dropPending) {
            session.dropPending = false;
            bool success = false;
            DropTarget* const dropTarget = session.accepted && session.window != nullptr ? session.window->dropTarget : nullptr;
            // The payload read can park this fiber.  End the hover session
            // first so a later drag cannot orphan this stack object, and do
            // not retain a WindowImpl pointer across that blocking callback:
            // the immutable XID is sufficient for XdndFinished.
            if (dndSession == &session) {
                dndSession = nullptr;
            }
            session.transferWindow = session.window;
            session.window = nullptr;
            if (dropTarget != nullptr) {
                dropTransfers.push_back(&session);
                DndOfferView offer;
                offer.names = &session.names;
                DndDrop drop;
                drop.platform = this;
                drop.session = &session;
                drop.offer = &offer;
                drop.atoms = &session.types;
                drop.time = session.time;
                dropTarget->dropped(drop);
                success = drop.started && drop.drained;
                // The public contract requires the transfer stream to be
                // deleted before dropped() returns.  Detach defensively if a
                // target violates it so its eventual destructor cannot write
                // through pointers into this stack frame.
                if (session.input != nullptr) {
                    session.input->activeSlot = nullptr;
                    session.input->drained = nullptr;
                    session.input = nullptr;
                }
                const auto found = std::find(dropTransfers.begin(), dropTransfers.end(), &session);
                if (found != dropTransfers.end()) {
                    dropTransfers.erase(found);
                }
            }
            session.transferWindow = nullptr;
            sendDndFinished(session, success);
            endDnd(session, false);
            return;
        }
    }
    endDnd(session, false);
}

void PlatformImpl::sendDndStatus(DndSession& session) {
    xcb_client_message_event_t event{};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = session.source;
    event.type = atoms.xdndStatus;
    event.data.data32[0] = 0;
    if (session.window != nullptr) {
        event.data.data32[0] = session.window->window;
    }
    // Bit 1 asks the source to keep sending positions; the zero rectangle
    // otherwise has compositor-dependent suppression semantics.
    event.data.data32[1] = 2 | (session.accepted ? 1 : 0);
    event.data.data32[2] = 0;
    event.data.data32[3] = 0;
    if (session.acceptedAction == DropAction::Copy) {
        event.data.data32[4] = atoms.xdndActionCopy;
    } else if (session.acceptedAction == DropAction::Move) {
        event.data.data32[4] = atoms.xdndActionMove;
    }
    xcb_send_event(connection, 0, session.source, XCB_EVENT_MASK_NO_EVENT, (const char*)(&event));
    flush();
}

void PlatformImpl::sendDndFinished(DndSession& session, bool success) {
    xcb_client_message_event_t event{};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = session.source;
    event.type = atoms.xdndFinished;
    event.data.data32[0] = session.target;
    event.data.data32[1] = success ? 1 : 0;
    if (success && session.acceptedAction == DropAction::Copy) {
        event.data.data32[2] = atoms.xdndActionCopy;
    } else if (success && session.acceptedAction == DropAction::Move) {
        event.data.data32[2] = atoms.xdndActionMove;
    }
    xcb_send_event(connection, 0, session.source, XCB_EVENT_MASK_NO_EVENT, (const char*)(&event));
    flush();
}

void PlatformImpl::endDnd(DndSession& session, bool notifyLeft) {
    if (notifyLeft && session.window != nullptr && session.window->dropTarget != nullptr) {
        session.window->dropTarget->dragLeft();
    }
    if (dndSession == &session) {
        dndSession = nullptr;
    }
}

void PlatformImpl::cancelDropTransfers(WindowImpl* window) {
    const std::vector<DndSession*> active = dropTransfers;
    for (DndSession* session : active) {
        if (session == nullptr || std::find(dropTransfers.begin(), dropTransfers.end(), session) == dropTransfers.end() || session->transferWindow != window) {
            continue;
        }
        session->transferWindow = nullptr;
        if (session->input != nullptr) {
            // failTransfer resumes the drop fiber synchronously.  Its callback
            // returns, unregisters the session, and sends XdndFinished before
            // WindowImpl destroys the target XID.
            session->input->failTransfer();
        }
    }
}

void WindowImpl::xdndEnter(const xcb_client_message_event_t& event) {
    platform.beginDnd(*this, event);
}

void WindowImpl::xdndPosition(const xcb_client_message_event_t& event) {
    DndSession* const session = platform.dndSession;
    if (session == nullptr || session->window != this || session->source != event.data.data32[0]) {
        return;
    }
    session->rootX = (i16)(event.data.data32[2] >> 16);
    session->rootY = (i16)(event.data.data32[2] & 0xffff);
    session->time = event.data.data32[3];
    session->proposedAction = event.data.data32[4];
    session->motionPending = true;
    session->fiber->wake();
}

void WindowImpl::xdndDrop(const xcb_client_message_event_t& event) {
    DndSession* const session = platform.dndSession;
    if (session == nullptr || session->window != this || session->source != event.data.data32[0]) {
        return;
    }
    if (session->version >= 1) {
        session->time = event.data.data32[2];
    }
    session->dropPending = true;
    session->fiber->wake();
}

void WindowImpl::xdndLeave(const xcb_client_message_event_t& event) {
    DndSession* const session = platform.dndSession;
    if (session == nullptr || session->window != this || session->source != event.data.data32[0]) {
        return;
    }
    session->leavePending = true;
    session->fiber->wake();
}

Platform* plt::createX11Platform(ObjPool& owner) {
    return owner.make<PlatformImpl>(owner);
}
