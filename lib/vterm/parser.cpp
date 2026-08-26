/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "parser.h"

#include "vt_trace.h"

#include <lib/vterm/color.h>
#include <lib/vterm/base64.h>
#include <lib/vterm/color_spec.h>
#include <lib/vterm/color_names.h>
#include <lib/vterm/terminal_types.h>

#include <std/alg/minmax.h>
#include <std/lib/buffer.h>
#include <std/mem/obj_pool.h>

#include <new>
#include <string.h>

#if defined(__SSE2__)
    #include <emmintrin.h>
#endif

#if defined(SHITTY_COMPACT_PARSER)
    #define SHITTY_PARSER_GENERATED "parser_test.rl.h"
#else
    #define SHITTY_PARSER_GENERATED "parser.rl.h"
#endif

using namespace stl;
using namespace plt;

namespace {
    struct ParserTermcapQuery {
        size_t offset;
        size_t length;
        u8 value;
    };

    [[gnu::always_inline]] static inline size_t printableAsciiPrefix(const u8* input, size_t size) {
        using Bytes = u8 __attribute__((vector_size(16)));
#if !defined(__SSE2__)
        using Bits = unsigned __int128;
#endif
        constexpr Bytes spaces = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
        constexpr Bytes deletes = {0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f};
        size_t offset = 0;
        while (size - offset >= sizeof(Bytes)) {
            Bytes word;
            memcpy(&word, input + offset, sizeof(word));
            const Bytes invalidBytes = (word < spaces) | (word >= deletes);
#if defined(__SSE2__)
            const u32 invalid = _mm_movemask_epi8(__builtin_bit_cast(__m128i, invalidBytes));
            if (invalid != 0) {
                return offset + __builtin_ctz(invalid);
            }
#else
            const Bits invalid = __builtin_bit_cast(Bits, invalidBytes);
            const u64 low = invalid;
            if (low != 0) {
                return offset + __builtin_ctzll(low) / 8;
            }
            const u64 high = invalid >> 64;
            if (high != 0) {
                return offset + 8 + __builtin_ctzll(high) / 8;
            }
#endif
            offset += sizeof(word);
        }
        while (offset < size && input[offset] >= 0x20 && input[offset] < 0x7f) {
            ++offset;
        }
        return offset;
    }

    [[gnu::always_inline]] static inline size_t zeroPrefix(const u8* input, size_t size) {
        using Bytes = u8 __attribute__((vector_size(16)));
        constexpr Bytes zero = {};
        size_t offset = 0;
        while (size - offset >= sizeof(Bytes)) {
            Bytes word;
            memcpy(&word, input + offset, sizeof(word));
#if defined(__SSE2__)
            const u32 zeros = _mm_movemask_epi8(__builtin_bit_cast(__m128i, word == zero));
            if (zeros != 0xffff) {
                return offset + __builtin_ctz((~zeros) & 0xffff);
            }
#else
            const Bytes nonzero = word != zero;
            using Bits = unsigned __int128;
            const Bits bits = __builtin_bit_cast(Bits, nonzero);
            const u64 low = bits;
            if (low != 0) {
                return offset + __builtin_ctzll(low) / 8;
            }
            const u64 high = bits >> 64;
            if (high != 0) {
                return offset + 8 + __builtin_ctzll(high) / 8;
            }
#endif
            offset += sizeof(word);
        }
        while (offset < size && input[offset] == 0) {
            ++offset;
        }
        return offset;
    }

    struct ProtocolParser {
        constexpr const static size_t maxParameters = 32;
        constexpr const static size_t maxDcsBytes = 4095;
        constexpr const static size_t maxOscBytes = 1024 * 1024;
        constexpr const static u32 maxSixelWidth = 4096;
        constexpr const static u32 maxSixelHeight = 4096;
        constexpr const static size_t maxUdkDefinitions = maxDcsBytes / 4 + 1;
        constexpr const static size_t maxTermcapQueries = maxDcsBytes / 2 + 1;

        int state = 0;
        u8 csiPrefix = 0;
        u8 csiIntermediates[4] = {};
        u8 csiIntermediateCount = 0;
        u32 parameters[maxParameters] = {};
        unsigned char separators[maxParameters] = {};
        bool present[maxParameters] = {};
        size_t parameterCount = 0;
        bool csiHadParameters = false;
        // Grows on demand up to maxOscBytes: a resident megabyte per
        // parser instance is paid only by sessions that actually stream
        // large control strings (OSC 52 clipboard payloads).
        stl::Buffer scratchStorage;
        u8* scratch = nullptr;
        size_t scratchCapacity = 0;
        size_t scratchSize = 0;
        size_t decodedOffset = 0;
        size_t decodedSize = 0;
        bool overflow = false;
        size_t stringLimit = 0;
        u8 stringUtf8Remaining = 0;
        u8 groundUtf8Remaining = 0;
        u8 discardedUtf8Remaining = 0;

        u8 dcsIntermediates[4] = {};
        u8 dcsIntermediateCount = 0;
        size_t dcsCapabilityOffset = 0;
        size_t dcsCapabilityDecodedLength = 0;
        u8 dcsCapabilityCandidates = 0;
        u8 dcsCapabilityHighNibble = 0;
        bool dcsCapabilityHasHighNibble = false;
        bool dcsCapabilityValid = false;
        bool dcsCapabilityComplete = false;
        ParserTermcapQuery dcsTermcapQueries[maxTermcapQueries];
        size_t dcsTermcapQueryCount = 0;
        ParserUdkDefinition dcsUdkDefinitions[maxUdkDefinitions];
        size_t dcsUdkDefinitionCount = 0;
        size_t dcsUdkValueOffset = 0;
        u32 dcsUdkCode = 0;
        InputKey dcsUdkKey = InputKey::Unknown;
        u8 dcsUdkHighNibble = 0;
        bool dcsUdkHasCode = false;
        bool dcsUdkHasHighNibble = false;
        bool dcsUdkValid = false;
        bool dcsUdkInValue = false;
        bool dcsUdkHeaderValid = false;
        bool dcsUdkClearDefinitions = false;
        bool dcsUdkLockDefinitions = false;
        bool dcsColorValid = false;
        bool dcsTabValid = false;
        u32 dcsCursorNumbers[5] = {};
        u8 dcsCursorNumberCount = 0;
        u8 dcsCursorBytes[4] = {};
        u8 dcsCursorByteCount = 0;
        u16 dcsCursorCharsetIds[4] = {};
        Charset dcsCursorCharsets[4] = {};
        u8 dcsCursorCharsetCount = 0;
        u16 dcsUpssId = 0;
        u8 dcsUpssBytes = 0;
        bool dcsUpss96 = false;
        bool dcsUpssValid = false;
        bool dcsUpssComplete = false;
        // Grows on demand like scratchStorage: rows of maxSixelWidth
        // bytes, zeroed up to sixelAllocatedRows. The image stays in
        // the parser until ST hands it to the interface whole, so a
        // multi-pass color band never touches the screen twice.
        stl::Buffer sixelGrid;
        u8* sixelRows = nullptr;
        size_t sixelGridRows = 0;
        u8 sixelPalette[SixelPatch::paletteBytes] = {};
        u32 sixelX = 0;
        u32 sixelBand = 0;
        u32 sixelPaintedWidth = 0;
        u32 sixelPaintedHeight = 0;
        u32 sixelDeclaredWidth = 0;
        u32 sixelDeclaredHeight = 0;
        u32 sixelAllocatedRows = 0;
        u32 sixelRegister = 0;

        u32 oscCommand = 0;
        size_t oscPayloadOffset = 0;
        bool oscCommandValid = false;
        bool oscTerminated = false;
        bool oscTitleHex = false;
        bool oscTitleHasHighNibble = false;
        bool oscTitleValid = false;
        size_t oscCwdPathOffset = 0;
        bool oscCwdValid = false;
        size_t oscHyperlinkIdOffset = 0;
        size_t oscHyperlinkIdLength = 0;
        size_t oscHyperlinkUriOffset = 0;
        bool oscHyperlinkHasId = false;
        u32 oscProgressState = 0;
        u32 oscProgressPercent = 0;
        bool oscProgressStatePresent = false;
        bool oscProgressPercentPresent = false;
        bool oscProgressValid = false;
        u8 osc52ReplySelector = 0;
        bool osc52Primary = false;
        bool osc52Clipboard = false;
        bool osc52SelectClipboard = false;
        bool osc52SelectorSeen = false;
        bool osc52PayloadSeen = false;
        bool osc52Query = false;
        size_t osc52PayloadOffset = 0;
        size_t oscNotificationFieldOffset = 0;
        size_t oscNotificationIdOffset = 0;
        size_t oscNotificationIdLength = 0;
        size_t oscNotificationPayloadOffset = 0;
        u32 oscNotificationPayloadBytes = 0;
        u8 oscNotificationKey = 0;
        bool oscNotificationValid = false;
        bool oscNotificationEncoded = false;
        bool oscNotificationFinal = false;
        bool oscNotificationQuery = false;
        bool oscNotificationClose = false;
        bool oscNotificationBody = false;
        Color oscColor{};
        double oscColorComponents[3]{};
        double oscColorMantissa = 0.0;
        double oscColorFraction = 0.1;
        u64 oscColorHex = 0;
        size_t oscColorNameOffset = 0;
        u32 oscColorExponent = 0;
        u8 oscColorComponent = 0;
        u8 oscColorDigits = 0;
        bool oscColorNegative = false;
        bool oscColorExponentNegative = false;
        bool oscColorValid = false;
        bool oscColorQuery = false;
        u32 oscFieldNumber = 0;
        u32 oscFieldFirst = 0;
        bool oscFieldNumeric = false;
        bool oscFieldPresent = false;
        bool oscFieldFirstValid = false;
        bool oscFieldHaveFirst = false;
        u8 scsIndex = 0;
        u8 scsMod = 0;
        bool scs96 = false;
        bool scsMultibyte = false;
    };

    template <bool traced>
    struct ParserImpl final: public Parser {
        ParserImpl(ParserIface& iface, VtermTrace* trace, bool osc52SelectClipboard);

        void reset() override;
        void feed(StringView bytes) override;
        void setOsc52SelectClipboard(bool clipboard) override;
        [[gnu::always_inline]] bool consumeStringUtf8Byte(u8 ch);
        [[gnu::always_inline]] bool executeC0(u8 ch);
        [[gnu::always_inline]] void groundControl(u8 ch);
        [[gnu::always_inline]] size_t highStringPrefix(const u8* data, size_t size);
        bool ragelGroundContinuation(u8 ch);
        void ragelGroundHigh(u8 ch);
        void ragelGroundAscii(u8 ch);
        size_t ragelStringSize() const noexcept;
        const u8* ragelStringData() const noexcept;
        void resetDecoded(size_t offset = 0) noexcept;
        StringView decodedString() const noexcept;
        void appendDecoded(u8 ch);
        void ensureScratch(size_t needed);
        bool decodeBase64(size_t offset) noexcept;
        void decodeCwd() noexcept;
        void decodeTitle() noexcept;
        void ragelAppendStringSpan(const u8* data, size_t size, size_t limit);
        void ragelAppendString(const u8& ch, size_t limit);
        void ragelAppendSynthetic(u8 ch, size_t limit);
        void ragelAppendEscapedString(u8 ch, size_t limit);
        void ragelBeginString(VtermTraceString type, bool buffered);
        [[gnu::always_inline]] void traceVt52Byte(u8 ch, bool final);
        void ragelBeginDcs();
        void ragelBeginOsc();
        void resetOscColor();
        bool ragelStringContinuation(const u8& ch);
        void ragelFinishString();
        void ragelFinishDcs();
        void finishDcsColor();
        void finishDcsTab();
        void ragelBeginSixel();
        void ensureSixelRows(u32 rows);
        void paintSixel(u8 byte, u32 count);
        void finishSixelColor();
        void finishSixelRaster();
        void finishSixel();
        void ragelFinishOsc();
        StringView ragelOscPayload();
        void beginCsi();
        u32 parameter(size_t index) const noexcept;
        u32 countParameter(size_t index) const noexcept;
        CsiRectangle rectangle(size_t offset) const noexcept;
        void dispatchScoscSlrm();
        void dispatchEraseDisplay(bool selective);
        void dispatchEraseLine(bool selective);
        void dispatchTabClear();
        void dispatchCursorStyle();
        void dispatchStandardMode(u32 mode, bool enabled);
        void dispatchStandardModes(bool set);
        void dispatchPrivateMode(u32 mode, bool enabled);
        void dispatchPrivateModes(bool set);
        bool privateModeValue(u32 mode, const ParserModeState& state, bool& value) const;
        void dispatchPrivateSave();
        void dispatchPrivateRestore();
        void dispatchModeReport(bool privateMode);
        void dispatchDecfra();
        void dispatchDeccra();
        void dispatchDecera(bool selective);
        void dispatchDeccara(bool reverse);
        void dispatchDecrqcra();
        void dispatchDecll();
        void dispatchDsr(bool privateMode);
        void dispatchTitleMode(bool set);
        void dispatchDecscl();
        void dispatchWindowOps();
        void dispatchLocatorReporting();
        void dispatchDecsle();
        void dispatchDecac();
        void dispatchXtmodkeys();
        void dispatchXtqmodkeys();
        void dispatchKittyKeyboardSet();
        void dispatchKittyClipboard(StringView payload);
        void designateCharset(u8 final);
        Charset decodeCharset(u16 id, bool is96) const;
        bool parseSgrColor(size_t& index, CellColor& color, int& paletteIndex);
        template <typename Sink>
        void dispatchSgrTo(Sink& sink, size_t first);
        void dispatchSgr();
        void traceCsi(u8 finalByte);

        ParserIface& iface;
        VtermTrace* parserTrace;
        ProtocolParser parser;
    };

#define SHITTY_PARSER_DATA
#include SHITTY_PARSER_GENERATED
#undef SHITTY_PARSER_DATA
}

template <bool traced>
ParserImpl<traced>::ParserImpl(ParserIface& iface_, VtermTrace* trace, bool osc52SelectClipboard)
    : iface(iface_)
    , parserTrace(trace)
{
    parser.osc52SelectClipboard = osc52SelectClipboard;
    int& cs = parser.state;
#define SHITTY_PARSER_INIT
#include SHITTY_PARSER_GENERATED
#undef SHITTY_PARSER_INIT
}

template <bool traced>
void ParserImpl<traced>::reset() {
    const bool osc52SelectClipboard = parser.osc52SelectClipboard;
    parser.~ProtocolParser();
    ::new (static_cast<void*>(&parser)) ProtocolParser();
    parser.osc52SelectClipboard = osc52SelectClipboard;
    int& cs = parser.state;
#define SHITTY_PARSER_INIT
#include SHITTY_PARSER_GENERATED
#undef SHITTY_PARSER_INIT
    if constexpr (traced) {
        parserTrace->escapeCancel();
        parserTrace->stringCancel();
    }
}

template <bool traced>
void ParserImpl<traced>::setOsc52SelectClipboard(bool clipboard) {
    parser.osc52SelectClipboard = clipboard;
}

template <bool traced>
[[gnu::always_inline]] inline bool ParserImpl<traced>::consumeStringUtf8Byte(u8 ch) {
    if (parser.stringUtf8Remaining != 0) {
        if ((ch & 0xc0) == 0x80) {
            --parser.stringUtf8Remaining;
            return true;
        }
        parser.stringUtf8Remaining = 0;
    }

    if (ch >= 0xc2 && ch <= 0xdf) {
        parser.stringUtf8Remaining = 1;
    } else if (ch >= 0xe0 && ch <= 0xef) {
        parser.stringUtf8Remaining = 2;
    } else if (ch >= 0xf0 && ch <= 0xf4) {
        parser.stringUtf8Remaining = 3;
    }
    return false;
}

template <bool traced>
[[gnu::always_inline]] inline bool ParserImpl<traced>::executeC0(u8 ch) {
    if (ch >= 0x20 || ch == '\x18' || ch == '\x1a' || ch == '\x1b') {
        return false;
    }
    if (ch == '\a') {
        iface.parserBell();
        return true;
    }
    if (ch == '\x0e') {
        iface.parserLockingShiftGl(1);
        return true;
    }
    if (ch == '\x0f') {
        iface.parserLockingShiftGl(0);
        return true;
    }
    switch (ch) {
        case '\b':
            iface.parserMoveCursorBackward(1);
            break;
        case '\t':
            iface.inp_HT();
            break;
        case '\n':
        case '\v':
        case '\f':
            // Same LNM handling as the ground state: an embedded LF is
            // still a line feed.
            iface.esc_IND();
            if (iface.parserAutoNewlineMode()) {
                iface.inp_CR();
            }
            break;
        case '\r':
            iface.inp_CR();
            break;
        default:
            break;
    }
    return true;
}

template <bool traced>
[[gnu::always_inline]] inline void ParserImpl<traced>::groundControl(u8 ch) {
    if constexpr (traced) {
        if (ch != 0) {
            parserTrace->control(ch);
        }
    }
    iface.parserResetGraphemeInput();
    switch (ch) {
        case '\a':
            iface.parserBell();
            break;
        case '\b':
            iface.parserMoveCursorBackward(1);
            break;
        case '\t':
            iface.inp_HT();
            break;
        case '\n':
        case '\v':
        case '\f':
            iface.esc_IND();
            if (iface.parserAutoNewlineMode()) {
                iface.inp_CR();
            }
            break;
        case '\r':
            iface.inp_CR();
            break;
        case '\x0e':
            iface.parserLockingShiftGl(1);
            break;
        case '\x0f':
            iface.parserLockingShiftGl(0);
            break;
        default:
            break;
    }
}

template <bool traced>
[[gnu::always_inline]] inline size_t ParserImpl<traced>::highStringPrefix(const u8* data, size_t size) {
    size_t count = 0;
    while (count < size) {
        const u8 ch = data[count];
        const bool passive = ch >= 0xa0 || (ch >= 0x80 && ch <= 0x8f) || (ch >= 0x91 && ch <= 0x95) || ch == 0x99;
        const bool continuation = parser.stringUtf8Remaining != 0 && (ch & 0xc0) == 0x80;
        if (!passive && !continuation) {
            break;
        }
        consumeStringUtf8Byte(ch);
        ++count;
    }
    return count;
}

template <bool traced>
bool ParserImpl<traced>::ragelGroundContinuation(u8 ch) {
    if (parser.groundUtf8Remaining == 0 || ch < 0x80) {
        return false;
    }
    if constexpr (traced) {
        parserTrace->text(&ch, 1);
    }
    iface.parserGroundHigh(ch);
    if ((ch & 0xc0) == 0x80) {
        --parser.groundUtf8Remaining;
    } else if (ch >= 0xc2 && ch <= 0xdf) {
        parser.groundUtf8Remaining = 1;
    } else if (ch >= 0xe0 && ch <= 0xef) {
        parser.groundUtf8Remaining = 2;
    } else if (ch >= 0xf0 && ch <= 0xf4) {
        parser.groundUtf8Remaining = 3;
    } else {
        parser.groundUtf8Remaining = 0;
    }
    return true;
}

template <bool traced>
void ParserImpl<traced>::ragelGroundHigh(u8 ch) {
    if (ragelGroundContinuation(ch)) {
        return;
    }
    if constexpr (traced) {
        if (ch >= 0xa0) {
            parserTrace->text(&ch, 1);
        } else {
            parserTrace->control(ch);
        }
    }
    if (ch <= 0x9f) {
        iface.parserResetGraphemeInput();
    }
    iface.parserGroundHigh(ch);
    if (!iface.parserGroundUtf8Enabled()) {
        parser.groundUtf8Remaining = 0;
    } else if (ch >= 0xc2 && ch <= 0xdf) {
        parser.groundUtf8Remaining = 1;
    } else if (ch >= 0xe0 && ch <= 0xef) {
        parser.groundUtf8Remaining = 2;
    } else if (ch >= 0xf0 && ch <= 0xf4) {
        parser.groundUtf8Remaining = 3;
    } else {
        parser.groundUtf8Remaining = 0;
    }
}

template <bool traced>
void ParserImpl<traced>::ragelGroundAscii(u8 ch) {
    parser.groundUtf8Remaining = 0;
    if constexpr (traced) {
        parserTrace->text(&ch, 1);
    }
    iface.parserGroundAscii(ch);
}

template <bool traced>
size_t ParserImpl<traced>::ragelStringSize() const noexcept {
    return parser.scratchSize;
}

template <bool traced>
const u8* ParserImpl<traced>::ragelStringData() const noexcept {
    return parser.scratch;
}

template <bool traced>
void ParserImpl<traced>::resetDecoded(size_t offset) noexcept {
    parser.decodedOffset = offset;
    parser.decodedSize = 0;
}

template <bool traced>
StringView ParserImpl<traced>::decodedString() const noexcept {
    return StringView(parser.scratch + parser.decodedOffset, parser.decodedSize);
}

template <bool traced>
void ParserImpl<traced>::ensureScratch(size_t needed) {
    if (needed <= parser.scratchCapacity) {
        return;
    }
    size_t capacity = parser.scratchCapacity == 0 ? 4096 : parser.scratchCapacity;
    while (capacity < needed) {
        capacity *= 2;
    }
    if (capacity > ProtocolParser::maxOscBytes) {
        capacity = ProtocolParser::maxOscBytes;
    }
    Buffer replacement(capacity);
    if (parser.scratchSize != 0) {
        replacement.append(parser.scratch, parser.scratchSize);
    }
    parser.scratchStorage.xchg(replacement);
    parser.scratch = (u8*)(parser.scratchStorage.mutData());
    parser.scratchCapacity = capacity;
}

template <bool traced>
void ParserImpl<traced>::appendDecoded(u8 ch) {
    if (parser.decodedOffset + parser.decodedSize == ProtocolParser::maxOscBytes) {
        parser.overflow = true;
        return;
    }
    ensureScratch(parser.decodedOffset + parser.decodedSize + 1);
    parser.scratch[parser.decodedOffset + parser.decodedSize++] = ch;
}

template <bool traced>
bool ParserImpl<traced>::decodeBase64(size_t offset) noexcept {
    resetDecoded(offset);
    parser.decodedSize = parser.scratchSize - offset;
    const bool valid = base64DecodeInPlace(parser.scratch + offset, parser.decodedSize);
    return valid;
}

template <bool traced>
void ParserImpl<traced>::decodeCwd() noexcept {
    const size_t begin = parser.oscCwdPathOffset;
    const size_t end = parser.scratchSize;
    resetDecoded(begin);
    for (size_t source = begin; source < end;) {
        if (parser.scratch[source] != '%') {
            parser.scratch[begin + parser.decodedSize++] = parser.scratch[source++];
            continue;
        }
        const auto nibble = [](u8 ch) {
            return ch <= '9' ? ch - '0' : (ch | 0x20) - 'a' + 10;
        };
        parser.scratch[begin + parser.decodedSize++] = (nibble(parser.scratch[source + 1]) << 4) | nibble(parser.scratch[source + 2]);
        source += 3;
    }
}

template <bool traced>
void ParserImpl<traced>::decodeTitle() noexcept {
    const size_t begin = parser.oscPayloadOffset;
    resetDecoded(begin);
    for (size_t source = begin; source + 1 < parser.scratchSize; source += 2) {
        const auto nibble = [](u8 ch) {
            return ch <= '9' ? ch - '0' : (ch | 0x20) - 'a' + 10;
        };
        const u8 decoded = (nibble(parser.scratch[source]) << 4) | nibble(parser.scratch[source + 1]);
        if (decoded < 32) {
            break;
        }
        parser.scratch[begin + parser.decodedSize++] = decoded;
    }
}

template <bool traced>
void ParserImpl<traced>::ragelAppendStringSpan(const u8* data, size_t size, size_t limit) {
    if constexpr (traced) {
        parserTrace->stringData(data, size);
    }
    const size_t available = parser.scratchSize < limit ? limit - parser.scratchSize : 0;
    const size_t appendSize = min(size, available);
    if (appendSize != 0) {
        ensureScratch(parser.scratchSize + appendSize);
        memcpy(parser.scratch + parser.scratchSize, data, appendSize);
        parser.scratchSize += appendSize;
    }
    if (appendSize != size) {
        parser.overflow = true;
    }
}

template <bool traced>
void ParserImpl<traced>::ragelAppendString(const u8& ch, size_t limit) {
    if constexpr (traced) {
        parserTrace->stringData(&ch, 1);
    }
    if (parser.scratchSize == limit) {
        parser.overflow = true;
        return;
    }
    ensureScratch(parser.scratchSize + 1);
    parser.scratch[parser.scratchSize++] = ch;
}

template <bool traced>
void ParserImpl<traced>::ragelAppendSynthetic(u8 ch, size_t limit) {
    ragelAppendString(ch, limit);
}

template <bool traced>
void ParserImpl<traced>::ragelAppendEscapedString(u8 ch, size_t limit) {
    const u8 bytes[] = {'\x1b', ch};
    ragelAppendStringSpan(bytes, sizeof(bytes), limit);
}

template <bool traced>
[[gnu::always_inline]] inline void ParserImpl<traced>::traceVt52Byte(u8 ch, bool final) {
    if constexpr (traced) {
        parserTrace->escapeByte(ch);
        if (final) {
            parserTrace->escapeEnd();
        }
    }
}

template <bool traced>
void ParserImpl<traced>::ragelBeginString(VtermTraceString type, bool buffered) {
    iface.parserResetGraphemeInput();
    parser.stringUtf8Remaining = 0;
    parser.stringLimit = type == VtermTraceString::Dcs ? parser.maxDcsBytes : type == VtermTraceString::Osc ? parser.maxOscBytes : 0;
    if (buffered) {
        parser.scratchSize = 0;
        resetDecoded();
        parser.overflow = false;
    }
    if constexpr (traced) {
        parserTrace->stringBegin(type);
    }
}

template <bool traced>
void ParserImpl<traced>::ragelBeginDcs() {
    ragelBeginString(VtermTraceString::Dcs, true);
    parser.parameters[0] = 0;
    parser.separators[0] = 0;
    parser.present[0] = false;
    parser.parameterCount = 1;
    parser.dcsIntermediateCount = 0;
    parser.dcsCapabilityOffset = 0;
    parser.dcsCapabilityDecodedLength = 0;
    parser.dcsCapabilityCandidates = 0;
    parser.dcsCapabilityHasHighNibble = false;
    parser.dcsCapabilityValid = false;
    parser.dcsCapabilityComplete = false;
    parser.dcsTermcapQueryCount = 0;
    parser.dcsUdkDefinitionCount = 0;
    resetDecoded();
    parser.dcsUdkValueOffset = 0;
    parser.dcsUdkCode = 0;
    parser.dcsUdkKey = InputKey::Unknown;
    parser.dcsUdkHasCode = false;
    parser.dcsUdkHasHighNibble = false;
    parser.dcsUdkValid = false;
    parser.dcsUdkInValue = false;
    parser.dcsUdkHeaderValid = false;
    parser.dcsUdkClearDefinitions = false;
    parser.dcsUdkLockDefinitions = false;
}

template <bool traced>
void ParserImpl<traced>::ragelBeginOsc() {
    ragelBeginString(VtermTraceString::Osc, true);
    parser.oscCommand = 0;
    parser.oscPayloadOffset = 0;
    parser.oscCommandValid = false;
    parser.oscTerminated = false;
    resetDecoded();
    parser.oscTitleHex = false;
    parser.oscTitleHasHighNibble = false;
    parser.oscTitleValid = false;
    parser.oscCwdPathOffset = 0;
    parser.oscCwdValid = false;
    parser.oscHyperlinkIdOffset = 0;
    parser.oscHyperlinkIdLength = 0;
    parser.oscHyperlinkUriOffset = 0;
    parser.oscHyperlinkHasId = false;
    parser.oscProgressState = 0;
    parser.oscProgressPercent = 0;
    parser.oscProgressStatePresent = false;
    parser.oscProgressPercentPresent = false;
    parser.oscProgressValid = false;
    parser.osc52ReplySelector = 0;
    parser.osc52Primary = false;
    parser.osc52Clipboard = false;
    parser.osc52SelectorSeen = false;
    parser.osc52PayloadSeen = false;
    parser.osc52Query = false;
}

template <bool traced>
void ParserImpl<traced>::resetOscColor() {
    parser.oscColor = {};
    parser.oscColorComponents[0] = 0.0;
    parser.oscColorComponents[1] = 0.0;
    parser.oscColorComponents[2] = 0.0;
    parser.oscColorHex = 0;
    parser.oscColorComponent = 0;
    parser.oscColorDigits = 0;
    parser.oscColorValid = true;
    parser.oscColorQuery = false;
}

template <bool traced>
bool ParserImpl<traced>::ragelStringContinuation(const u8& ch) {
    if (!consumeStringUtf8Byte(ch)) {
        return false;
    }
    if (parser.stringLimit != 0) {
        ragelAppendString(ch, parser.stringLimit);
    } else if constexpr (traced) {
        parserTrace->stringData(&ch, 1);
    }
    return true;
}

template <bool traced>
void ParserImpl<traced>::ragelFinishString() {
    parser.stringUtf8Remaining = 0;
    parser.stringLimit = 0;
    if constexpr (traced) {
        parserTrace->stringEnd();
    }
}

template <bool traced>
void ParserImpl<traced>::ragelFinishDcs() {
    ragelFinishString();
}

template <bool traced>
void ParserImpl<traced>::finishDcsColor() {
    if (parser.dcsColorValid && parser.parameterCount <= 5) {
        const u32 index = parameter(0);
        const u32 model = parameter(1);
        if (index < 256) {
            if (model == 1) {
                iface.dcs_DECRSTS_HLS(index, parameter(2), parameter(3), parameter(4));
            } else if (model == 2) {
                iface.dcs_DECRSTS_RGB(index, parameter(2), parameter(3), parameter(4));
            }
        }
    }
    parser.parameters[0] = 0;
    parser.present[0] = false;
    parser.parameterCount = 1;
    parser.dcsColorValid = true;
}

template <bool traced>
void ParserImpl<traced>::finishDcsTab() {
    if (parser.dcsTabValid && parser.present[0] && parser.parameters[0] > 1) {
        iface.dcs_DECRSTS_TAB(parser.parameters[0]);
    }
    parser.parameters[0] = 0;
    parser.present[0] = false;
    parser.dcsTabValid = true;
}

template <bool traced>
void ParserImpl<traced>::ragelBeginSixel() {
    parser.sixelX = 0;
    parser.sixelBand = 0;
    parser.sixelPaintedWidth = 0;
    parser.sixelPaintedHeight = 0;
    parser.sixelDeclaredWidth = 0;
    parser.sixelDeclaredHeight = 0;
    parser.sixelAllocatedRows = 0;
    parser.sixelRegister = 0;
    parser.parameters[0] = 0;
    parser.present[0] = false;
    parser.parameterCount = 1;

    // The VT340 power-up color map, in percent RGB.
    constexpr const static u8 powerUpMap[16][3] = {
        {0, 0, 0},
        {20, 20, 80},
        {80, 13, 13},
        {20, 80, 20},
        {80, 20, 80},
        {20, 80, 80},
        {80, 80, 20},
        {53, 53, 53},
        {26, 26, 26},
        {33, 33, 60},
        {60, 26, 26},
        {33, 60, 33},
        {60, 33, 60},
        {33, 60, 60},
        {60, 60, 33},
        {80, 80, 80},
    };
    memset(parser.sixelPalette, 0, sizeof(parser.sixelPalette));
    for (size_t index = 0; index < 16; ++index) {
        for (size_t channel = 0; channel < 3; ++channel) {
            parser.sixelPalette[index * 3 + channel] = (u8)(((u32)(powerUpMap[index][channel]) * 255 + 50) / 100);
        }
    }
}

template <bool traced>
void ParserImpl<traced>::ensureSixelRows(u32 rows) {
    if (rows <= parser.sixelAllocatedRows) {
        return;
    }
    const size_t needed = (size_t)(rows)*ProtocolParser::maxSixelWidth;
    if (rows > parser.sixelGridRows) {
        size_t capacity = parser.sixelGridRows == 0 ? 64 : parser.sixelGridRows;
        while (capacity < rows) {
            capacity *= 2;
        }
        capacity = min<size_t>(capacity, ProtocolParser::maxSixelHeight);
        Buffer replacement(capacity * ProtocolParser::maxSixelWidth);
        if (parser.sixelAllocatedRows != 0) {
            replacement.append(parser.sixelRows, (size_t)(parser.sixelAllocatedRows) * ProtocolParser::maxSixelWidth);
        }
        parser.sixelGrid.xchg(replacement);
        parser.sixelRows = (u8*)(parser.sixelGrid.mutData());
        parser.sixelGridRows = capacity;
    }
    memset(parser.sixelRows + (size_t)(parser.sixelAllocatedRows) * ProtocolParser::maxSixelWidth, 0, needed - (size_t)(parser.sixelAllocatedRows) * ProtocolParser::maxSixelWidth);
    parser.sixelAllocatedRows = rows;
}

template <bool traced>
void ParserImpl<traced>::paintSixel(u8 byte, u32 count) {
    const u8 bits = (u8)(byte - 0x3f);
    const u64 target = (u64)(parser.sixelX) + count;
    const u32 begin = parser.sixelX;
    const u32 end = (u32)(min<u64>(target, ProtocolParser::maxSixelWidth));
    parser.sixelX = (u32)(min<u64>(target, (u64)(ProtocolParser::maxSixelWidth) + 1));
    if (bits == 0 || begin >= end || parser.sixelBand * 6 >= ProtocolParser::maxSixelHeight) {
        return;
    }

    ensureSixelRows(min(parser.sixelBand * 6 + 6, ProtocolParser::maxSixelHeight));
    const u8 value = (u8)(parser.sixelRegister + 1);
    u32 bottom = 0;
    for (u32 bit = 0; bit < 6; ++bit) {
        if ((bits & (1u << bit)) == 0) {
            continue;
        }
        const u32 y = parser.sixelBand * 6 + bit;
        if (y >= parser.sixelAllocatedRows) {
            break;
        }
        memset(parser.sixelRows + (size_t)(y)*ProtocolParser::maxSixelWidth + begin, value, end - begin);
        bottom = y + 1;
    }
    parser.sixelPaintedWidth = max(parser.sixelPaintedWidth, end);
    parser.sixelPaintedHeight = max(parser.sixelPaintedHeight, bottom);
}

template <bool traced>
void ParserImpl<traced>::finishSixelColor() {
    parser.sixelRegister = parser.parameters[0] % SixelPatch::paletteEntries;
    if (parser.parameterCount >= 5) {
        u8* rgb = parser.sixelPalette + parser.sixelRegister * 3;
        if (parser.parameters[1] == 2) {
            for (size_t channel = 0; channel < 3; ++channel) {
                rgb[channel] = (u8)((min<u32>(parser.parameters[2 + channel], 100) * 255 + 50) / 100);
            }
        } else if (parser.parameters[1] == 1) {
            const Color color = decHlsColor(parser.parameters[2], parser.parameters[3], parser.parameters[4]);
            rgb[0] = color.red;
            rgb[1] = color.green;
            rgb[2] = color.blue;
        }
    }
    parser.parameters[0] = 0;
    parser.present[0] = false;
    parser.parameterCount = 1;
}

template <bool traced>
void ParserImpl<traced>::finishSixelRaster() {
    if (parser.parameterCount >= 3 && parser.present[2]) {
        parser.sixelDeclaredWidth = parser.parameters[2];
    }
    if (parser.parameterCount >= 4 && parser.present[3]) {
        parser.sixelDeclaredHeight = parser.parameters[3];
    }
    parser.parameters[0] = 0;
    parser.present[0] = false;
    parser.parameterCount = 1;
}

template <bool traced>
void ParserImpl<traced>::finishSixel() {
    const u32 width = min(max(parser.sixelPaintedWidth, parser.sixelDeclaredWidth), ProtocolParser::maxSixelWidth);
    const u32 height = min(max(parser.sixelPaintedHeight, parser.sixelDeclaredHeight), ProtocolParser::maxSixelHeight);
    if (width == 0 || height == 0) {
        return;
    }
    // A size declared past the painted extent still reads as
    // transparent pixels.
    ensureSixelRows(height);
    const ParserSixelImage image = {
        .pixels = parser.sixelRows,
        .palette = parser.sixelPalette,
        .pitch = ProtocolParser::maxSixelWidth,
        .width = width,
        .height = height,
    };
    iface.dcs_SIXEL(image);
}

template <bool traced>
void ParserImpl<traced>::ragelFinishOsc() {
    ragelFinishString();
}

template <bool traced>
StringView ParserImpl<traced>::ragelOscPayload() {
    return StringView(parser.scratch + parser.oscPayloadOffset, parser.scratchSize - parser.oscPayloadOffset);
}

template <bool traced>
void ParserImpl<traced>::beginCsi() {
    parser.stringUtf8Remaining = 0;
    parser.stringLimit = 0;
    iface.parserResetGraphemeInput();
    parser.parameters[0] = 0;
    parser.separators[0] = 0;
    parser.present[0] = false;
    parser.parameterCount = 1;
    parser.csiHadParameters = false;
    parser.csiPrefix = 0;
    parser.csiIntermediateCount = 0;
}

template <bool traced>
u32 ParserImpl<traced>::parameter(size_t index) const noexcept {
    return index < parser.parameterCount ? parser.parameters[index] : 0;
}

template <bool traced>
u32 ParserImpl<traced>::countParameter(size_t index) const noexcept {
    const u32 value = parameter(index);
    return value ? value : 1;
}

template <bool traced>
CsiRectangle ParserImpl<traced>::rectangle(size_t offset) const noexcept {
    return {
        parameter(offset),
        parameter(offset + 1),
        parameter(offset + 2),
        parameter(offset + 3),
    };
}

template <bool traced>
void ParserImpl<traced>::dispatchScoscSlrm() {
    if (iface.horizontalMarginMode()) {
        iface.csi_SLRM(parameter(0), parameter(1), parser.parameterCount <= 2);
    } else {
        iface.esc_DECSC();
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchEraseDisplay(bool selective) {
    switch (parameter(0)) {
        case 0:
            if (selective) {
                iface.selectiveEraseDisplayAfter();
            } else {
                iface.eraseDisplayAfter();
            }
            break;
        case 1:
            if (selective) {
                iface.selectiveEraseDisplayBefore();
            } else {
                iface.eraseDisplayBefore();
            }
            break;
        case 2:
            if (selective) {
                iface.selectiveEraseDisplayAll();
            } else {
                iface.eraseDisplayAll();
            }
            break;
        case 3:
            if (!selective) {
                iface.eraseScrollback();
            }
            break;
        default:
            break;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchEraseLine(bool selective) {
    switch (parameter(0)) {
        case 0:
            if (selective) {
                iface.selectiveEraseLineAfter();
            } else {
                iface.eraseLineAfter();
            }
            break;
        case 1:
            if (selective) {
                iface.selectiveEraseLineBefore();
            } else {
                iface.eraseLineBefore();
            }
            break;
        case 2:
            if (selective) {
                iface.selectiveEraseLineAll();
            } else {
                iface.eraseLineAll();
            }
            break;
        default:
            break;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchTabClear() {
    if (parameter(0) == 0) {
        iface.clearTabStop();
    } else if (parameter(0) == 3) {
        iface.clearAllTabStops();
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchCursorStyle() {
    using Style = TerminalCursor::Style;
    switch (parameter(0)) {
        case 0:
            iface.setCursorStyle(1, Style::filled_block, true);
            break;
        case 1:
            iface.setCursorStyle(1, Style::filled_block, true);
            break;
        case 2:
            iface.setCursorStyle(2, Style::filled_block, false);
            break;
        case 3:
            iface.setCursorStyle(3, Style::underline, true);
            break;
        case 4:
            iface.setCursorStyle(4, Style::underline, false);
            break;
        case 5:
            iface.setCursorStyle(5, Style::bar, true);
            break;
        case 6:
            iface.setCursorStyle(6, Style::bar, false);
            break;
        default:
            iface.refreshCursorStyle();
            break;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchStandardMode(u32 mode, bool enabled) {
    switch (mode) {
        case 2:
            iface.setKeyboardLocked(enabled);
            break;
        case 4:
            iface.setInsertMode(enabled);
            break;
        case 6:
            iface.setEraseModeAll(enabled);
            break;
        case 12:
            iface.setLocalEcho(!enabled);
            break;
        case 20:
            iface.setAutoNewline(enabled);
            break;
        default:
            break;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchStandardModes(bool set) {
    for (size_t index = 0; index < parser.parameterCount; ++index) {
        dispatchStandardMode(parser.parameters[index], set);
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchPrivateMode(u32 mode, bool enabled) {
    switch (mode) {
        case 1:
            iface.setApplicationCursorKeys(enabled);
            break;
        case 2:
            iface.setAnsiMode(enabled);
            break;
        case 3:
            iface.setColumn132(enabled);
            break;
        case 4:
            iface.setSmoothScroll(enabled);
            break;
        case 5:
            iface.setScreenReverseVideo(enabled);
            break;
        case 6:
            iface.setOriginMode(enabled);
            break;
        case 7:
            iface.setAutoWrap(enabled);
            break;
        case 8:
            iface.setAutoRepeat(enabled);
            break;
        case 9:
            iface.setMouseTracking(enabled ? MouseTrackingMode::X10_Compat : MouseTrackingMode::Disabled);
            break;
        case 12:
            iface.setCursorBlink(enabled);
            break;
        case 25:
            iface.setCursorVisible(enabled);
            break;
        case 40:
            iface.setAllowColumnMode(enabled);
            break;
        case 41:
            iface.setMoreFix(enabled);
            break;
        case 42:
            iface.setNationalReplacement(enabled);
            break;
        case 45:
            iface.setReverseWrap(enabled);
            break;
        case 47:
            iface.setAlternateScreen(enabled, false);
            break;
        case 66:
            iface.parserSetApplicationKeypad(enabled);
            break;
        case 67:
            iface.setBackspaceSendsBackspace(enabled);
            break;
        case 69:
            iface.setHorizontalMargins(enabled);
            break;
        case 95:
            iface.setNoClearColumn(enabled);
            break;
        case 1000:
            iface.setMouseTracking(enabled ? MouseTrackingMode::VT200 : MouseTrackingMode::Disabled);
            break;
        case 1001:
            iface.setMouseTracking(enabled ? MouseTrackingMode::VT200_Highlight : MouseTrackingMode::Disabled);
            break;
        case 1002:
            iface.setMouseTracking(enabled ? MouseTrackingMode::VT200_ButtonEvent : MouseTrackingMode::Disabled);
            break;
        case 1003:
            iface.setMouseTracking(enabled ? MouseTrackingMode::VT200_AnyEvent : MouseTrackingMode::Disabled);
            break;
        case 1004:
            iface.setFocusEvents(enabled);
            break;
        case 1005:
            iface.setMouseEncoding(MouseTrackingEnc::UTF8, enabled);
            break;
        case 1006:
            iface.setMouseEncoding(MouseTrackingEnc::SGR, enabled);
            break;
        case 1007:
            iface.setAlternateScroll(enabled);
            break;
        case 1015:
            iface.setMouseEncoding(MouseTrackingEnc::URXVT, enabled);
            break;
        case 1016:
            iface.setMouseEncoding(MouseTrackingEnc::SGRPixels, enabled);
            break;
        case 1034:
            iface.setEightBitInput(enabled);
            break;
        case 1036:
        case 1039:
            iface.setAltSendsEscape(enabled);
            break;
        case 1045:
            iface.setExtendedReverseWrap(enabled);
            break;
        case 1047:
            iface.setAlternateScreen(enabled, !enabled);
            break;
        case 1048:
            if (enabled) {
                iface.esc_DECSC();
            } else {
                iface.esc_DECRC();
            }
            break;
        case 1049:
            iface.setSavedAlternateScreen(enabled);
            break;
        case 2004:
            iface.setBracketedPaste(enabled);
            break;
        case 2026:
            iface.setSynchronizedOutput(enabled);
            break;
        case 2027:
            iface.setGraphemeCluster(enabled);
            break;
        case 2031:
            iface.setColorSchemeUpdates(enabled);
            break;
        case 2048:
            iface.setInBandResize(enabled);
            break;
        case 5522:
            iface.setPasteMimeNotifications(enabled);
            break;
        default:
            break;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchPrivateModes(bool set) {
    for (size_t index = 0; index < parser.parameterCount; ++index) {
        dispatchPrivateMode(parser.parameters[index], set);
    }
}

template <bool traced>
bool ParserImpl<traced>::privateModeValue(u32 mode, const ParserModeState& state, bool& value) const {
    switch (mode) {
        case 1:
            value = state.applicationCursorKeys;
            return true;
        case 2:
            value = state.ansiMode;
            return true;
        case 3:
            value = state.column132;
            return true;
        case 4:
            value = state.smoothScroll;
            return true;
        case 5:
            value = state.screenReverseVideo;
            return true;
        case 6:
            value = state.originMode;
            return true;
        case 7:
            value = state.autoWrap;
            return true;
        case 8:
            value = state.autoRepeat;
            return true;
        case 9:
            value = state.mouseTracking == MouseTrackingMode::X10_Compat;
            return true;
        case 12:
            value = state.cursorBlink;
            return true;
        case 25:
            value = state.showCursor;
            return true;
        case 40:
            value = state.allowColumnMode;
            return true;
        case 41:
            value = state.moreFix;
            return true;
        case 42:
            value = state.nationalReplacement;
            return true;
        case 45:
            value = state.reverseWrap;
            return true;
        case 47:
        case 1047:
        case 1049:
            value = state.alternateScreen;
            return true;
        case 66:
            value = state.applicationKeypad;
            return true;
        case 67:
            value = state.backspaceSendsBackspace;
            return true;
        case 69:
            value = state.horizontalMargins;
            return true;
        case 95:
            value = state.noClearColumn;
            return true;
        case 1000:
            value = state.mouseTracking == MouseTrackingMode::VT200;
            return true;
        case 1001:
            value = state.mouseTracking == MouseTrackingMode::VT200_Highlight;
            return true;
        case 1002:
            value = state.mouseTracking == MouseTrackingMode::VT200_ButtonEvent;
            return true;
        case 1003:
            value = state.mouseTracking == MouseTrackingMode::VT200_AnyEvent;
            return true;
        case 1004:
            value = state.focusEvents;
            return true;
        case 1005:
            value = state.mouseEncoding == MouseTrackingEnc::UTF8;
            return true;
        case 1006:
            value = state.mouseEncoding == MouseTrackingEnc::SGR;
            return true;
        case 1007:
            value = state.alternateScroll;
            return true;
        case 1015:
            value = state.mouseEncoding == MouseTrackingEnc::URXVT;
            return true;
        case 1016:
            value = state.mouseEncoding == MouseTrackingEnc::SGRPixels;
            return true;
        case 1034:
            value = state.eightBitInput;
            return true;
        case 1036:
        case 1039:
            value = state.altSendsEscape;
            return true;
        case 1045:
            value = state.extendedReverseWrap;
            return true;
        case 1048:
            value = state.savedCursor;
            return true;
        case 2004:
            value = state.bracketedPaste;
            return true;
        case 2026:
            value = state.synchronizedOutput;
            return true;
        case 2027:
            value = state.graphemeCluster;
            return true;
        case 2031:
            value = state.colorSchemeUpdates;
            return true;
        case 2048:
            value = state.inBandResize;
            return true;
        case 5522:
            value = state.pasteMimeNotifications;
            return true;
        default:
            return false;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchKittyClipboard(StringView payload) {
    StringView metadata = payload;
    StringView encodedPayload;
    (void)payload.split(';', metadata, encodedPayload);

    StringView type;
    StringView id;
    StringView encodedMime;
    bool primary = false;
    bool valid = true;

    while (!metadata.empty()) {
        StringView record = metadata;
        StringView rest;
        if (metadata.split(':', record, rest)) {
            metadata = rest;
        } else {
            metadata = {};
        }
        if (record.empty()) {
            continue;
        }
        StringView key;
        StringView value;
        if (!record.split('=', key, value)) {
            valid = false;
            continue;
        }
        if (key == StringView(u8"type")) {
            type = value;
        } else if (key == StringView(u8"id")) {
            id = value;
        } else if (key == StringView(u8"loc")) {
            primary = value == StringView(u8"primary");
        } else if (key == StringView(u8"mime")) {
            encodedMime = value;
        }
    }

    if (type == StringView(u8"read")) {
        size_t decodedSize = encodedPayload.length();
        valid = valid && base64DecodeInPlace((u8*)(encodedPayload.data()), decodedSize);
        iface.osc_KITTY_CLIPBOARD_READ(id, StringView(encodedPayload.data(), decodedSize), primary, valid);
        return;
    }
    if (type == StringView(u8"write")) {
        if (valid) {
            iface.osc_KITTY_CLIPBOARD_WRITE(id, primary);
        } else {
            iface.osc_KITTY_CLIPBOARD_INVALID(id, true);
        }
        return;
    }

    StringView mime;
    size_t mimeSize = encodedMime.length();
    valid = valid && base64DecodeInPlace((u8*)(encodedMime.data()), mimeSize);
    mime = StringView(encodedMime.data(), mimeSize);

    size_t decodedSize = encodedPayload.length();
    valid = valid && base64DecodeInPlace((u8*)(encodedPayload.data()), decodedSize);
    const StringView decoded(encodedPayload.data(), decodedSize);
    if (type == StringView(u8"wdata")) {
        iface.osc_KITTY_CLIPBOARD_WRITE_DATA(id, mime, decoded, valid);
    } else if (type == StringView(u8"walias")) {
        iface.osc_KITTY_CLIPBOARD_WRITE_ALIAS(id, mime, decoded, valid);
    } else {
        iface.osc_KITTY_CLIPBOARD_INVALID(id, false);
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchPrivateSave() {
    const ParserModeState state = iface.parserModeState();
    for (size_t index = 0; index < parser.parameterCount; ++index) {
        const u32 mode = parser.parameters[index];
        if (mode == 2 || mode == 1048 || mode == 1049) {
            continue;
        }
        bool enabled = false;
        if (privateModeValue(mode, state, enabled)) {
            iface.savePrivateMode(mode, enabled);
        }
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchPrivateRestore() {
    for (size_t index = 0; index < parser.parameterCount; ++index) {
        const u32 mode = parser.parameters[index];
        bool enabled;
        if (iface.restorePrivateMode(mode, enabled)) {
            dispatchPrivateMode(mode, enabled);
        }
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchModeReport(bool privateMode) {
    const CompatibilityLevel compatibility = iface.parserCompatibilityLevel();
    if (compatibility < CompatibilityLevel::VT300) {
        return;
    }
    const u32 mode = parameter(0);
    const ParserModeState state = iface.parserModeState();
    u8 result = 0;
    bool enabled;
    if (privateMode) {
        if (privateModeValue(mode, state, enabled)) {
            if ((mode == 69 && compatibility < CompatibilityLevel::VT400) || (mode == 95 && compatibility < CompatibilityLevel::VT500)) {
                result = 0;
            } else {
                result = enabled ? 1 : 2;
            }
        } else if (mode == 60 || mode == 61 || mode == 64 || mode == 68 || mode == 73 || (mode == 81 && compatibility >= CompatibilityLevel::VT400) || ((mode == 34 || mode == 35 || mode == 36 || mode == 57 || (mode >= 96 && mode <= 104) || mode == 106) && compatibility >= CompatibilityLevel::VT500)) {
            result = 4;
        }
    } else {
        switch (mode) {
            case 2:
                result = state.keyboardLocked ? 1 : 2;
                break;
            case 4:
                result = state.insertMode ? 1 : 2;
                break;
            case 6:
                result = state.eraseModeAll ? 1 : 2;
                break;
            case 12:
                result = state.localEcho ? 2 : 1;
                break;
            case 20:
                result = state.autoNewline ? 1 : 2;
                break;
            case 1:
            case 3:
            case 5:
            case 7:
            case 10:
            case 11:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
                result = 4;
                break;
            default:
                break;
        }
    }
    iface.reportMode(mode, privateMode, result);
}

template <bool traced>
void ParserImpl<traced>::dispatchDecfra() {
    if (iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
        iface.csi_DECFRA(parameter(0), rectangle(1));
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchDeccra() {
    iface.csi_DECCRA(rectangle(0), countParameter(5), countParameter(6));
}

template <bool traced>
void ParserImpl<traced>::dispatchDecera(bool selective) {
    iface.csi_DECERA(rectangle(0), selective);
}

template <bool traced>
void ParserImpl<traced>::dispatchDeccara(bool reverse) {
    const CsiRectangle area = rectangle(0);
    CellAttributeChange change;
    if (!reverse) {
        struct Sink {
            CellAttributeChange& change;

            void sgrReset() {
                change.set(CellAttributeChange::Bold | CellAttributeChange::Faint | CellAttributeChange::Italic | CellAttributeChange::Underline | CellAttributeChange::Blink | CellAttributeChange::Inverse | CellAttributeChange::Conceal | CellAttributeChange::Strike | CellAttributeChange::Overline, false);
                change.setUnderline(0);
                change.setForeground(CellColor::defaultForeground());
                change.setBackground(CellColor::defaultBackground());
                change.setUnderlineFromForeground();
            }

            void sgrBold(bool enabled) {
                change.set(CellAttributeChange::Bold, enabled);
            }

            void sgrFaint(bool enabled) {
                change.set(CellAttributeChange::Faint, enabled);
            }

            void sgrItalic(bool enabled) {
                change.set(CellAttributeChange::Italic, enabled);
            }

            void sgrUnderline(u8 style) {
                change.setUnderline(style);
            }

            void sgrBlink(bool enabled) {
                change.set(CellAttributeChange::Blink, enabled);
            }

            void sgrInverse(bool enabled) {
                change.set(CellAttributeChange::Inverse, enabled);
            }

            void sgrConceal(bool enabled) {
                change.set(CellAttributeChange::Conceal, enabled);
            }

            void sgrStrike(bool enabled) {
                change.set(CellAttributeChange::Strike, enabled);
            }

            void sgrOverline(bool enabled) {
                change.set(CellAttributeChange::Overline, enabled);
            }

            void sgrForeground(CellColor color, int, bool) {
                change.setForeground(color);
            }

            void sgrDefaultForeground() {
                change.setForeground(CellColor::defaultForeground());
            }

            void sgrBackground(CellColor color, int) {
                change.setBackground(color);
            }

            void sgrDefaultBackground() {
                change.setBackground(CellColor::defaultBackground());
            }

            void sgrUnderlineColor(CellColor color, int) {
                change.setUnderlineColor(color);
            }

            void sgrDefaultUnderlineColor() {
                change.setUnderlineFromForeground();
            }

            void sgrFinish() {
            }
        } sink{change};

        if (parser.parameterCount <= 4) {
            sink.sgrReset();
            sink.sgrFinish();
        } else {
            dispatchSgrTo(sink, 4);
        }
        if (!change.empty()) {
            iface.changeRectangleAttributes(area, change);
        }
        return;
    }

    const auto enable = [&change](u16 bit) {
        change.toggle(bit);
    };
    const size_t end = parser.parameterCount > 4 ? parser.parameterCount : 5;
    for (size_t index = 4; index < end; ++index) {
        switch (parameter(index)) {
            case 0:
                change.toggle(CellAttributeChange::Bold | CellAttributeChange::Underline | CellAttributeChange::Blink | CellAttributeChange::Inverse);
                break;
            case 1:
                enable(CellAttributeChange::Bold);
                break;
            case 4:
                enable(CellAttributeChange::Underline);
                break;
            case 5:
                enable(CellAttributeChange::Blink);
                break;
            case 7:
                enable(CellAttributeChange::Inverse);
                break;
            case 8:
                enable(CellAttributeChange::Conceal);
                break;
            default:
                break;
        }
    }
    if (!change.empty()) {
        iface.changeRectangleAttributes(area, change);
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchDecrqcra() {
    // The four rectangle coordinates are independently optional. In
    // particular, CSI Pi * y asks for the whole page.
    if (parser.parameterCount >= 1) {
        iface.csi_DECRQCRA(parameter(0), rectangle(2));
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchDecll() {
    for (size_t index = 0; index < parser.parameterCount; ++index) {
        const u32 operation = parser.parameters[index];
        if (operation == 0) {
            iface.resetLeds();
        } else if (operation >= 1 && operation <= 3) {
            iface.setLed(operation - 1, true);
        } else if (operation >= 21 && operation <= 23) {
            iface.setLed(operation - 21, false);
        }
    }
    iface.commitLeds();
}

template <bool traced>
void ParserImpl<traced>::dispatchLocatorReporting() {
    const u32 mode = parameter(0);
    iface.setLocatorReporting(mode == 1 || mode == 2, mode == 2, parameter(1) == 1);
}

template <bool traced>
void ParserImpl<traced>::dispatchXtqmodkeys() {
    const u32 resource = parameter(0);
    if (resource <= 4 || resource == 6 || resource == 7) {
        iface.reportModifyKeyResource(resource);
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchKittyKeyboardSet() {
    const u8 flags = parameter(0) & 0x1f;
    switch (parser.parameterCount > 1 ? parameter(1) : 1) {
        case 1:
            iface.setKittyKeyboardFlags(flags);
            break;
        case 2:
            iface.addKittyKeyboardFlags(flags);
            break;
        case 3:
            iface.removeKittyKeyboardFlags(flags);
            break;
        default:
            break;
    }
}

template <bool traced>
Charset ParserImpl<traced>::decodeCharset(u16 id, bool is96) const {
    const u8 mod = id >> 8;
    const u8 final = id;
    Charset charset = Charset::UTF8;
    if (is96) {
        if (mod == 0) {
            if (final == 'A') {
                charset = Charset::IsoLatin1;
            } else if (final == '<') {
                charset = Charset::DecUserPref;
            }
        }
    } else if (mod == 0) {
        switch (final) {
            case 'A':
                charset = Charset::IsoUK;
                break;
            case '0':
                charset = Charset::DecSpec;
                break;
            case '5':
            case 'C':
                charset = Charset::NrcFinnish;
                break;
            case '<':
                charset = Charset::DecUserPref;
                break;
            case '>':
                charset = Charset::DecTechn;
                break;
            case '4':
                charset = Charset::NrcDutch;
                break;
            case 'R':
            case 'f':
                charset = Charset::NrcFrench;
                break;
            case '9':
            case 'Q':
                charset = Charset::NrcFrenchCanadian;
                break;
            case 'K':
                charset = Charset::NrcGerman;
                break;
            case 'Y':
                charset = Charset::NrcItalian;
                break;
            case '`':
            case 'E':
            case '6':
                charset = Charset::NrcNorwegianDanish;
                break;
            case 'Z':
                charset = Charset::NrcSpanish;
                break;
            case '7':
            case 'H':
                charset = Charset::NrcSwedish;
                break;
            case '=':
                charset = Charset::NrcSwiss;
                break;
        }
    } else if (mod == '%') {
        switch (final) {
            case '0':
            case '2':
                charset = Charset::NrcTurkish;
                break;
            case '3':
                charset = Charset::NrcSerboCroatian;
                break;
            case '5':
                charset = Charset::DecSuppl;
                break;
            case '6':
                charset = Charset::NrcPortuguese;
                break;
            case '=':
                charset = Charset::NrcHebrew;
                break;
        }
    } else if (mod == '&' && (final == '4' || final == '5')) {
        charset = Charset::NrcRussian;
    } else if (mod == '"') {
        if (final == '?' || final == '>') {
            charset = Charset::NrcGreek;
        } else if (final == '4') {
            charset = Charset::NrcHebrew;
        }
    }
    return charset;
}

template <bool traced>
void ParserImpl<traced>::designateCharset(u8 final) {
    if (parser.scsMultibyte) {
        return;
    }
    const u16 id = parser.scsMod == 0 ? final : ((u16)(parser.scsMod) << 8) | final;
    iface.parserDesignateCharset(parser.scsIndex, decodeCharset(id, parser.scs96), id, parser.scs96);
}

template <bool traced>
void ParserImpl<traced>::dispatchDsr(bool privateMode) {
    const u32 operation = parameter(0);
    if (!privateMode) {
        if (operation == 5) {
            iface.dsrOperatingStatus();
        } else if (operation == 6) {
            iface.dsrCursorPosition(false);
        }
        return;
    }
    switch (operation) {
        case 6:
            iface.dsrCursorPosition(true);
            break;
        case 15:
            iface.dsrPrinter();
            break;
        case 25:
            iface.dsrUserDefinedKeys();
            break;
        case 26:
            iface.dsrKeyboard();
            break;
        case 55:
            iface.dsrLocator();
            break;
        case 56:
            iface.dsrLocatorType();
            break;
        case 62:
            iface.dsrMacroSpace();
            break;
        case 63:
            iface.dsrMemoryChecksum(parameter(1));
            break;
        case 75:
            iface.dsrDataIntegrity();
            break;
        case 85:
            iface.dsrMultipleSession();
            break;
        case 996:
            iface.dsrColorScheme();
            break;
        default:
            break;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchTitleMode(bool set) {
    if (!parser.csiHadParameters) {
        iface.resetTitleModes();
        return;
    }
    for (size_t index = 0; index < parser.parameterCount; ++index) {
        const u32 mode = parser.parameters[index];
        if (mode <= 3) {
            iface.setTitleMode(1 << mode, set);
        }
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchDecscl() {
    CompatibilityLevel level;
    switch (parameter(0)) {
        case 61:
            level = CompatibilityLevel::VT100;
            break;
        case 62:
            level = CompatibilityLevel::VT200;
            break;
        case 63:
            level = CompatibilityLevel::VT300;
            break;
        case 64:
            level = CompatibilityLevel::VT400;
            break;
        case 65:
            level = CompatibilityLevel::VT500;
            break;
        default:
            return;
    }
    const u32 controlMode = parameter(1);
    if (controlMode <= 2) {
        iface.csi_DECSCL(level, level != CompatibilityLevel::VT100 && controlMode != 1);
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchWindowOps() {
    if (!iface.windowOperationsAllowed()) {
        return;
    }
    const u32 operation = parameter(0);
    const u32 first = parameter(1);
    const u32 second = parameter(2);
    const bool firstPresent = parser.parameterCount > 1 && parser.present[1];
    const bool secondPresent = parser.parameterCount > 2 && parser.present[2];
    switch (operation) {
        case 4:
            iface.xtResizePixels(first, firstPresent, second, secondPresent);
            break;
        case 8:
            iface.xtResizeCells(first, firstPresent, second, secondPresent);
            break;
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 9:
        case 10:
            iface.xtWindowOperation(operation, first, second);
            break;
        case 11:
            iface.xtReportWindowState();
            break;
        case 13:
            iface.xtReportWindowPosition();
            break;
        case 14:
            iface.xtReportWindowPixelSize(firstPresent && first == 2);
            break;
        case 15:
            iface.xtReportScreenPixelSize();
            break;
        case 16:
            iface.xtReportCellSize();
            break;
        case 18:
            iface.xtReportGridSize();
            break;
        case 19:
            iface.xtReportScreenGridSize();
            break;
        case 20:
            iface.xtReportIconTitle();
            break;
        case 21:
            iface.xtReportWindowTitle();
            break;
        case 22:
            if (first <= 2) {
                iface.xtPushTitle(first != 2, first != 1);
            }
            break;
        case 23:
            if (first <= 2) {
                iface.xtPopTitle(first != 2, first != 1);
            }
            break;
        default:
            if (operation >= 24) {
                iface.xtResizeRows(operation);
            }
            break;
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchDecsle() {
    for (size_t index = 0; index < parser.parameterCount; ++index) {
        switch (parser.parameters[index]) {
            case 0:
                iface.resetLocatorEvents();
                break;
            case 1:
                iface.setLocatorButtonDown(true);
                break;
            case 2:
                iface.setLocatorButtonDown(false);
                break;
            case 3:
                iface.setLocatorButtonUp(true);
                break;
            case 4:
                iface.setLocatorButtonUp(false);
                break;
            default:
                break;
        }
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchDecac() {
    if (parser.parameterCount != 1 && parser.parameterCount != 3) {
        return;
    }
    if (!parser.present[0]) {
        return;
    }
    const u32 item = parser.parameters[0];
    if (parser.parameterCount == 1) {
        if (item == 1) {
            iface.csi_DECAC_TEXT_RESET();
        } else if (item == 2) {
            iface.csi_DECAC_FRAME_RESET();
        }
        return;
    }
    if (!parser.present[1] || !parser.present[2] || parser.separators[1] != ';' || parser.separators[2] != ';' || parser.parameters[1] > 255 || parser.parameters[2] > 255) {
        return;
    }
    const u8 foreground = (u8)(parser.parameters[1]);
    const u8 background = (u8)(parser.parameters[2]);
    if (item == 1) {
        iface.csi_DECAC_TEXT(foreground, background);
    } else if (item == 2) {
        iface.csi_DECAC_FRAME(foreground, background);
    }
}

template <bool traced>
void ParserImpl<traced>::dispatchXtmodkeys() {
    if (!parser.csiHadParameters) {
        iface.resetModifyKeyResources();
        return;
    }
    const u32 resource = parameter(0);
    if (resource > 4 && resource != 6 && resource != 7) {
        return;
    }
    const bool valuePresent = parser.parameterCount > 1;
    const u32 value = parameter(1);
    const u32 maximum = resource == 4 ? 2 : 4;
    if (!valuePresent || value <= maximum) {
        iface.setModifyKeyResource(resource, value, !valuePresent);
    }
}

template <bool traced>
bool ParserImpl<traced>::parseSgrColor(size_t& index, CellColor& color, int& paletteIndex) {
    if (index + 1 >= parser.parameterCount) {
        return false;
    }
    const bool colon = parser.separators[index + 1] == ':';
    const u32 mode = parser.parameters[++index];
    if (colon) {
        const size_t first = index + 1;
        size_t end = index;
        while (end + 1 < parser.parameterCount && parser.separators[end + 1] == ':') {
            ++end;
        }
        index = end;

        if (mode == 5) {
            if (end - first + 1 != 1 || parser.parameters[first] > 255) {
                return false;
            }
            paletteIndex = parser.parameters[first];
            color = CellColor::indexed(paletteIndex);
            return true;
        }
        const size_t count = end - first + 1;
        const size_t rgbFirst = first + (count >= 4);
        if (mode != 2 || count < 3 || parser.parameters[rgbFirst] > 255 || parser.parameters[rgbFirst + 1] > 255 || parser.parameters[rgbFirst + 2] > 255) {
            return false;
        }
        paletteIndex = -1;
        color = CellColor::direct({
            (u8)(parser.parameters[rgbFirst]),
            (u8)(parser.parameters[rgbFirst + 1]),
            (u8)(parser.parameters[rgbFirst + 2]),
        });
        return true;
    }

    if (mode == 5) {
        if (index + 1 >= parser.parameterCount) {
            return false;
        }
        const u32 value = parser.parameters[++index];
        if (value > 255) {
            return false;
        }
        paletteIndex = value;
        color = CellColor::indexed(value);
        return true;
    }
    if (mode != 2) {
        return false;
    }

    const size_t first = index + 1;
    const size_t available = parser.parameterCount - first;
    index += min<size_t>(available, 3);
    if (available < 3 || parser.parameters[first] > 255 || parser.parameters[first + 1] > 255 || parser.parameters[first + 2] > 255) {
        return false;
    }
    paletteIndex = -1;
    color = CellColor::direct({
        (u8)(parser.parameters[first]),
        (u8)(parser.parameters[first + 1]),
        (u8)(parser.parameters[first + 2]),
    });
    index = first + 2;
    return true;
}

template <bool traced>
template <typename Sink>
void ParserImpl<traced>::dispatchSgrTo(Sink& sink, size_t first) {
    for (size_t index = first; index < parser.parameterCount; ++index) {
        const u32 attribute = parser.parameters[index];
        switch (attribute) {
            case 0:
                sink.sgrReset();
                break;
            case 1:
                sink.sgrBold(true);
                break;
            case 2:
                sink.sgrFaint(true);
                break;
            case 3:
                sink.sgrItalic(true);
                break;
            case 4:
                if (index + 1 < parser.parameterCount && parser.separators[index + 1] == ':') {
                    const u32 style = parser.parameters[++index];
                    if (style <= 5) {
                        sink.sgrUnderline(style);
                    }
                } else {
                    sink.sgrUnderline(1);
                }
                break;
            case 5:
            case 6:
                sink.sgrBlink(true);
                break;
            case 7:
                sink.sgrInverse(true);
                break;
            case 8:
                sink.sgrConceal(true);
                break;
            case 9:
                sink.sgrStrike(true);
                break;
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
                break;
            case 21:
                sink.sgrUnderline(2);
                break;
            case 22:
                sink.sgrBold(false);
                sink.sgrFaint(false);
                break;
            case 23:
                sink.sgrItalic(false);
                break;
            case 24:
                sink.sgrUnderline(0);
                break;
            case 25:
                sink.sgrBlink(false);
                break;
            case 27:
                sink.sgrInverse(false);
                break;
            case 28:
                sink.sgrConceal(false);
                break;
            case 29:
                sink.sgrStrike(false);
                break;
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
                sink.sgrForeground(CellColor::indexed(attribute - 30), attribute - 30, true);
                break;
            case 38: {
                CellColor color{};
                int paletteIndex;
                if (parseSgrColor(index, color, paletteIndex)) {
                    sink.sgrForeground(color, paletteIndex, false);
                }
            } break;
            case 39:
                sink.sgrDefaultForeground();
                break;
            case 40:
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
                sink.sgrBackground(CellColor::indexed(attribute - 40), attribute - 40);
                break;
            case 48: {
                CellColor color{};
                int paletteIndex;
                if (parseSgrColor(index, color, paletteIndex)) {
                    sink.sgrBackground(color, paletteIndex);
                }
            } break;
            case 49:
                sink.sgrDefaultBackground();
                break;
            case 53:
                sink.sgrOverline(true);
                break;
            case 55:
                sink.sgrOverline(false);
                break;
            case 58: {
                CellColor color{};
                int paletteIndex;
                if (parseSgrColor(index, color, paletteIndex)) {
                    sink.sgrUnderlineColor(color, paletteIndex);
                }
            } break;
            case 59:
                sink.sgrDefaultUnderlineColor();
                break;
            case 90:
            case 91:
            case 92:
            case 93:
            case 94:
            case 95:
            case 96:
            case 97:
                sink.sgrForeground(CellColor::indexed(attribute - 82), attribute - 82, false);
                break;
            case 100:
            case 101:
            case 102:
            case 103:
            case 104:
            case 105:
            case 106:
            case 107:
                sink.sgrBackground(CellColor::indexed(attribute - 92), attribute - 92);
                break;
            default:
                break;
        }
    }
    sink.sgrFinish();
}

template <bool traced>
void ParserImpl<traced>::dispatchSgr() {
    dispatchSgrTo(iface, 0);
}

template <bool traced>
void ParserImpl<traced>::traceCsi(u8 finalByte) {
    if constexpr (traced) {
        parserTrace->csi(finalByte, StringView(&parser.csiPrefix, parser.csiPrefix == 0 ? 0 : 1), StringView(parser.csiIntermediates, parser.csiIntermediateCount), parser.parameters, parser.separators, parser.parameterCount, parser.csiHadParameters);
    }
}

template <bool traced>
void ParserImpl<traced>::feed(StringView bytes) {
    const u8* p = bytes.data();
    const u8* const pe = p + bytes.length();
    const u8* const eof = nullptr;
    int& cs = parser.state;
    while (p != pe) {
        if (cs == parser_en_main) {
            const u8 current = *p;
            if (current == 0) {
                iface.parserResetGraphemeInput();
                p += zeroPrefix(p, pe - p);
                continue;
            }
            if (current < 0x20 && current != 0x1b) {
                groundControl(current);
                ++p;
                continue;
            }
            if (current == 0x7f) {
                ++p;
                continue;
            }
            if (parser.groundUtf8Remaining == 0 && current >= 0x80 && iface.parserUtf8BulkEligible()) {
                u8 pendingTrace = 0;
                const size_t consumed = iface.parserPlaceUtf8Run(StringView(p, pe - p), pendingTrace);
                if (consumed > 0) {
                    parser.groundUtf8Remaining = pendingTrace;
                    if constexpr (traced) {
                        parserTrace->text(p, consumed);
                    }
                    p += consumed;
                    continue;
                }
            }
            if (current >= 0x80 && current <= 0x9f && iface.parserGroundUtf8Enabled()) {
                ragelGroundHigh(current);
                ++p;
                continue;
            }
            if (parser.groundUtf8Remaining == 0 && current >= 0x20 && current < 0x7f) {
                const size_t consumed = iface.parserPlaceAscii(StringView(p, pe - p));
                if (consumed != 0) {
                    if constexpr (traced) {
                        const u8* trace = p;
                        const u8* const traceEnd = p + consumed;
                        while (trace != traceEnd) {
                            const u8* carriageReturn = (const u8*)memchr(trace, '\r', traceEnd - trace);
                            if (carriageReturn == nullptr) {
                                parserTrace->text(trace, traceEnd - trace);
                                break;
                            }
                            parserTrace->text(trace, carriageReturn - trace);
                            parserTrace->control('\r');
                            trace = carriageReturn + 1;
                            if (trace != traceEnd && *trace == '\n') {
                                parserTrace->control('\n');
                                ++trace;
                            }
                        }
                    }
                    p += consumed;
                    if (p + 1 < pe && p[0] == '\r' && p[1] == '\n') {
                        if constexpr (traced) {
                            parserTrace->control('\r');
                            parserTrace->control('\n');
                        }
                        iface.parserResetGraphemeInput();
                        iface.inp_CR();
                        if (iface.parserAutoNewlineMode()) {
                            iface.inp_CR();
                        }
                        iface.esc_IND();
                        p += 2;
                    }
                    continue;
                }
            }
        }

#define SHITTY_PARSER_EXEC
#include SHITTY_PARSER_GENERATED
#undef SHITTY_PARSER_EXEC
    }
}

Parser* Parser::create(ObjPool* pool, ParserIface& iface, VtermTrace* trace, bool osc52SelectClipboard) {
#if defined(SHITTY_FOR_TESTS)
    if (trace) {
        return pool->make<ParserImpl<true>>(iface, trace, osc52SelectClipboard);
    }
#endif
    return pool->make<ParserImpl<false>>(iface, trace, osc52SelectClipboard);
}

#undef SHITTY_PARSER_GENERATED
