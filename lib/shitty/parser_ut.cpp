/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include <lib/vterm/parser.h>
#include <lib/vterm/terminal_types.h>

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/lib/buffer.h>
#include <std/mem/obj_pool.h>

#include <cstdio>
#include <cstring>

using namespace stl;
using namespace plt;

namespace {
    struct ParserCall {
        const char* name;
        i64 values[24];
        size_t valueCount;
        size_t textOffsets[3];
        size_t textLengths[3];
    };

    struct RecordingParserIface final: public ParserIface {
        template <typename T>
        static i64 value(T input) {
            return (i64)(input);
        }

        void appendValues(ParserCall&) const {
        }

        template <typename T, typename... Rest>
        void appendValues(ParserCall& call, T first, Rest... rest) const {
            call.values[call.valueCount++] = value(first);
            appendValues(call, rest...);
        }

        template <typename... Values>
        ParserCall& record(const char* name, Values... values) const {
            STD_INSIST(callCount < sizeof(calls) / sizeof(calls[0]));
            ParserCall& call = calls[callCount++];
            call = {};
            call.name = name;
            appendValues(call, values...);
            return call;
        }

        void saveText(ParserCall& call, size_t index, StringView text) const {
            STD_INSIST(index < sizeof(call.textOffsets) / sizeof(call.textOffsets[0]));
            call.textOffsets[index] = strings.used();
            call.textLengths[index] = text.length();
            strings.append(text.data(), text.length());
        }

        void resetCalls() {
            callCount = 0;
            strings.reset();
        }

        const ParserCall& find(const char* name, size_t valueCount = (size_t)-1) const {
            for (size_t index = 0; index < callCount; ++index) {
                const ParserCall& call = calls[index];
                if (StringView(call.name) == StringView(name) && (valueCount == (size_t)-1 || call.valueCount == valueCount)) {
                    return call;
                }
            }
            STD_INSIST(false);
            return calls[0];
        }

        bool called(const char* name) const {
            for (size_t index = 0; index < callCount; ++index) {
                if (StringView(calls[index].name) == StringView(name)) {
                    return true;
                }
            }
            return false;
        }

        StringView text(const ParserCall& call, size_t index = 0) const {
            return StringView((const u8*)(strings.data()) + call.textOffsets[index], call.textLengths[index]);
        }

        void parserResetGraphemeInput() override {
            record("parserResetGraphemeInput");
        }

        void parserBell() override {
            record("parserBell");
        }

        bool parserAutoNewlineMode() const override {
            record("parserAutoNewlineMode", autoNewlineMode);
            return autoNewlineMode;
        }

        CompatibilityLevel parserCompatibilityLevel() const override {
            record("parserCompatibilityLevel", compatibilityLevel);
            return compatibilityLevel;
        }

        void parserSetCompatibilityLevel(CompatibilityLevel level) override {
            record("parserSetCompatibilityLevel", level);
            compatibilityLevel = level;
        }

        void parserSet8BitControls(bool enabled) override {
            record("parserSet8BitControls", enabled);
        }

        void parserSetApplicationKeypad(bool enabled) override {
            record("parserSetApplicationKeypad", enabled);
        }

        void parserMoveCursorBackward(u32 count) override {
            record("parserMoveCursorBackward", count);
        }

        bool parserHexTitleInput() const override {
            record("parserHexTitleInput", hexTitleInput);
            return hexTitleInput;
        }

        void parserSingleShift(u8 index) override {
            record("parserSingleShift", index);
        }

        void parserLockingShiftGl(u8 index) override {
            record("parserLockingShiftGl", index);
        }

        void parserLockingShiftGr(u8 index) override {
            record("parserLockingShiftGr", index);
        }

        void parserResetCharsets(bool isoLatin1) override {
            record("parserResetCharsets", isoLatin1);
        }

        void parserDesignateCharset(u8 index, Charset charset, u16 id, bool is96) override {
            record("parserDesignateCharset", index, charset, id, is96);
        }

        bool parserHighlightMouseTracking() const override {
            record("parserHighlightMouseTracking", highlightMouseTracking);
            return highlightMouseTracking;
        }

        bool windowOperationsAllowed() const override {
            record("windowOperationsAllowed", allowWindowOperations);
            return allowWindowOperations;
        }

        void parserWritePty(StringView bytes) override {
            ParserCall& call = record("parserWritePty");
            saveText(call, 0, bytes);
        }

        bool parserGroundUtf8Enabled() const override {
            record("parserGroundUtf8Enabled", groundUtf8Enabled);
            return groundUtf8Enabled;
        }

        void parserGroundHigh(u8 byte) override {
            record("parserGroundHigh", byte);
        }

        void parserGroundAscii(u8 byte) override {
            record("parserGroundAscii", byte);
        }

        bool parserUtf8BulkEligible() const override {
            record("parserUtf8BulkEligible", utf8BulkEligible);
            return utf8BulkEligible;
        }

        size_t parserPlaceAscii(StringView bytes) override {
            ParserCall& call = record("parserPlaceAscii", asciiConsumed);
            saveText(call, 0, bytes);
            return asciiConsumed;
        }

        size_t parserPlaceUtf8Run(StringView bytes, u8& pendingTrace) override {
            pendingTrace = 0;
            ParserCall& call = record("parserPlaceUtf8Run", utf8RunConsumed);
            saveText(call, 0, bytes);
            return utf8RunConsumed;
        }

        void unhandledInput(unsigned char byte) override {
            record("unhandledInput", byte);
        }

        void inp_CR() override {
            record("inp_CR");
        }

        void inp_HT() override {
            record("inp_HT");
        }

        bool esc_IND() override {
            record("esc_IND", indexResult);
            return indexResult;
        }

        void esc_RI() override {
            record("esc_RI");
        }

        void esc_NEL() override {
            record("esc_NEL");
        }

        void esc_BI() override {
            record("esc_BI");
        }

        void esc_FI() override {
            record("esc_FI");
        }

        void esc_HTS() override {
            record("esc_HTS");
        }

        void esc_SPA() override {
            record("esc_SPA");
        }

        void esc_EPA() override {
            record("esc_EPA");
        }

        void esc_DECSC() override {
            record("esc_DECSC");
        }

        void esc_DECRC() override {
            record("esc_DECRC");
        }

        void esc_RIS() override {
            record("esc_RIS");
        }

        void csi_DECSTR() override {
            record("csi_DECSTR");
        }

        bool horizontalMarginMode() const override {
            record("horizontalMarginMode", horizontalMargins);
            return horizontalMargins;
        }

        void csi_SLRM(u32 left, u32 right, bool valid) override {
            record("csi_SLRM", left, right, valid);
        }

        void csi_SCORC() override {
            record("csi_SCORC");
        }

#define RECORD_U32_METHOD(method, parameter) \
    void method(u32 parameter) override {    \
        record(#method, parameter);          \
    }

        RECORD_U32_METHOD(csi_CUU, count)
        RECORD_U32_METHOD(csi_CUD, count)
        RECORD_U32_METHOD(csi_CUF, count)
        RECORD_U32_METHOD(csi_CUB, count)
        RECORD_U32_METHOD(csi_CNL, count)
        RECORD_U32_METHOD(csi_CPL, count)
        RECORD_U32_METHOD(csi_CHA, column)
        RECORD_U32_METHOD(csi_HPA, column)
        RECORD_U32_METHOD(csi_HPR, count)
        RECORD_U32_METHOD(csi_VPA, row)
        RECORD_U32_METHOD(csi_VPR, count)

#undef RECORD_U32_METHOD

        void csi_CUP(u32 row, u32 column) override {
            record("csi_CUP", row, column);
        }

#define RECORD_COUNT_METHOD(method)   \
    void method(u32 count) override { \
        record(#method, count);       \
    }

        RECORD_COUNT_METHOD(csi_SU)
        RECORD_COUNT_METHOD(csi_SD)
        RECORD_COUNT_METHOD(csi_CHT)
        RECORD_COUNT_METHOD(csi_CBT)
        RECORD_COUNT_METHOD(csi_REP)
        RECORD_COUNT_METHOD(csi_ICH)

#undef RECORD_COUNT_METHOD

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(eraseDisplayAfter)
        RECORD_VOID_METHOD(eraseDisplayBefore)
        RECORD_VOID_METHOD(eraseDisplayAll)
        RECORD_VOID_METHOD(eraseScrollback)
        RECORD_VOID_METHOD(eraseLineAfter)
        RECORD_VOID_METHOD(eraseLineBefore)
        RECORD_VOID_METHOD(eraseLineAll)
        RECORD_VOID_METHOD(selectiveEraseDisplayAfter)
        RECORD_VOID_METHOD(selectiveEraseDisplayBefore)
        RECORD_VOID_METHOD(selectiveEraseDisplayAll)
        RECORD_VOID_METHOD(selectiveEraseLineAfter)
        RECORD_VOID_METHOD(selectiveEraseLineBefore)
        RECORD_VOID_METHOD(selectiveEraseLineAll)

#undef RECORD_VOID_METHOD

        void setDecProtection(bool enabled) override {
            record("setDecProtection", enabled);
        }

        void csi_DECFRA(u32 codepoint, CsiRectangle rectangle) override {
            record("csi_DECFRA", codepoint, rectangle.top, rectangle.left, rectangle.bottom, rectangle.right);
        }

        void csi_DECCRA(CsiRectangle source, u32 targetRow, u32 targetColumn) override {
            record("csi_DECCRA", source.top, source.left, source.bottom, source.right, targetRow, targetColumn);
        }

        void csi_DECERA(CsiRectangle rectangle, bool selective) override {
            record("csi_DECERA", rectangle.top, rectangle.left, rectangle.bottom, rectangle.right, selective);
        }

        void setAttributeChangeExtent(bool rectangular) override {
            record("setAttributeChangeExtent", rectangular);
        }

        void changeRectangleAttributes(CsiRectangle rectangle, CellAttributeChange change) override {
            lastAttributeChange = change;
            record("changeRectangleAttributes", rectangle.top, rectangle.left, rectangle.bottom, rectangle.right, change.setMask, change.clearMask, change.toggleMask);
        }

        void csi_XTCHECKSUM(u32 flags) override {
            record("csi_XTCHECKSUM", flags);
        }

        void csi_DECRQCRA(u32 requestId, CsiRectangle rectangle) override {
            record("csi_DECRQCRA", requestId, rectangle.top, rectangle.left, rectangle.bottom, rectangle.right);
        }

#define RECORD_COUNT_METHOD(method)   \
    void method(u32 count) override { \
        record(#method, count);       \
    }

        RECORD_COUNT_METHOD(csi_IL)
        RECORD_COUNT_METHOD(csi_DL)
        RECORD_COUNT_METHOD(csi_DCH)
        RECORD_COUNT_METHOD(csi_ECH)
        RECORD_COUNT_METHOD(csi_DECIC)
        RECORD_COUNT_METHOD(csi_DECDC)

#undef RECORD_COUNT_METHOD

        void csi_STBM(u32 top, u32 bottom, bool valid) override {
            record("csi_STBM", top, bottom, valid);
        }

        void clearTabStop() override {
            record("clearTabStop");
        }

        void clearAllTabStops() override {
            record("clearAllTabStops");
        }

        void resetTabStops() override {
            record("resetTabStops");
        }

        ParserModeState parserModeState() const override {
            record("parserModeState");
            return modeState;
        }

#define RECORD_BOOL_METHOD(method)       \
    void method(bool enabled) override { \
        record(#method, enabled);        \
    }

        RECORD_BOOL_METHOD(setKeyboardLocked)
        RECORD_BOOL_METHOD(setInsertMode)
        RECORD_BOOL_METHOD(setEraseModeAll)
        RECORD_BOOL_METHOD(setLocalEcho)
        RECORD_BOOL_METHOD(setAutoNewline)
        RECORD_BOOL_METHOD(setAnsiMode)
        RECORD_BOOL_METHOD(setApplicationCursorKeys)
        RECORD_BOOL_METHOD(setColumn132)
        RECORD_BOOL_METHOD(setSmoothScroll)
        RECORD_BOOL_METHOD(setScreenReverseVideo)
        RECORD_BOOL_METHOD(setOriginMode)
        RECORD_BOOL_METHOD(setAutoWrap)
        RECORD_BOOL_METHOD(setAutoRepeat)
        RECORD_BOOL_METHOD(setAllowColumnMode)
        RECORD_BOOL_METHOD(setMoreFix)
        RECORD_BOOL_METHOD(setNationalReplacement)
        RECORD_BOOL_METHOD(setReverseWrap)

#undef RECORD_BOOL_METHOD

        void setMouseTracking(MouseTrackingMode mode) override {
            record("setMouseTracking", mode);
        }

#define RECORD_BOOL_METHOD(method)       \
    void method(bool enabled) override { \
        record(#method, enabled);        \
    }

        RECORD_BOOL_METHOD(setCursorBlink)
        RECORD_BOOL_METHOD(setCursorVisible)

#undef RECORD_BOOL_METHOD

        void setAlternateScreen(bool enabled, bool clear) override {
            record("setAlternateScreen", enabled, clear);
        }

#define RECORD_BOOL_METHOD(method)       \
    void method(bool enabled) override { \
        record(#method, enabled);        \
    }

        RECORD_BOOL_METHOD(setBackspaceSendsBackspace)
        RECORD_BOOL_METHOD(setHorizontalMargins)
        RECORD_BOOL_METHOD(setNoClearColumn)
        RECORD_BOOL_METHOD(setFocusEvents)

#undef RECORD_BOOL_METHOD

        void setMouseEncoding(MouseTrackingEnc encoding, bool enabled) override {
            record("setMouseEncoding", encoding, enabled);
        }

#define RECORD_BOOL_METHOD(method)       \
    void method(bool enabled) override { \
        record(#method, enabled);        \
    }

        RECORD_BOOL_METHOD(setAlternateScroll)
        RECORD_BOOL_METHOD(setEightBitInput)
        RECORD_BOOL_METHOD(setAltSendsEscape)
        RECORD_BOOL_METHOD(setSavedAlternateScreen)
        RECORD_BOOL_METHOD(setExtendedReverseWrap)
        RECORD_BOOL_METHOD(setBracketedPaste)
        RECORD_BOOL_METHOD(setSynchronizedOutput)
        RECORD_BOOL_METHOD(setGraphemeCluster)
        RECORD_BOOL_METHOD(setColorSchemeUpdates)
        RECORD_BOOL_METHOD(setInBandResize)
        RECORD_BOOL_METHOD(setPasteMimeNotifications)

#undef RECORD_BOOL_METHOD

        void savePrivateMode(u32 mode, bool enabled) override {
            record("savePrivateMode", mode, enabled);
        }

        bool restorePrivateMode(u32 mode, bool& enabled) const override {
            record("restorePrivateMode", mode, restoreModeFound, restoreModeValue);
            enabled = restoreModeValue;
            return restoreModeFound;
        }

        void reportMode(u32 mode, bool privateMode, u8 state) override {
            record("reportMode", mode, privateMode, state);
        }

        void csi_ecma48_SL(u32 count) override {
            record("csi_ecma48_SL", count);
        }

        void csi_ecma48_SR(u32 count) override {
            record("csi_ecma48_SR", count);
        }

        void setCursorStyle(u8 reportStyle, TerminalCursor::Style shape, bool blink) override {
            record("setCursorStyle", reportStyle, shape, blink);
        }

        void refreshCursorStyle() override {
            record("refreshCursorStyle");
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(csi_priDA)
        RECORD_VOID_METHOD(csi_secDA)
        RECORD_VOID_METHOD(csi_terDA)
        RECORD_VOID_METHOD(csi_DECRQDE)
        RECORD_VOID_METHOD(dsrOperatingStatus)
        RECORD_VOID_METHOD(dsrPrinter)

#undef RECORD_VOID_METHOD

        void csi_DECREQTPARM(u32 permission) override {
            record("csi_DECREQTPARM", permission);
        }

        void csi_DECRQTSR_COLOR(u32 model) override {
            record("csi_DECRQTSR_COLOR", model);
        }

        void csi_DECRQPSR_TABS() override {
            record("csi_DECRQPSR_TABS");
        }

        void csi_DECRQPSR_CURSOR() override {
            record("csi_DECRQPSR_CURSOR");
        }

        void csi_DECRQUPSS() override {
            record("csi_DECRQUPSS");
        }

        void dsrCursorPosition(bool privateMode) override {
            record("dsrCursorPosition", privateMode);
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(dsrUserDefinedKeys)
        RECORD_VOID_METHOD(dsrKeyboard)
        RECORD_VOID_METHOD(dsrLocator)
        RECORD_VOID_METHOD(dsrLocatorType)
        RECORD_VOID_METHOD(dsrMacroSpace)

#undef RECORD_VOID_METHOD

        void dsrMemoryChecksum(u32 requestId) override {
            record("dsrMemoryChecksum", requestId);
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(dsrDataIntegrity)
        RECORD_VOID_METHOD(dsrMultipleSession)
        RECORD_VOID_METHOD(dsrColorScheme)
        RECORD_VOID_METHOD(sgrReset)

#undef RECORD_VOID_METHOD

#define RECORD_BOOL_METHOD(method)       \
    void method(bool enabled) override { \
        record(#method, enabled);        \
    }

        RECORD_BOOL_METHOD(sgrBold)
        RECORD_BOOL_METHOD(sgrFaint)
        RECORD_BOOL_METHOD(sgrItalic)

#undef RECORD_BOOL_METHOD

        void sgrUnderline(u8 style) override {
            record("sgrUnderline", style);
        }

#define RECORD_BOOL_METHOD(method)       \
    void method(bool enabled) override { \
        record(#method, enabled);        \
    }

        RECORD_BOOL_METHOD(sgrBlink)
        RECORD_BOOL_METHOD(sgrInverse)
        RECORD_BOOL_METHOD(sgrConceal)
        RECORD_BOOL_METHOD(sgrStrike)
        RECORD_BOOL_METHOD(sgrOverline)

#undef RECORD_BOOL_METHOD

        void sgrForeground(CellColor color, int paletteIndex, bool brightenBold) override {
            record("sgrForeground", color.encoded(), paletteIndex, brightenBold);
        }

        void sgrDefaultForeground() override {
            record("sgrDefaultForeground");
        }

        void sgrBackground(CellColor color, int paletteIndex) override {
            record("sgrBackground", color.encoded(), paletteIndex);
        }

        void sgrDefaultBackground() override {
            record("sgrDefaultBackground");
        }

        void sgrUnderlineColor(CellColor color, int paletteIndex) override {
            record("sgrUnderlineColor", color.encoded(), paletteIndex);
        }

        void sgrDefaultUnderlineColor() override {
            record("sgrDefaultUnderlineColor");
        }

        void sgrFinish() override {
            record("sgrFinish");
        }

        void csi_XTPUSHSGR(const u32* attributes, size_t count) override {
            ParserCall& call = record("csi_XTPUSHSGR");
            for (size_t index = 0; index < count; ++index) {
                call.values[call.valueCount++] = attributes[index];
            }
        }

        void csi_XTPOPSGR() override {
            record("csi_XTPOPSGR");
        }

        void esch_DECALN() override {
            record("esch_DECALN");
        }

        void setLineAttribute(u8 attribute) override {
            record("setLineAttribute", attribute);
        }

#define RECORD_TEXT_METHOD(method)             \
    void method(StringView payload) override { \
        ParserCall& call = record(#method);    \
        saveText(call, 0, payload);            \
    }

        RECORD_TEXT_METHOD(osc_TITLE_0)
        RECORD_TEXT_METHOD(osc_TITLE_1)
        RECORD_TEXT_METHOD(osc_TITLE_2)

#undef RECORD_TEXT_METHOD

        void osc_PALETTE(u32 index, Color color, bool query) override {
            record("osc_PALETTE", index, color.red, color.green, color.blue, query);
        }

        void osc_SPECIAL_COLOR(u32 index, Color color, bool query) override {
            record("osc_SPECIAL_COLOR", index, color.red, color.green, color.blue, query);
        }

        void osc_SPECIAL_COLOR_MODE(u32 index, u32 mode) override {
            record("osc_SPECIAL_COLOR_MODE", index, mode);
        }

        void osc_RAW(u32 command, StringView payload) override {
            ParserCall& call = record("osc_RAW", command);
            saveText(call, 0, payload);
        }

        void osc_CWD(StringView path, bool valid) override {
            ParserCall& call = record("osc_CWD", valid);
            saveText(call, 0, path);
        }

        void osc_HYPERLINK(StringView id, bool hasId, StringView uri) override {
            ParserCall& call = record("osc_HYPERLINK", hasId);
            saveText(call, 0, id);
            saveText(call, 1, uri);
        }

        void osc_NOTIFY(StringView payload) override {
            ParserCall& call = record("osc_NOTIFY");
            saveText(call, 0, payload);
        }

        void osc_PROGRESS(u32 state, u32 percent, bool percentPresent) override {
            record("osc_PROGRESS", state, percent, percentPresent);
        }

#define RECORD_COLOR_METHOD(method)                                 \
    void method(Color color, bool query) override {                 \
        record(#method, color.red, color.green, color.blue, query); \
    }

        RECORD_COLOR_METHOD(osc_DEFAULT_FOREGROUND)
        RECORD_COLOR_METHOD(osc_DEFAULT_BACKGROUND)
        RECORD_COLOR_METHOD(osc_CURSOR_COLOR)
        RECORD_COLOR_METHOD(osc_SELECTION_BACKGROUND)
        RECORD_COLOR_METHOD(osc_SELECTION_FOREGROUND)

#undef RECORD_COLOR_METHOD

        void osc_CLIPBOARD_QUERY(bool primary, bool clipboard, u8 replySelector, bool selectorsEmpty) override {
            record("osc_CLIPBOARD_QUERY", primary, clipboard, replySelector, selectorsEmpty);
        }

        void osc_CLIPBOARD_WRITE(StringView content, bool valid, bool primary, bool clipboard) override {
            ParserCall& call = record("osc_CLIPBOARD_WRITE", valid, primary, clipboard);
            saveText(call, 0, content);
        }

        void osc_KITTY_CLIPBOARD_READ(StringView id, StringView mimeTypes, bool primary, bool valid) override {
            ParserCall& call = record("osc_KITTY_CLIPBOARD_READ", primary, valid);
            saveText(call, 0, id);
            saveText(call, 1, mimeTypes);
        }

        void osc_KITTY_CLIPBOARD_WRITE(StringView id, bool primary) override {
            ParserCall& call = record("osc_KITTY_CLIPBOARD_WRITE", primary);
            saveText(call, 0, id);
        }

        void osc_KITTY_CLIPBOARD_WRITE_DATA(StringView id, StringView mimeType, StringView content, bool valid) override {
            ParserCall& call = record("osc_KITTY_CLIPBOARD_WRITE_DATA", valid);
            saveText(call, 0, id);
            saveText(call, 1, mimeType);
            saveText(call, 2, content);
        }

        void osc_KITTY_CLIPBOARD_WRITE_ALIAS(StringView id, StringView mimeType, StringView aliases, bool valid) override {
            ParserCall& call = record("osc_KITTY_CLIPBOARD_WRITE_ALIAS", valid);
            saveText(call, 0, id);
            saveText(call, 1, mimeType);
            saveText(call, 2, aliases);
        }

        void osc_KITTY_CLIPBOARD_INVALID(StringView id, bool write) override {
            ParserCall& call = record("osc_KITTY_CLIPBOARD_INVALID", write);
            saveText(call, 0, id);
        }

#define RECORD_TEXT_METHOD(method, parameter)    \
    void method(StringView parameter) override { \
        ParserCall& call = record(#method);      \
        saveText(call, 0, parameter);            \
    }

        RECORD_TEXT_METHOD(osc_NOTIFICATION_CAPABILITIES, payload)
        RECORD_TEXT_METHOD(osc_NOTIFICATION_CLOSE, id)

#undef RECORD_TEXT_METHOD

        void osc_NOTIFICATION_TITLE(StringView id, StringView content, bool encoded, bool final) override {
            ParserCall& call = record("osc_NOTIFICATION_TITLE", encoded, final);
            saveText(call, 0, id);
            saveText(call, 1, content);
        }

        void osc_NOTIFICATION_BODY(StringView id, StringView content, bool encoded, bool final) override {
            ParserCall& call = record("osc_NOTIFICATION_BODY", encoded, final);
            saveText(call, 0, id);
            saveText(call, 1, content);
        }

        void osc_RESET_PALETTE() override {
            record("osc_RESET_PALETTE");
        }

        void osc_RESET_PALETTE(u32 index) override {
            record("osc_RESET_PALETTE", index);
        }

        void osc_RESET_SPECIAL_COLOR() override {
            record("osc_RESET_SPECIAL_COLOR");
        }

        void osc_RESET_SPECIAL_COLOR(u32 index) override {
            record("osc_RESET_SPECIAL_COLOR", index);
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(osc_RESET_DEFAULT_FOREGROUND)
        RECORD_VOID_METHOD(osc_RESET_DEFAULT_BACKGROUND)
        RECORD_VOID_METHOD(osc_RESET_CURSOR_COLOR)
        RECORD_VOID_METHOD(osc_RESET_SELECTION_BACKGROUND)
        RECORD_VOID_METHOD(osc_RESET_SELECTION_FOREGROUND)

#undef RECORD_VOID_METHOD

#define RECORD_TEXT_METHOD(method)             \
    void method(StringView payload) override { \
        ParserCall& call = record(#method);    \
        saveText(call, 0, payload);            \
    }

        RECORD_TEXT_METHOD(osc_SHELL_A)
        RECORD_TEXT_METHOD(osc_SHELL_B)
        RECORD_TEXT_METHOD(osc_SHELL_C)
        RECORD_TEXT_METHOD(osc_SHELL_D)
        RECORD_TEXT_METHOD(osc_SHELL_I)
        RECORD_TEXT_METHOD(osc_SHELL_L)
        RECORD_TEXT_METHOD(osc_SHELL_N)
        RECORD_TEXT_METHOD(osc_SHELL_P)
        RECORD_TEXT_METHOD(osc_SHELL_UNKNOWN)

#undef RECORD_TEXT_METHOD

        void osc_UNKNOWN(u32 command, StringView payload) override {
            ParserCall& call = record("osc_UNKNOWN", command);
            saveText(call, 0, payload);
        }

        void csi_DECSCL(CompatibilityLevel level, bool send8BitControls) override {
            record("csi_DECSCL", level, send8BitControls);
        }

        void xtResizePixels(u32 height, bool heightPresent, u32 width, bool widthPresent) override {
            record("xtResizePixels", height, heightPresent, width, widthPresent);
        }

        void xtResizeCells(u32 height, bool heightPresent, u32 width, bool widthPresent) override {
            record("xtResizeCells", height, heightPresent, width, widthPresent);
        }

        void xtWindowOperation(u32 operation, u32 first, u32 second) override {
            record("xtWindowOperation", operation, first, second);
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(xtReportWindowState)
        RECORD_VOID_METHOD(xtReportWindowPosition)

#undef RECORD_VOID_METHOD

        void xtReportWindowPixelSize(bool compositorSize) override {
            record("xtReportWindowPixelSize", compositorSize);
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(xtReportScreenPixelSize)
        RECORD_VOID_METHOD(xtReportCellSize)
        RECORD_VOID_METHOD(xtReportGridSize)
        RECORD_VOID_METHOD(xtReportScreenGridSize)
        RECORD_VOID_METHOD(xtReportIconTitle)
        RECORD_VOID_METHOD(xtReportWindowTitle)

#undef RECORD_VOID_METHOD

        void xtPushTitle(bool icon, bool window) override {
            record("xtPushTitle", icon, window);
        }

        void xtPopTitle(bool icon, bool window) override {
            record("xtPopTitle", icon, window);
        }

        void xtResizeRows(u32 rows) override {
            record("xtResizeRows", rows);
        }

        void resetTitleModes() override {
            record("resetTitleModes");
        }

        void setTitleMode(u8 bit, bool enabled) override {
            record("setTitleMode", bit, enabled);
        }

        void csi_XTHIMOUSE(u32 start, u32 startX, u32 startY, u32 firstRow, u32 lastRow) override {
            record("csi_XTHIMOUSE", start, startX, startY, firstRow, lastRow);
        }

        void setLocatorReporting(bool enabled, bool oneShot, bool pixels) override {
            record("setLocatorReporting", enabled, oneShot, pixels);
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(resetLocatorEvents)

#undef RECORD_VOID_METHOD

        void setLocatorButtonDown(bool enabled) override {
            record("setLocatorButtonDown", enabled);
        }

        void setLocatorButtonUp(bool enabled) override {
            record("setLocatorButtonUp", enabled);
        }

        void csi_DECRQLP() override {
            record("csi_DECRQLP");
        }

        void csi_DECAC_TEXT(u8 foreground, u8 background) override {
            record("csi_DECAC_TEXT", foreground, background);
        }

        void csi_DECAC_TEXT_RESET() override {
            record("csi_DECAC_TEXT_RESET");
        }

        void csi_DECAC_FRAME(u8 foreground, u8 background) override {
            record("csi_DECAC_FRAME", foreground, background);
        }

        void csi_DECAC_FRAME_RESET() override {
            record("csi_DECAC_FRAME_RESET");
        }

        void csi_DECEFR(u32 top, u32 left, u32 bottom, u32 right) override {
            record("csi_DECEFR", top, left, bottom, right);
        }

        void resetModifyKeyResources() override {
            record("resetModifyKeyResources");
        }

        void setModifyKeyResource(u8 resource, u8 value, bool useDefault) override {
            record("setModifyKeyResource", resource, value, useDefault);
        }

        void reportModifyKeyResource(u8 resource) override {
            record("reportModifyKeyResource", resource);
        }

        void csi_kittyKeyboardPush(u32 flags) override {
            record("csi_kittyKeyboardPush", flags);
        }

        void csi_kittyKeyboardPop(u32 count) override {
            record("csi_kittyKeyboardPop", count);
        }

        void setKittyKeyboardFlags(u8 flags) override {
            record("setKittyKeyboardFlags", flags);
        }

        void addKittyKeyboardFlags(u8 flags) override {
            record("addKittyKeyboardFlags", flags);
        }

        void removeKittyKeyboardFlags(u8 flags) override {
            record("removeKittyKeyboardFlags", flags);
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(csi_kittyKeyboardQuery)
        RECORD_VOID_METHOD(csi_XTVERSION)

        void csi_XTSMGRAPHICS(u32 item, u32 action, u32 value) override {
            record("csi_XTSMGRAPHICS", item, action, value);
        }
        RECORD_VOID_METHOD(csi_SETMARK)
#undef RECORD_VOID_METHOD

        void resetLeds() override {
            record("resetLeds");
        }

        void setLed(u8 index, bool enabled) override {
            record("setLed", index, enabled);
        }

        void commitLeds() override {
            record("commitLeds");
        }

#define RECORD_VOID_METHOD(method) \
    void method() override {       \
        record(#method);           \
    }

        RECORD_VOID_METHOD(dcs_DECRQSS_DECSCL)
        RECORD_VOID_METHOD(dcs_DECRQSS_SGR)
        RECORD_VOID_METHOD(dcs_DECRQSS_DECSTBM)
        RECORD_VOID_METHOD(dcs_DECRQSS_DECSLRM)
        RECORD_VOID_METHOD(dcs_DECRQSS_DECSLPP)
        RECORD_VOID_METHOD(dcs_DECRQSS_DECSCUSR)
        RECORD_VOID_METHOD(dcs_DECRQSS_DECSCA)
        RECORD_VOID_METHOD(dcs_DECRQSS_DECSACE)
        RECORD_VOID_METHOD(dcs_DECRQSS_UNKNOWN)

#undef RECORD_VOID_METHOD

        void dcs_XTGETTCAP(StringView encoded, StringView value) override {
            ParserCall& call = record("dcs_XTGETTCAP");
            saveText(call, 0, encoded);
            saveText(call, 1, value);
        }

        void dcs_DECUDK(bool clearDefinitions, bool lockDefinitions, const ParserUdkDefinition* definitions, size_t definitionCount, StringView values) override {
            ParserCall& call = record("dcs_DECUDK", clearDefinitions, lockDefinitions, definitionCount, definitionCount == 0 ? 0 : definitions[0].valueOffset, definitionCount == 0 ? 0 : definitions[0].valueLength, definitionCount == 0 ? InputKey::Unknown : definitions[0].key);
            saveText(call, 0, values);
        }

        void dcs_DECRSTS_HLS(u32 index, u32 hue, u32 luminosity, u32 saturation) override {
            record("dcs_DECRSTS_HLS", index, hue, luminosity, saturation);
        }

        void dcs_DECRSTS_RGB(u32 index, u32 red, u32 green, u32 blue) override {
            record("dcs_DECRSTS_RGB", index, red, green, blue);
        }

        void dcs_DECRSTS_TABS_BEGIN() override {
            record("dcs_DECRSTS_TABS_BEGIN");
        }

        void dcs_DECRSTS_TAB(u32 column) override {
            record("dcs_DECRSTS_TAB", column);
        }

        void dcs_DECRSTS_CURSOR(u32 row, u32 column, u8 rendition, u8 protection, u8 flags, u8 gl, u8 gr, u8 sizeFlags, const Charset* charsets, const u16* charsetIds) override {
            ParserCall& call = record("dcs_DECRSTS_CURSOR", row, column, rendition, protection, flags, gl, gr, sizeFlags);
            for (size_t index = 0; index < 4; ++index) {
                call.values[call.valueCount++] = value(charsets[index]);
                call.values[call.valueCount++] = charsetIds[index];
            }
        }

        void dcs_DECAUPSS(Charset charset, u16 id, bool is96) override {
            record("dcs_DECAUPSS", charset, id, is96);
        }

        void dcs_SIXEL(const ParserSixelImage& image) override {
            ParserCall& call = record("dcs_SIXEL", image.width, image.height);
            // Snapshot the pixels tightly, row by row: the image only
            // lives for the duration of the call.
            call.textOffsets[0] = strings.used();
            for (u32 row = 0; row < image.height; ++row) {
                strings.append(image.pixels + (size_t)(row)*image.pitch, image.width);
            }
            call.textLengths[0] = (size_t)(image.width) * image.height;
            saveText(call, 1, StringView(image.palette, SixelPatch::paletteBytes));
        }

        mutable ParserCall calls[64]{};
        mutable size_t callCount = 0;
        mutable Buffer strings;

        bool autoNewlineMode = false;
        CompatibilityLevel compatibilityLevel = CompatibilityLevel::VT500;
        bool hexTitleInput = false;
        bool highlightMouseTracking = false;
        bool allowWindowOperations = true;
        bool groundUtf8Enabled = true;
        bool utf8BulkEligible = false;
        size_t asciiConsumed = 0;
        size_t utf8RunConsumed = 0;
        bool indexResult = false;
        bool horizontalMargins = false;
        ParserModeState modeState{};
        bool restoreModeFound = true;
        bool restoreModeValue = true;
        CellAttributeChange lastAttributeChange{};
    };

    struct ParserFixture {
        ParserFixture()
            : pool(ObjPool::fromMemory())
            , parser(Parser::create(pool.mutPtr(), iface, nullptr, false))
        {
        }

        void feed(StringView input) {
            parser->feed(input);
        }

        const ParserCall& expect(const char* name, size_t valueCount = (size_t)-1) const {
            return iface.find(name, valueCount);
        }

        ObjPool::Ref pool;
        RecordingParserIface iface;
        Parser* parser;
    };

    enum class VteKnownKind : u8 {
        ESCAPE,
        CSI,
        DCS,
    };

    struct VteKnownSequence {
        const char* command;
        VteKnownKind kind;
        u8 final;
        u8 prefix;
        u8 intermediate;
    };

#define VTE_PREFIX_NONE 0
#define VTE_PREFIX_EQUAL '='
#define VTE_PREFIX_GT '>'
#define VTE_PREFIX_WHAT '?'
#define VTE_INTERMEDIATE_NONE 0
#define VTE_INTERMEDIATE_SPACE ' '
#define VTE_INTERMEDIATE_BANG '!'
#define VTE_INTERMEDIATE_DQUOTE '"'
#define VTE_INTERMEDIATE_HASH '#'
#define VTE_INTERMEDIATE_CASH '$'
#define VTE_INTERMEDIATE_PERCENT '%'
#define VTE_INTERMEDIATE_AND '&'
#define VTE_INTERMEDIATE_SQUOTE '\''
#define VTE_INTERMEDIATE_PCLOSE ')'
#define VTE_INTERMEDIATE_MULT '*'
#define VTE_INTERMEDIATE_PLUS '+'
#define VTE_INTERMEDIATE_COMMA ','
#define VTE_INTERMEDIATE_MINUS '-'
#define VTE_KNOWN_ROW(command, kind, final, prefix, count, intermediate, nop) {#command, VteKnownKind::kind, final, VTE_PREFIX_##prefix, VTE_INTERMEDIATE_##intermediate},
#define _VTE_SEQ(command, kind, final, prefix, count, intermediate, flags) VTE_KNOWN_ROW(command, kind, final, prefix, count, intermediate, false)
#define _VTE_NOQ(command, kind, final, prefix, count, intermediate, flags) VTE_KNOWN_ROW(command, kind, final, prefix, count, intermediate, true)

    static constexpr VteKnownSequence vteKnownEscape[] = {
#include "tst/vte/upstream/parser-esc.hh"
    };
    static constexpr VteKnownSequence vteKnownCsi[] = {
#include "tst/vte/upstream/parser-csi.hh"
    };
    static constexpr VteKnownSequence vteKnownDcs[] = {
#include "tst/vte/upstream/parser-dcs.hh"
    };

#undef _VTE_NOQ
#undef _VTE_SEQ
#undef VTE_KNOWN_ROW
#undef VTE_INTERMEDIATE_MINUS
#undef VTE_INTERMEDIATE_COMMA
#undef VTE_INTERMEDIATE_PLUS
#undef VTE_INTERMEDIATE_MULT
#undef VTE_INTERMEDIATE_PCLOSE
#undef VTE_INTERMEDIATE_SQUOTE
#undef VTE_INTERMEDIATE_AND
#undef VTE_INTERMEDIATE_PERCENT
#undef VTE_INTERMEDIATE_CASH
#undef VTE_INTERMEDIATE_HASH
#undef VTE_INTERMEDIATE_DQUOTE
#undef VTE_INTERMEDIATE_BANG
#undef VTE_INTERMEDIATE_SPACE
#undef VTE_INTERMEDIATE_NONE
#undef VTE_PREFIX_WHAT
#undef VTE_PREFIX_GT
#undef VTE_PREFIX_EQUAL
#undef VTE_PREFIX_NONE

    struct VteKnownDispatch {
        const char* command;
        const char* callback;
    };

    static constexpr VteKnownDispatch vteKnownDispatches[] = {
        {"DECDHL_TH", "setLineAttribute"},
        {"DECDHL_BH", "setLineAttribute"},
        {"DECSWL", "setLineAttribute"},
        {"DECBI", "esc_BI"},
        {"DECDWL", "setLineAttribute"},
        {"DECSC", "esc_DECSC"},
        {"DECRC", "esc_DECRC"},
        {"DECALN", "esch_DECALN"},
        {"DECFI", "esc_FI"},
        {"DECANM", "parserSetCompatibilityLevel"},
        {"DECKPAM", "parserSetApplicationKeypad"},
        {"DECKPNM", "parserSetApplicationKeypad"},
        {"IND", "esc_IND"},
        {"NEL", "esc_NEL"},
        {"HTS", "esc_HTS"},
        {"RI", "esc_RI"},
        {"SS2", "parserSingleShift"},
        {"SS3", "parserSingleShift"},
        {"SPA", "esc_SPA"},
        {"EPA", "esc_EPA"},
        {"RIS", "esc_RIS"},
        {"LS2", "parserLockingShiftGl"},
        {"LS3", "parserLockingShiftGl"},
        {"LS3R", "parserLockingShiftGr"},
        {"LS2R", "parserLockingShiftGr"},
        {"LS1R", "parserLockingShiftGr"},
        {"ICH", "csi_ICH"},
        {"SL", "csi_ecma48_SL"},
        {"CUU", "csi_CUU"},
        {"SR", "csi_ecma48_SR"},
        {"CUD", "csi_CUD"},
        {"CUF", "csi_CUF"},
        {"CUB", "csi_CUB"},
        {"CNL", "csi_CNL"},
        {"CPL", "csi_CPL"},
        {"CHA", "csi_CHA"},
        {"CUP", "csi_CUP"},
        {"CHT", "csi_CHT"},
        {"ED", "eraseDisplayAfter"},
        {"DECSED", "selectiveEraseDisplayAfter"},
        {"EL", "eraseLineAfter"},
        {"DECSEL", "selectiveEraseLineAfter"},
        {"IL", "csi_IL"},
        {"DL", "csi_DL"},
        {"DCH", "csi_DCH"},
        {"SU", "csi_SU"},
        {"SD_OR_XTERM_IHMT", "csi_SD"},
        {"ECH", "csi_ECH"},
        {"CBT", "csi_CBT"},
        {"HPA", "csi_HPA"},
        {"HPR", "csi_HPR"},
        {"REP", "csi_REP"},
        {"DA1", "csi_priDA"},
        {"DA3", "csi_terDA"},
        {"DA2", "csi_secDA"},
        {"DECRQDE", "csi_DECRQDE"},
        {"DECREQTPARM_OR_WYCDIR", "csi_DECREQTPARM"},
        {"VPA", "csi_VPA"},
        {"VPR", "csi_VPR"},
        {"HVP", "csi_CUP"},
        {"TBC", "clearTabStop"},
        {"DECST8C", "resetTabStops"},
        {"HPB", "csi_CUB"},
        {"VPB", "csi_CUU"},
        {"SM_ECMA", "setInsertMode"},
        {"SM_DEC", "setAutoWrap"},
        {"RM_ECMA", "setInsertMode"},
        {"RM_DEC", "setAutoWrap"},
        {"SGR", "sgrReset"},
        {"DECSGR", "reportModifyKeyResource"},
        {"DSR_ECMA", "dsrOperatingStatus"},
        {"DSR_DEC", "dsrUserDefinedKeys"},
        {"DECSTR", "csi_DECSTR"},
        {"DECSCL", "csi_DECSCL"},
        {"DECRQM_ECMA", "reportMode"},
        {"DECRQM_DEC", "reportMode"},
        {"DECLL", "resetLeds"},
        {"DECSCUSR", "setCursorStyle"},
        {"DECSCA", "setDecProtection"},
        {"XTERM_VERSION", "csi_XTVERSION"},
        {"DECSTBM", "csi_STBM"},
        {"DECCARA", "changeRectangleAttributes"},
        {"DECSACE", "setAttributeChangeExtent"},
        {"DECPCTERM_OR_XTERM_RPM", "restorePrivateMode"},
        {"DECSLRM_OR_SCOSC", "esc_DECSC"},
        {"XTERM_SPM", "savePrivateMode"},
        {"DECRQUPSS", "csi_kittyKeyboardQuery"},
        {"DECSLPP_OR_XTERM_WM", "xtReportWindowState"},
        {"DECRARA", "changeRectangleAttributes"},
        {"XTERM_RTM", "resetTitleModes"},
        {"XTERM_STM", "setTitleMode"},
        {"XTERM_MODKEYS", "resetModifyKeyResources"},
        {"XTERM_CHECKSUM_MODE", "csi_XTCHECKSUM"},
        {"SCORC", "csi_SCORC"},
        {"DECCRA", "csi_DECCRA"},
        {"DECEFR", "csi_DECEFR"},
        {"DECFRA", "csi_DECFRA"},
        {"DECRQCRA", "csi_DECRQCRA"},
        {"DECERA", "csi_DECERA"},
        {"DECELR", "setLocatorReporting"},
        {"DECSLE", "resetLocatorEvents"},
        {"DECSERA", "csi_DECERA"},
        {"DECRQLP", "csi_DECRQLP"},
        {"DECIC", "csi_DECIC"},
        {"DECDC", "csi_DECDC"},
        {"DECRQSS", "dcs_DECRQSS_UNKNOWN"},
        {"XTERM_SMGRAPHICS", "csi_XTSMGRAPHICS"},
        {"XTERM_RQTCAP", "dcs_XTGETTCAP"},
        {"DECUDK", "dcs_DECUDK"},
    };

    static const char* vteKnownCallback(StringView command) {
        for (const VteKnownDispatch& dispatch : vteKnownDispatches) {
            if (command == StringView(dispatch.command)) {
                return dispatch.callback;
            }
        }
        return nullptr;
    }

    static bool vteKnownInfrastructureCall(const ParserCall& call) {
        const StringView name(call.name);
        return name == StringView(u8"parserResetGraphemeInput") || name == StringView(u8"parserCompatibilityLevel") || name == StringView(u8"unhandledInput");
    }

    static StringView vteKnownParameters(StringView command) {
        if (command == StringView(u8"SM_ECMA") || command == StringView(u8"RM_ECMA") || command == StringView(u8"DECRQM_ECMA")) {
            return StringView(u8"4");
        }
        if (command == StringView(u8"SM_DEC") || command == StringView(u8"RM_DEC") || command == StringView(u8"DECRQM_DEC") || command == StringView(u8"DECPCTERM_OR_XTERM_RPM") || command == StringView(u8"XTERM_SPM")) {
            return StringView(u8"7");
        }
        if (command == StringView(u8"DECSGR")) {
            return StringView(u8"1");
        }
        if (command == StringView(u8"DSR_ECMA")) {
            return StringView(u8"5");
        }
        if (command == StringView(u8"DSR_DEC")) {
            return StringView(u8"25");
        }
        if (command == StringView(u8"DECSCL")) {
            return StringView(u8"65;1");
        }
        if (command == StringView(u8"DECCARA") || command == StringView(u8"DECRARA")) {
            return StringView(u8"1;1;1;1;1");
        }
        if (command == StringView(u8"DECRQCRA")) {
            return StringView(u8"1;1;1;1;1;1");
        }
        if (command == StringView(u8"DECSLPP_OR_XTERM_WM")) {
            return StringView(u8"11");
        }
        if (command == StringView(u8"XTERM_STM")) {
            return StringView(u8"1");
        }
        if (command == StringView(u8"DECUDK")) {
            return StringView(u8"0;0");
        }
        return {};
    }

    static StringView vteKnownBody(StringView command) {
        if (command == StringView(u8"DECRQSS")) {
            return StringView(u8"z");
        }
        if (command == StringView(u8"XTERM_RQTCAP")) {
            return StringView(u8"544e");
        }
        if (command == StringView(u8"DECUDK")) {
            return StringView(u8"17/41");
        }
        return {};
    }

    static void feedVteKnown(ParserFixture& fixture, const VteKnownSequence& sequence) {
        u8 bytes[32];
        size_t count = 0;
        const StringView command(sequence.command);
        bytes[count++] = 0x1b;
        if (sequence.kind == VteKnownKind::CSI) {
            bytes[count++] = '[';
        } else if (sequence.kind == VteKnownKind::DCS) {
            bytes[count++] = 'P';
        }
        if (sequence.prefix != 0) {
            bytes[count++] = sequence.prefix;
        }
        const StringView parameters = vteKnownParameters(command);
        if (!parameters.empty()) {
            memcpy(bytes + count, parameters.data(), parameters.length());
        }
        count += parameters.length();
        if (sequence.intermediate != 0) {
            bytes[count++] = sequence.intermediate;
        }
        bytes[count++] = sequence.final;
        if (sequence.kind == VteKnownKind::DCS) {
            const StringView body = vteKnownBody(command);
            if (!body.empty()) {
                memcpy(bytes + count, body.data(), body.length());
            }
            count += body.length();
            bytes[count++] = 0x1b;
            bytes[count++] = '\\';
        }
        fixture.feed(StringView(bytes, count));
    }

    template <size_t count>
    static size_t checkVteKnown(const VteKnownSequence (&sequences)[count]) {
        size_t checked = 0;
        for (const VteKnownSequence& sequence : sequences) {
            const StringView command(sequence.command);
            const char* callback = vteKnownCallback(command);
            const bool countCallback = callback != nullptr;
            if (command == StringView(u8"XTERM_PUSHSGR") && sequence.final == '{') {
                callback = "csi_XTPUSHSGR";
            } else if (command == StringView(u8"XTERM_POPSGR") && sequence.final == '}') {
                callback = "csi_XTPOPSGR";
            }
            ParserFixture fixture;
            feedVteKnown(fixture, sequence);
            if (callback == nullptr) {
                for (size_t index = 0; index < fixture.iface.callCount; ++index) {
                    if (!vteKnownInfrastructureCall(fixture.iface.calls[index])) {
                        fprintf(stderr, "VTE known %s unexpectedly called %s\n", sequence.command, fixture.iface.calls[index].name);
                        STD_INSIST(false);
                    }
                }
                continue;
            }
            if (!fixture.iface.called(callback)) {
                fprintf(stderr, "VTE known %s did not call %s\n", sequence.command, callback);
                STD_INSIST(false);
            }
            checked += countCallback;
        }
        return checked;
    }

    static void expectValues(const ParserCall& call) {
        STD_INSIST(call.valueCount == 0);
    }

    template <typename First, typename... Rest>
    static void expectValues(const ParserCall& call, First first, Rest... rest) {
        constexpr size_t count = 1 + sizeof...(rest);
        STD_INSIST(call.valueCount == count);
        const i64 expected[count] = {(i64)(first), (i64)(rest)...};
        for (size_t index = 0; index < count; ++index) {
            STD_INSIST(call.values[index] == expected[index]);
        }
    }

    static void expectText(const RecordingParserIface& iface, const ParserCall& call, size_t index, StringView expected) {
        STD_INSIST(iface.text(call, index) == expected);
    }
}

#define SHITTY_PARSER_CALLBACK_TEST0(test, callback, input) \
    STD_TEST(test) {                                        \
        ParserFixture fixture;                              \
        fixture.feed(StringView(input));                    \
        expectValues(fixture.expect(#callback));            \
    }

#define SHITTY_PARSER_CALLBACK_TEST1(test, callback, input, first) \
    STD_TEST(test) {                                               \
        ParserFixture fixture;                                     \
        fixture.feed(StringView(input));                           \
        expectValues(fixture.expect(#callback), first);            \
    }

#define SHITTY_PARSER_CALLBACK_TEST2(test, callback, input, first, second) \
    STD_TEST(test) {                                                       \
        ParserFixture fixture;                                             \
        fixture.feed(StringView(input));                                   \
        expectValues(fixture.expect(#callback), first, second);            \
    }

#define SHITTY_PARSER_CALLBACK_TEST3(test, callback, input, first, second, third) \
    STD_TEST(test) {                                                              \
        ParserFixture fixture;                                                    \
        fixture.feed(StringView(input));                                          \
        expectValues(fixture.expect(#callback), first, second, third);            \
    }

#define SHITTY_PARSER_CALLBACK_TEST4(test, callback, input, first, second, third, fourth) \
    STD_TEST(test) {                                                                      \
        ParserFixture fixture;                                                            \
        fixture.feed(StringView(input));                                                  \
        expectValues(fixture.expect(#callback), first, second, third, fourth);            \
    }

#define SHITTY_PARSER_CALLBACK_TEST5(test, callback, input, first, second, third, fourth, fifth) \
    STD_TEST(test) {                                                                             \
        ParserFixture fixture;                                                                   \
        fixture.feed(StringView(input));                                                         \
        expectValues(fixture.expect(#callback), first, second, third, fourth, fifth);            \
    }

#define SHITTY_PARSER_TEXT_TEST(test, callback, input, expected)  \
    STD_TEST(test) {                                              \
        ParserFixture fixture;                                    \
        fixture.feed(StringView(input));                          \
        const ParserCall& call = fixture.expect(#callback);       \
        expectValues(call);                                       \
        expectText(fixture.iface, call, 0, StringView(expected)); \
    }

STD_TEST_SUITE(ParserCallbacks) {
    STD_TEST(ResetGraphemeInput) {
        ParserFixture fixture;
        const u8 input = 0;
        fixture.feed(StringView(&input, 1));
        expectValues(fixture.expect("parserResetGraphemeInput"));
    }
    SHITTY_PARSER_CALLBACK_TEST0(Bell, parserBell, u8"\a")
    SHITTY_PARSER_CALLBACK_TEST1(ReadAutoNewlineMode, parserAutoNewlineMode, u8"\n", false)
    SHITTY_PARSER_CALLBACK_TEST1(ReadCompatibilityLevel, parserCompatibilityLevel, u8"\x1b<", CompatibilityLevel::VT500)
    SHITTY_PARSER_CALLBACK_TEST1(SetCompatibilityLevel, parserSetCompatibilityLevel, u8"\x1b<", CompatibilityLevel::VT400)
    SHITTY_PARSER_CALLBACK_TEST1(Set8BitControls, parserSet8BitControls, u8"\x1b G", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetApplicationKeypad, parserSetApplicationKeypad, u8"\x1b=", true)
    SHITTY_PARSER_CALLBACK_TEST1(MoveCursorBackward, parserMoveCursorBackward, u8"\b", 1)
    SHITTY_PARSER_CALLBACK_TEST1(ReadHexTitleInput, parserHexTitleInput, u8"\x1b]0;x\a", false)
    SHITTY_PARSER_CALLBACK_TEST1(SingleShift, parserSingleShift, u8"\x1bN", 2)
    SHITTY_PARSER_CALLBACK_TEST1(LockingShiftGl, parserLockingShiftGl, u8"\x0e", 1)
    SHITTY_PARSER_CALLBACK_TEST1(LockingShiftGr, parserLockingShiftGr, u8"\x1b~", 1)
    SHITTY_PARSER_CALLBACK_TEST1(ResetCharsets, parserResetCharsets, u8"\x1b%G", false)
    SHITTY_PARSER_CALLBACK_TEST4(DesignateCharset, parserDesignateCharset, u8"\x1b(0", 0, Charset::DecSpec, '0', false)
    SHITTY_PARSER_CALLBACK_TEST1(ReadHighlightMouseTracking, parserHighlightMouseTracking, u8"\x1b[1;2;3;4;5T", false)
    SHITTY_PARSER_CALLBACK_TEST1(ReadWindowOperationsAllowed, windowOperationsAllowed, u8"\x1b[1t", true)

    STD_TEST(WritePty) {
        ParserFixture fixture;
        fixture.iface.compatibilityLevel = CompatibilityLevel::VT52;
        fixture.feed(StringView(u8"\x1bZ"));
        const ParserCall& call = fixture.expect("parserWritePty");
        expectValues(call);
        expectText(fixture.iface, call, 0, StringView(u8"\x1b/Z"));
    }

    STD_TEST(ReadGroundUtf8Mode) {
        ParserFixture fixture;
        const u8 input[] = {0xc2, 0xa2};
        fixture.feed(StringView(input, sizeof(input)));
        expectValues(fixture.expect("parserGroundUtf8Enabled"), true);
    }

    STD_TEST(PlaceGroundHighByte) {
        ParserFixture fixture;
        const u8 input[] = {0xc2, 0xa2};
        fixture.feed(StringView(input, sizeof(input)));
        expectValues(fixture.expect("parserGroundHigh"), 0xc2);
    }

    STD_TEST(TreatRawC1AsTextInUtf8Mode) {
        for (u16 value = 0x80; value <= 0x9f; ++value) {
            ParserFixture fixture;
            const u8 input = value;
            fixture.feed(StringView(&input, 1));
            expectValues(fixture.expect("parserGroundHigh"), input);
        }
    }

    STD_TEST(InterpretRawDeviceAttributesOutsideUtf8Mode) {
        ParserFixture fixture;
        fixture.iface.groundUtf8Enabled = false;
        const u8 input = 0x9a;
        fixture.feed(StringView(&input, 1));
        expectValues(fixture.expect("csi_priDA"));
    }

    STD_TEST(S8c1tDoesNotChangeInputEncoding) {
        ParserFixture fixture;
        const u8 input[] = {'\x1b', ' ', 'G', 0x9a};
        fixture.feed(StringView(input, sizeof(input)));
        expectValues(fixture.expect("parserSet8BitControls"), true);
        expectValues(fixture.expect("parserGroundHigh"), 0x9a);
        STD_INSIST(!fixture.iface.called("csi_priDA"));
    }

    STD_TEST(InterpretSevenBitControlInUtf8Mode) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bZ"));
        expectValues(fixture.expect("csi_priDA"));
    }

    STD_TEST(PreserveUtf8EncodedC1Codepoint) {
        ParserFixture fixture;
        const u8 input[] = {0xc2, 0x9a};
        fixture.feed(StringView(input, sizeof(input)));
        STD_INSIST(!fixture.iface.called("csi_priDA"));
        expectValues(fixture.expect("parserGroundHigh"), 0xc2);
    }

    SHITTY_PARSER_CALLBACK_TEST1(PlaceGroundAsciiByte, parserGroundAscii, u8"x", 'x')

    STD_TEST(ReadUtf8BulkEligibility) {
        ParserFixture fixture;
        const u8 input[] = {0xc2, 0xa2};
        fixture.feed(StringView(input, sizeof(input)));
        expectValues(fixture.expect("parserUtf8BulkEligible"), false);
    }

    STD_TEST(PlaceAscii) {
        ParserFixture fixture;
        fixture.iface.asciiConsumed = 3;
        fixture.feed(StringView(u8"a\r\n"));
        const ParserCall& call = fixture.expect("parserPlaceAscii");
        expectValues(call, 3);
        expectText(fixture.iface, call, 0, StringView(u8"a\r\n"));
    }

    STD_TEST(PlaceUtf8Run) {
        ParserFixture fixture;
        fixture.iface.utf8BulkEligible = true;
        fixture.iface.utf8RunConsumed = 2;
        const u8 input[] = {0xc2, 0xa2};
        fixture.feed(StringView(input, sizeof(input)));
        const ParserCall& call = fixture.expect("parserPlaceUtf8Run");
        expectValues(call, 2);
        expectText(fixture.iface, call, 0, StringView(input, sizeof(input)));
    }

    SHITTY_PARSER_CALLBACK_TEST1(UnhandledEscape, unhandledInput, u8"\x1b?", '?')
    SHITTY_PARSER_CALLBACK_TEST0(CarriageReturn, inp_CR, u8"\r")
    SHITTY_PARSER_CALLBACK_TEST0(HorizontalTab, inp_HT, u8"\t")
    SHITTY_PARSER_CALLBACK_TEST1(
        Index,
        esc_IND,
        u8"\x1b"
        "D",
        false
    )
    SHITTY_PARSER_CALLBACK_TEST0(ReverseIndex, esc_RI, u8"\x1bM")
    SHITTY_PARSER_CALLBACK_TEST0(
        NextLine,
        esc_NEL,
        u8"\x1b"
        "E"
    )
    SHITTY_PARSER_CALLBACK_TEST0(
        BackIndex,
        esc_BI,
        u8"\x1b"
        "6"
    )
    SHITTY_PARSER_CALLBACK_TEST0(
        ForwardIndex,
        esc_FI,
        u8"\x1b"
        "9"
    )
    SHITTY_PARSER_CALLBACK_TEST0(SetTabStop, esc_HTS, u8"\x1bH")
    SHITTY_PARSER_CALLBACK_TEST0(StartProtectedArea, esc_SPA, u8"\x1bV")
    SHITTY_PARSER_CALLBACK_TEST0(EndProtectedArea, esc_EPA, u8"\x1bW")
    SHITTY_PARSER_CALLBACK_TEST0(
        SaveCursor,
        esc_DECSC,
        u8"\x1b"
        "7"
    )
    SHITTY_PARSER_CALLBACK_TEST0(
        RestoreCursor,
        esc_DECRC,
        u8"\x1b"
        "8"
    )
    SHITTY_PARSER_CALLBACK_TEST0(
        ResetTerminal,
        esc_RIS,
        u8"\x1b"
        "c"
    )
    SHITTY_PARSER_CALLBACK_TEST0(SoftReset, csi_DECSTR, u8"\x1b[!p")
    SHITTY_PARSER_CALLBACK_TEST1(ReadHorizontalMarginMode, horizontalMarginMode, u8"\x1b[2;8s", false)

    STD_TEST(SetLeftAndRightMargins) {
        ParserFixture fixture;
        fixture.iface.horizontalMargins = true;
        fixture.feed(StringView(u8"\x1b[2;8s"));
        expectValues(fixture.expect("csi_SLRM"), 2, 8, true);
    }

    SHITTY_PARSER_CALLBACK_TEST0(RestoreCursorCsi, csi_SCORC, u8"\x1b[u")

    SHITTY_PARSER_CALLBACK_TEST1(CursorUp, csi_CUU, u8"\x1b[7A", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorDown, csi_CUD, u8"\x1b[7B", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorForward, csi_CUF, u8"\x1b[7C", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorBackward, csi_CUB, u8"\x1b[7D", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorNextLine, csi_CNL, u8"\x1b[7E", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorPreviousLine, csi_CPL, u8"\x1b[7F", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorHorizontalAbsolute, csi_CHA, u8"\x1b[7G", 7)
    SHITTY_PARSER_CALLBACK_TEST1(HorizontalPositionAbsolute, csi_HPA, u8"\x1b[7`", 7)
    SHITTY_PARSER_CALLBACK_TEST1(HorizontalPositionRelative, csi_HPR, u8"\x1b[7a", 7)
    SHITTY_PARSER_CALLBACK_TEST1(VerticalPositionAbsolute, csi_VPA, u8"\x1b[7d", 7)
    SHITTY_PARSER_CALLBACK_TEST1(VerticalPositionRelative, csi_VPR, u8"\x1b[7e", 7)
    SHITTY_PARSER_CALLBACK_TEST2(CursorPosition, csi_CUP, u8"\x1b[3;4H", 3, 4)
    SHITTY_PARSER_CALLBACK_TEST1(ScrollUp, csi_SU, u8"\x1b[7S", 7)
    SHITTY_PARSER_CALLBACK_TEST1(ScrollDown, csi_SD, u8"\x1b[7T", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorForwardTab, csi_CHT, u8"\x1b[7I", 7)
    SHITTY_PARSER_CALLBACK_TEST1(CursorBackwardTab, csi_CBT, u8"\x1b[7Z", 7)
    SHITTY_PARSER_CALLBACK_TEST1(RepeatCharacter, csi_REP, u8"\x1b[7b", 7)
    SHITTY_PARSER_CALLBACK_TEST1(InsertCharacter, csi_ICH, u8"\x1b[7@", 7)

    SHITTY_PARSER_CALLBACK_TEST0(EraseDisplayAfter, eraseDisplayAfter, u8"\x1b[J")
    SHITTY_PARSER_CALLBACK_TEST0(EraseDisplayBefore, eraseDisplayBefore, u8"\x1b[1J")
    SHITTY_PARSER_CALLBACK_TEST0(EraseDisplayAll, eraseDisplayAll, u8"\x1b[2J")
    SHITTY_PARSER_CALLBACK_TEST0(EraseScrollback, eraseScrollback, u8"\x1b[3J")
    SHITTY_PARSER_CALLBACK_TEST0(EraseLineAfter, eraseLineAfter, u8"\x1b[K")
    SHITTY_PARSER_CALLBACK_TEST0(EraseLineBefore, eraseLineBefore, u8"\x1b[1K")
    SHITTY_PARSER_CALLBACK_TEST0(EraseLineAll, eraseLineAll, u8"\x1b[2K")
    SHITTY_PARSER_CALLBACK_TEST0(SelectiveEraseDisplayAfter, selectiveEraseDisplayAfter, u8"\x1b[?J")
    SHITTY_PARSER_CALLBACK_TEST0(SelectiveEraseDisplayBefore, selectiveEraseDisplayBefore, u8"\x1b[?1J")
    SHITTY_PARSER_CALLBACK_TEST0(SelectiveEraseDisplayAll, selectiveEraseDisplayAll, u8"\x1b[?2J")
    SHITTY_PARSER_CALLBACK_TEST0(SelectiveEraseLineAfter, selectiveEraseLineAfter, u8"\x1b[?K")
    SHITTY_PARSER_CALLBACK_TEST0(SelectiveEraseLineBefore, selectiveEraseLineBefore, u8"\x1b[?1K")
    SHITTY_PARSER_CALLBACK_TEST0(SelectiveEraseLineAll, selectiveEraseLineAll, u8"\x1b[?2K")
    SHITTY_PARSER_CALLBACK_TEST1(SetDecProtection, setDecProtection, u8"\x1b[1\"q", true)
    SHITTY_PARSER_CALLBACK_TEST5(FillRectangle, csi_DECFRA, u8"\x1b[65;2;3;4;5$x", 65, 2, 3, 4, 5)

    STD_TEST(CopyRectangle) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[1;2;3;4;1;6;7;1$v"));
        expectValues(fixture.expect("csi_DECCRA"), 1, 2, 3, 4, 6, 7);
    }

    SHITTY_PARSER_CALLBACK_TEST5(EraseRectangle, csi_DECERA, u8"\x1b[1;2;3;4$z", 1, 2, 3, 4, false)
    SHITTY_PARSER_CALLBACK_TEST5(SelectiveEraseRectangle, csi_DECERA, u8"\x1b[1;2;3;4${", 1, 2, 3, 4, true)
    SHITTY_PARSER_CALLBACK_TEST1(SetAttributeChangeExtent, setAttributeChangeExtent, u8"\x1b[2*x", true)

    STD_TEST(ChangeRectangleAttributes) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[1;2;3;4;1$r"));
        const ParserCall& call = fixture.expect("changeRectangleAttributes");
        STD_INSIST(call.valueCount == 7);
        expectValues(call, 1, 2, 3, 4, CellAttributeChange::Bold, 0, 0);
    }

    STD_TEST(ChangeRectangleAttributesAcceptsFullSgr) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[;;;;4:3;38:5:10;48:2:1:2:3;1$r"));
        fixture.expect("changeRectangleAttributes");
        const CellAttributeChange& change = fixture.iface.lastAttributeChange;

        STD_INSIST(change.setMask == (CellAttributeChange::Bold | CellAttributeChange::Underline));
        STD_INSIST(change.clearMask == 0);
        STD_INSIST(change.toggleMask == 0);
        STD_INSIST(change.underlineStyleChanged);
        STD_INSIST(change.underlineStyle == 3);
        STD_INSIST(change.colorMask == (CellAttributeChange::Foreground | CellAttributeChange::Background));
        STD_INSIST(change.foreground == CellColor::indexed(10));
        STD_INSIST(change.background == CellColor::direct({1, 2, 3}));
    }

    STD_TEST(ChangeRectangleAttributesResetMatchesSgrReset) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[1;2;3;4;0$r"));
        fixture.expect("changeRectangleAttributes");
        const CellAttributeChange& change = fixture.iface.lastAttributeChange;
        constexpr u16 allAttributes = CellAttributeChange::Bold | CellAttributeChange::Faint | CellAttributeChange::Italic | CellAttributeChange::Underline | CellAttributeChange::Blink | CellAttributeChange::Inverse | CellAttributeChange::Conceal | CellAttributeChange::Strike | CellAttributeChange::Overline;

        STD_INSIST(change.setMask == 0);
        STD_INSIST(change.clearMask == allAttributes);
        STD_INSIST(change.underlineStyleChanged);
        STD_INSIST(change.underlineStyle == 0);
        STD_INSIST(change.colorMask == (CellAttributeChange::Foreground | CellAttributeChange::Background | CellAttributeChange::UnderlineColor | CellAttributeChange::UnderlineFromForeground));
        STD_INSIST(change.foreground == CellColor::defaultForeground());
        STD_INSIST(change.background == CellColor::defaultBackground());
    }

    STD_TEST(ChangeRectangleAttributesDefaultsToSgrReset) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[$r"));
        fixture.expect("changeRectangleAttributes");
        const CellAttributeChange& change = fixture.iface.lastAttributeChange;
        constexpr u16 allAttributes = CellAttributeChange::Bold | CellAttributeChange::Faint | CellAttributeChange::Italic | CellAttributeChange::Underline | CellAttributeChange::Blink | CellAttributeChange::Inverse | CellAttributeChange::Conceal | CellAttributeChange::Strike | CellAttributeChange::Overline;

        STD_INSIST(change.clearMask == allAttributes);
        STD_INSIST(change.colorMask == (CellAttributeChange::Foreground | CellAttributeChange::Background | CellAttributeChange::UnderlineColor | CellAttributeChange::UnderlineFromForeground));
    }

    SHITTY_PARSER_CALLBACK_TEST5(RequestRectangleChecksum, csi_DECRQCRA, u8"\x1b[9;1;2;3;4;5*y", 9, 2, 3, 4, 5)
    SHITTY_PARSER_CALLBACK_TEST5(RequestRectangleChecksumDefaultsToWholePage, csi_DECRQCRA, u8"\x1b[1*y", 1, 0, 0, 0, 0)
    SHITTY_PARSER_CALLBACK_TEST1(SetChecksumFlags, csi_XTCHECKSUM, u8"\x1b[31#y", 31)
    SHITTY_PARSER_CALLBACK_TEST1(InsertLines, csi_IL, u8"\x1b[7L", 7)
    SHITTY_PARSER_CALLBACK_TEST1(DeleteLines, csi_DL, u8"\x1b[7M", 7)
    SHITTY_PARSER_CALLBACK_TEST1(DeleteCharacters, csi_DCH, u8"\x1b[7P", 7)
    SHITTY_PARSER_CALLBACK_TEST1(EraseCharacters, csi_ECH, u8"\x1b[7X", 7)
    SHITTY_PARSER_CALLBACK_TEST1(InsertColumns, csi_DECIC, u8"\x1b[7'}", 7)
    SHITTY_PARSER_CALLBACK_TEST1(DeleteColumns, csi_DECDC, u8"\x1b[7'~", 7)
    SHITTY_PARSER_CALLBACK_TEST3(SetTopBottomMargins, csi_STBM, u8"\x1b[2;9r", 2, 9, true)
    SHITTY_PARSER_CALLBACK_TEST0(ClearTabStop, clearTabStop, u8"\x1b[g")
    SHITTY_PARSER_CALLBACK_TEST0(ClearAllTabStops, clearAllTabStops, u8"\x1b[3g")
    SHITTY_PARSER_CALLBACK_TEST0(ResetTabStops, resetTabStops, u8"\x1b[?5W")
    SHITTY_PARSER_CALLBACK_TEST0(ResetTabStopsDefault, resetTabStops, u8"\x1b[?W")
    SHITTY_PARSER_CALLBACK_TEST0(ReadModeState, parserModeState, u8"\x1b[?7s")

    SHITTY_PARSER_CALLBACK_TEST1(SetKeyboardLocked, setKeyboardLocked, u8"\x1b[2h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetInsertMode, setInsertMode, u8"\x1b[4h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetEraseModeAll, setEraseModeAll, u8"\x1b[6h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetLocalEcho, setLocalEcho, u8"\x1b[12h", false)
    SHITTY_PARSER_CALLBACK_TEST1(SetAutoNewline, setAutoNewline, u8"\x1b[20h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetAnsiMode, setAnsiMode, u8"\x1b[?2h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetApplicationCursorKeys, setApplicationCursorKeys, u8"\x1b[?1h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetColumn132, setColumn132, u8"\x1b[?3h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetSmoothScroll, setSmoothScroll, u8"\x1b[?4h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetScreenReverseVideo, setScreenReverseVideo, u8"\x1b[?5h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetOriginMode, setOriginMode, u8"\x1b[?6h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetAutoWrap, setAutoWrap, u8"\x1b[?7h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetAutoRepeat, setAutoRepeat, u8"\x1b[?8h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetAllowColumnMode, setAllowColumnMode, u8"\x1b[?40h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetMoreFix, setMoreFix, u8"\x1b[?41h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetNationalReplacement, setNationalReplacement, u8"\x1b[?42h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetReverseWrap, setReverseWrap, u8"\x1b[?45h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetMouseTracking, setMouseTracking, u8"\x1b[?1000h", MouseTrackingMode::VT200)
    SHITTY_PARSER_CALLBACK_TEST1(SetCursorBlink, setCursorBlink, u8"\x1b[?12h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetCursorVisible, setCursorVisible, u8"\x1b[?25h", true)
    SHITTY_PARSER_CALLBACK_TEST2(SetAlternateScreen, setAlternateScreen, u8"\x1b[?47h", true, false)
    SHITTY_PARSER_CALLBACK_TEST1(SetBackspaceMode, setBackspaceSendsBackspace, u8"\x1b[?67h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetHorizontalMargins, setHorizontalMargins, u8"\x1b[?69h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetNoClearColumn, setNoClearColumn, u8"\x1b[?95h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetFocusEvents, setFocusEvents, u8"\x1b[?1004h", true)
    SHITTY_PARSER_CALLBACK_TEST2(SetMouseEncoding, setMouseEncoding, u8"\x1b[?1006h", MouseTrackingEnc::SGR, true)
    SHITTY_PARSER_CALLBACK_TEST2(ResetMouseEncoding, setMouseEncoding, u8"\x1b[?1006l", MouseTrackingEnc::SGR, false)
    SHITTY_PARSER_CALLBACK_TEST1(SetAlternateScroll, setAlternateScroll, u8"\x1b[?1007h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetEightBitInput, setEightBitInput, u8"\x1b[?1034h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetAltSendsEscape, setAltSendsEscape, u8"\x1b[?1036h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetSavedAlternateScreen, setSavedAlternateScreen, u8"\x1b[?1049h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetExtendedReverseWrap, setExtendedReverseWrap, u8"\x1b[?1045h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetBracketedPaste, setBracketedPaste, u8"\x1b[?2004h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetSynchronizedOutput, setSynchronizedOutput, u8"\x1b[?2026h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetGraphemeCluster, setGraphemeCluster, u8"\x1b[?2027h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetColorSchemeUpdates, setColorSchemeUpdates, u8"\x1b[?2031h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetInBandResize, setInBandResize, u8"\x1b[?2048h", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetPasteMimeNotifications, setPasteMimeNotifications, u8"\x1b[?5522h", true)
    SHITTY_PARSER_CALLBACK_TEST2(SavePrivateMode, savePrivateMode, u8"\x1b[?7s", 7, false)

    STD_TEST(UnknownPrivateModeIsNotSaved) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[?9999s"));
        STD_INSIST(!fixture.iface.called("savePrivateMode"));
    }

    STD_TEST(RestorePrivateMode) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[?7r"));
        expectValues(fixture.expect("restorePrivateMode"), 7, true, true);
    }

    SHITTY_PARSER_CALLBACK_TEST3(ReportMode, reportMode, u8"\x1b[4$p", 4, false, 2)
    SHITTY_PARSER_CALLBACK_TEST3(ReportResetGraphemeMode, reportMode, u8"\x1b[?2027$p", 2027, true, 2)
    SHITTY_PARSER_CALLBACK_TEST1(ScrollLeft, csi_ecma48_SL, u8"\x1b[7 @", 7)
    SHITTY_PARSER_CALLBACK_TEST1(ScrollRight, csi_ecma48_SR, u8"\x1b[7 A", 7)
    SHITTY_PARSER_CALLBACK_TEST3(SetCursorStyle, setCursorStyle, u8"\x1b[5 q", 5, TerminalCursor::Style::bar, true)
    SHITTY_PARSER_CALLBACK_TEST0(RefreshCursorStyle, refreshCursorStyle, u8"\x1b[9 q")

    SHITTY_PARSER_CALLBACK_TEST0(PrimaryDeviceAttributes, csi_priDA, u8"\x1b[c")
    SHITTY_PARSER_CALLBACK_TEST0(SecondaryDeviceAttributes, csi_secDA, u8"\x1b[>c")
    SHITTY_PARSER_CALLBACK_TEST0(TertiaryDeviceAttributes, csi_terDA, u8"\x1b[=c")
    SHITTY_PARSER_CALLBACK_TEST0(RequestDisplayedExtent, csi_DECRQDE, u8"\x1b[\"v")
    SHITTY_PARSER_CALLBACK_TEST1(RequestTerminalParameters, csi_DECREQTPARM, u8"\x1b[1x", 1)
    SHITTY_PARSER_CALLBACK_TEST1(RequestColorTableHls, csi_DECRQTSR_COLOR, u8"\x1b[2;1$u", 1)
    SHITTY_PARSER_CALLBACK_TEST1(RequestColorTableRgb, csi_DECRQTSR_COLOR, u8"\x1b[2;2$u", 2)
    SHITTY_PARSER_CALLBACK_TEST0(RequestTabStops, csi_DECRQPSR_TABS, u8"\x1b[2$w")
    SHITTY_PARSER_CALLBACK_TEST0(RequestCursorInformation, csi_DECRQPSR_CURSOR, u8"\x1b[1$w")
    SHITTY_PARSER_CALLBACK_TEST0(RequestUserPreferenceCharset, csi_DECRQUPSS, u8"\x1b[&u")
    SHITTY_PARSER_CALLBACK_TEST0(OperatingStatus, dsrOperatingStatus, u8"\x1b[5n")
    SHITTY_PARSER_CALLBACK_TEST1(CursorPositionReport, dsrCursorPosition, u8"\x1b[6n", false)
    SHITTY_PARSER_CALLBACK_TEST0(PrinterStatus, dsrPrinter, u8"\x1b[?15n")
    SHITTY_PARSER_CALLBACK_TEST0(UserDefinedKeysStatus, dsrUserDefinedKeys, u8"\x1b[?25n")
    SHITTY_PARSER_CALLBACK_TEST0(KeyboardStatus, dsrKeyboard, u8"\x1b[?26n")
    SHITTY_PARSER_CALLBACK_TEST0(LocatorStatus, dsrLocator, u8"\x1b[?55n")
    SHITTY_PARSER_CALLBACK_TEST0(LocatorType, dsrLocatorType, u8"\x1b[?56n")
    SHITTY_PARSER_CALLBACK_TEST0(MacroSpace, dsrMacroSpace, u8"\x1b[?62n")
    SHITTY_PARSER_CALLBACK_TEST1(MemoryChecksum, dsrMemoryChecksum, u8"\x1b[?63;9n", 9)
    SHITTY_PARSER_CALLBACK_TEST0(DataIntegrity, dsrDataIntegrity, u8"\x1b[?75n")
    SHITTY_PARSER_CALLBACK_TEST0(MultipleSession, dsrMultipleSession, u8"\x1b[?85n")
    SHITTY_PARSER_CALLBACK_TEST0(ColorScheme, dsrColorScheme, u8"\x1b[?996n")

    SHITTY_PARSER_CALLBACK_TEST0(SgrReset, sgrReset, u8"\x1b[0m")
    SHITTY_PARSER_CALLBACK_TEST1(SgrBold, sgrBold, u8"\x1b[1m", true)
    SHITTY_PARSER_CALLBACK_TEST1(SgrFaint, sgrFaint, u8"\x1b[2m", true)
    SHITTY_PARSER_CALLBACK_TEST1(SgrItalic, sgrItalic, u8"\x1b[3m", true)
    SHITTY_PARSER_CALLBACK_TEST1(SgrUnderline, sgrUnderline, u8"\x1b[4:3m", 3)
    SHITTY_PARSER_CALLBACK_TEST1(SgrBlink, sgrBlink, u8"\x1b[5m", true)
    SHITTY_PARSER_CALLBACK_TEST1(SgrInverse, sgrInverse, u8"\x1b[7m", true)
    SHITTY_PARSER_CALLBACK_TEST1(SgrConceal, sgrConceal, u8"\x1b[8m", true)
    SHITTY_PARSER_CALLBACK_TEST1(SgrStrike, sgrStrike, u8"\x1b[9m", true)
    SHITTY_PARSER_CALLBACK_TEST1(SgrOverline, sgrOverline, u8"\x1b[53m", true)

    STD_TEST(SgrForeground) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[31m"));
        expectValues(fixture.expect("sgrForeground"), CellColor::indexed(1).encoded(), 1, true);
    }

    STD_TEST(SgrForegroundDefaultsEmptyColonComponentsToZero) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[38:2::1:2m"));
        expectValues(fixture.expect("sgrForeground"), CellColor::direct({0, 1, 2}).encoded(), -1, false);
    }

    SHITTY_PARSER_CALLBACK_TEST0(SgrDefaultForeground, sgrDefaultForeground, u8"\x1b[39m")

    STD_TEST(SgrBackground) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[41m"));
        expectValues(fixture.expect("sgrBackground"), CellColor::indexed(1).encoded(), 1);
    }

    SHITTY_PARSER_CALLBACK_TEST0(SgrDefaultBackground, sgrDefaultBackground, u8"\x1b[49m")

    STD_TEST(SgrUnderlineColor) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[58;5;7m"));
        expectValues(fixture.expect("sgrUnderlineColor"), CellColor::indexed(7).encoded(), 7);
    }

    SHITTY_PARSER_CALLBACK_TEST0(SgrDefaultUnderlineColor, sgrDefaultUnderlineColor, u8"\x1b[59m")
    SHITTY_PARSER_CALLBACK_TEST0(SgrFinish, sgrFinish, u8"\x1b[m")
    SHITTY_PARSER_CALLBACK_TEST0(PopSgr, csi_XTPOPSGR, u8"\x1b[#}")
    STD_TEST(PushSgr) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[1;30;31#{"));
        expectValues(fixture.expect("csi_XTPUSHSGR"), 1, 30, 31);
    }
    SHITTY_PARSER_CALLBACK_TEST0(ScreenAlignmentPattern, esch_DECALN, u8"\x1b#8")
    SHITTY_PARSER_CALLBACK_TEST1(SetLineAttribute, setLineAttribute, u8"\x1b#3", 1)

    SHITTY_PARSER_TEXT_TEST(TitleAndIcon, osc_TITLE_0, u8"\x1b]0;title\a", u8"title")
    SHITTY_PARSER_TEXT_TEST(IconTitle, osc_TITLE_1, u8"\x1b]1;icon\a", u8"icon")
    SHITTY_PARSER_TEXT_TEST(WindowTitle, osc_TITLE_2, u8"\x1b]2;window\a", u8"window")
    SHITTY_PARSER_CALLBACK_TEST5(SetPaletteColor, osc_PALETTE, u8"\x1b]4;7;#123\a", 7, 0x10, 0x20, 0x30, false)
    SHITTY_PARSER_CALLBACK_TEST5(SetSpecialColor, osc_SPECIAL_COLOR, u8"\x1b]5;7;#123\a", 7, 0x10, 0x20, 0x30, false)
    SHITTY_PARSER_CALLBACK_TEST2(SetSpecialColorMode, osc_SPECIAL_COLOR_MODE, u8"\x1b]6;7;2\a", 7, 2)

    STD_TEST(HexTitleIsDecodedInPlace) {
        ParserFixture fixture;
        fixture.iface.hexTitleInput = true;
        fixture.feed(StringView(u8"\x1b]0;6869\a"));
        const ParserCall& call = fixture.expect("osc_TITLE_0");
        expectText(fixture.iface, call, 0, StringView(u8"hi"));
    }

    STD_TEST(CurrentWorkingDirectory) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]7;file://host/a%20b\a"));
        const ParserCall& call = fixture.expect("osc_CWD");
        expectValues(call, true);
        expectText(fixture.iface, call, 0, StringView(u8"/a b"));
    }

    STD_TEST(Hyperlink) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]8;id=link;https://example.com\a"));
        const ParserCall& call = fixture.expect("osc_HYPERLINK");
        expectValues(call, true);
        expectText(fixture.iface, call, 0, StringView(u8"link"));
        expectText(fixture.iface, call, 1, StringView(u8"https://example.com"));
    }

    SHITTY_PARSER_TEXT_TEST(Notification, osc_NOTIFY, u8"\x1b]9;hello\a", u8"hello")
    SHITTY_PARSER_CALLBACK_TEST3(Progress, osc_PROGRESS, u8"\x1b]9;4;1;77\a", 1, 77, true)
    SHITTY_PARSER_CALLBACK_TEST3(ProgressWithoutPercent, osc_PROGRESS, u8"\x1b]9;4;4\a", 4, 0, false)
    SHITTY_PARSER_CALLBACK_TEST4(DefaultForeground, osc_DEFAULT_FOREGROUND, u8"\x1b]10;#123\a", 0x10, 0x20, 0x30, false)
    SHITTY_PARSER_CALLBACK_TEST4(DefaultBackground, osc_DEFAULT_BACKGROUND, u8"\x1b]11;#123\a", 0x10, 0x20, 0x30, false)
    SHITTY_PARSER_CALLBACK_TEST4(CursorColor, osc_CURSOR_COLOR, u8"\x1b]12;#123\a", 0x10, 0x20, 0x30, false)
    SHITTY_PARSER_CALLBACK_TEST4(SelectionBackground, osc_SELECTION_BACKGROUND, u8"\x1b]17;#123\a", 0x10, 0x20, 0x30, false)
    SHITTY_PARSER_CALLBACK_TEST4(SelectionForeground, osc_SELECTION_FOREGROUND, u8"\x1b]19;#123\a", 0x10, 0x20, 0x30, false)

    STD_TEST(ClipboardQuery) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]52;p;?\a"));
        const ParserCall& call = fixture.expect("osc_CLIPBOARD_QUERY");
        expectValues(call, true, false, 'p', false);
    }

    STD_TEST(ClipboardWrite) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]52;c;YQ==\a"));
        const ParserCall& call = fixture.expect("osc_CLIPBOARD_WRITE");
        expectValues(call, true, false, true);
        expectText(fixture.iface, call, 0, StringView(u8"a"));
    }

    STD_TEST(KittyClipboardRead) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]5522;type=read:id=abc:loc=primary;dGV4dC9wbGFpbg==\x1b\\"));
        const ParserCall& call = fixture.expect("osc_KITTY_CLIPBOARD_READ");
        expectValues(call, true, true);
        expectText(fixture.iface, call, 0, StringView(u8"abc"));
        expectText(fixture.iface, call, 1, StringView(u8"text/plain"));
    }

    STD_TEST(KittyClipboardWrite) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]5522;type=write:name=Zm9v:pw=cHc=:id=7;\x1b\\"));
        const ParserCall& call = fixture.expect("osc_KITTY_CLIPBOARD_WRITE");
        expectValues(call, false);
        expectText(fixture.iface, call, 0, StringView(u8"7"));
    }

    STD_TEST(KittyClipboardWriteData) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]5522;type=wdata:mime=dGV4dC9wbGFpbg==:id=7;QUI=\x1b\\"));
        const ParserCall& call = fixture.expect("osc_KITTY_CLIPBOARD_WRITE_DATA");
        expectValues(call, true);
        expectText(fixture.iface, call, 0, StringView(u8"7"));
        expectText(fixture.iface, call, 1, StringView(u8"text/plain"));
        expectText(fixture.iface, call, 2, StringView(u8"AB"));
    }

    STD_TEST(KittyClipboardWriteAlias) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]5522;type=walias:mime=dGV4dC9wbGFpbg==;VEVYVCBTVFJJTkc=\x1b\\"));
        const ParserCall& call = fixture.expect("osc_KITTY_CLIPBOARD_WRITE_ALIAS");
        expectValues(call, true);
        expectText(fixture.iface, call, 1, StringView(u8"text/plain"));
        expectText(fixture.iface, call, 2, StringView(u8"TEXT STRING"));
    }

    STD_TEST(KittyClipboardMalformedMetadata) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]5522;type=write:novalue;\x1b\\"));
        const ParserCall& call = fixture.expect("osc_KITTY_CLIPBOARD_INVALID");
        expectValues(call, true);
    }

    STD_TEST(KittyClipboardUnknownType) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]5522;type=nonsense;\x1b\\"));
        const ParserCall& call = fixture.expect("osc_KITTY_CLIPBOARD_INVALID");
        expectValues(call, false);
    }

    STD_TEST(MalformedClipboard) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]52;p\a"));
        const ParserCall& call = fixture.expect("osc_RAW");
        expectValues(call, 52);
        expectText(fixture.iface, call, 0, StringView(u8"p"));
    }

    STD_TEST(EncodedNotificationRemainsEncodedForConsumer) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]99;i=id:p=title:e=1;YQ==\a"));
        const ParserCall& call = fixture.expect("osc_NOTIFICATION_TITLE");
        expectValues(call, true, true);
        expectText(fixture.iface, call, 0, StringView(u8"id"));
        expectText(fixture.iface, call, 1, StringView(u8"YQ=="));
    }

    SHITTY_PARSER_TEXT_TEST(NotificationCapabilities, osc_NOTIFICATION_CAPABILITIES, u8"\x1b]99;i=id:p=?;\a", u8"id")
    SHITTY_PARSER_TEXT_TEST(NotificationClose, osc_NOTIFICATION_CLOSE, u8"\x1b]99;i=id:p=close;\a", u8"id")

    STD_TEST(NotificationTitle) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]99;i=id:p=title;hello\a"));
        const ParserCall& call = fixture.expect("osc_NOTIFICATION_TITLE");
        expectValues(call, false, true);
        expectText(fixture.iface, call, 0, StringView(u8"id"));
        expectText(fixture.iface, call, 1, StringView(u8"hello"));
    }

    STD_TEST(NotificationBody) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]99;i=id:p=body:d=0;hello\a"));
        const ParserCall& call = fixture.expect("osc_NOTIFICATION_BODY");
        expectValues(call, false, false);
        expectText(fixture.iface, call, 0, StringView(u8"id"));
        expectText(fixture.iface, call, 1, StringView(u8"hello"));
    }

    SHITTY_PARSER_CALLBACK_TEST0(ResetPalette, osc_RESET_PALETTE, u8"\x1b]104;\a")
    SHITTY_PARSER_CALLBACK_TEST1(ResetPaletteEntry, osc_RESET_PALETTE, u8"\x1b]104;7\a", 7)
    SHITTY_PARSER_CALLBACK_TEST0(ResetSpecialColors, osc_RESET_SPECIAL_COLOR, u8"\x1b]105;\a")
    SHITTY_PARSER_CALLBACK_TEST1(ResetSpecialColorEntry, osc_RESET_SPECIAL_COLOR, u8"\x1b]105;7\a", 7)
    SHITTY_PARSER_CALLBACK_TEST0(ResetDefaultForeground, osc_RESET_DEFAULT_FOREGROUND, u8"\x1b]110;\a")
    SHITTY_PARSER_CALLBACK_TEST0(ResetDefaultBackground, osc_RESET_DEFAULT_BACKGROUND, u8"\x1b]111;\a")
    SHITTY_PARSER_CALLBACK_TEST0(ResetCursorColor, osc_RESET_CURSOR_COLOR, u8"\x1b]112;\a")
    SHITTY_PARSER_CALLBACK_TEST0(ResetSelectionBackground, osc_RESET_SELECTION_BACKGROUND, u8"\x1b]117;\a")
    SHITTY_PARSER_CALLBACK_TEST0(ResetSelectionForeground, osc_RESET_SELECTION_FOREGROUND, u8"\x1b]119;\a")
    SHITTY_PARSER_TEXT_TEST(ShellPromptStart, osc_SHELL_A, u8"\x1b]133;A;payload\a", u8"A;payload")
    SHITTY_PARSER_TEXT_TEST(ShellCommandStart, osc_SHELL_B, u8"\x1b]133;B;payload\a", u8"B;payload")
    SHITTY_PARSER_TEXT_TEST(ShellCommandExecuted, osc_SHELL_C, u8"\x1b]133;C;payload\a", u8"C;payload")
    SHITTY_PARSER_TEXT_TEST(ShellCommandFinished, osc_SHELL_D, u8"\x1b]133;D;payload\a", u8"D;payload")
    SHITTY_PARSER_TEXT_TEST(ShellInputUntilEndOfLine, osc_SHELL_I, u8"\x1b]133;I;payload\a", u8"I;payload")
    SHITTY_PARSER_TEXT_TEST(ShellFreshLine, osc_SHELL_L, u8"\x1b]133;L;payload\a", u8"L;payload")
    SHITTY_PARSER_TEXT_TEST(ShellNewCommand, osc_SHELL_N, u8"\x1b]133;N;payload\a", u8"N;payload")
    SHITTY_PARSER_TEXT_TEST(ShellPromptStartExplicit, osc_SHELL_P, u8"\x1b]133;P;payload\a", u8"P;payload")
    SHITTY_PARSER_TEXT_TEST(ShellUnknown, osc_SHELL_UNKNOWN, u8"\x1b]133;X;payload\a", u8"X;payload")

    STD_TEST(UnknownOsc) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b]999;payload\a"));
        const ParserCall& call = fixture.expect("osc_UNKNOWN");
        expectValues(call, 999);
        expectText(fixture.iface, call, 0, StringView(u8"payload"));
    }

    SHITTY_PARSER_CALLBACK_TEST2(SetCompatibilityFromCsi, csi_DECSCL, u8"\x1b[65;1\"p", CompatibilityLevel::VT500, false)
    SHITTY_PARSER_CALLBACK_TEST4(ResizePixels, xtResizePixels, u8"\x1b[4;600;800t", 600, true, 800, true)
    SHITTY_PARSER_CALLBACK_TEST4(ResizeCells, xtResizeCells, u8"\x1b[8;24;80t", 24, true, 80, true)
    SHITTY_PARSER_CALLBACK_TEST3(WindowOperation, xtWindowOperation, u8"\x1b[3;4;5t", 3, 4, 5)
    SHITTY_PARSER_CALLBACK_TEST0(ReportWindowState, xtReportWindowState, u8"\x1b[11t")
    SHITTY_PARSER_CALLBACK_TEST0(ReportWindowPosition, xtReportWindowPosition, u8"\x1b[13t")
    SHITTY_PARSER_CALLBACK_TEST1(ReportWindowPixelSize, xtReportWindowPixelSize, u8"\x1b[14;2t", true)
    SHITTY_PARSER_CALLBACK_TEST0(ReportScreenPixelSize, xtReportScreenPixelSize, u8"\x1b[15t")
    SHITTY_PARSER_CALLBACK_TEST0(ReportCellSize, xtReportCellSize, u8"\x1b[16t")
    SHITTY_PARSER_CALLBACK_TEST0(ReportGridSize, xtReportGridSize, u8"\x1b[18t")
    SHITTY_PARSER_CALLBACK_TEST0(ReportScreenGridSize, xtReportScreenGridSize, u8"\x1b[19t")
    SHITTY_PARSER_CALLBACK_TEST0(ReportIconTitle, xtReportIconTitle, u8"\x1b[20t")
    SHITTY_PARSER_CALLBACK_TEST0(ReportWindowTitle, xtReportWindowTitle, u8"\x1b[21t")
    SHITTY_PARSER_CALLBACK_TEST2(PushTitle, xtPushTitle, u8"\x1b[22;0t", true, true)
    SHITTY_PARSER_CALLBACK_TEST2(PopTitle, xtPopTitle, u8"\x1b[23;0t", true, true)
    SHITTY_PARSER_CALLBACK_TEST1(ResizeRows, xtResizeRows, u8"\x1b[42t", 42)
    SHITTY_PARSER_CALLBACK_TEST0(ResetTitleModes, resetTitleModes, u8"\x1b[>T")
    SHITTY_PARSER_CALLBACK_TEST2(SetTitleMode, setTitleMode, u8"\x1b[>2t", 4, true)

    STD_TEST(HighlightMouseTracking) {
        ParserFixture fixture;
        fixture.iface.highlightMouseTracking = true;
        fixture.feed(StringView(u8"\x1b[1;2;3;4;5T"));
        expectValues(fixture.expect("csi_XTHIMOUSE"), 1, 2, 3, 4, 5);
    }

    SHITTY_PARSER_CALLBACK_TEST3(SetLocatorReporting, setLocatorReporting, u8"\x1b[2;1'z", true, true, true)
    SHITTY_PARSER_CALLBACK_TEST0(ResetLocatorEvents, resetLocatorEvents, u8"\x1b[0'{")
    SHITTY_PARSER_CALLBACK_TEST1(SetLocatorButtonDown, setLocatorButtonDown, u8"\x1b[1'{", true)
    SHITTY_PARSER_CALLBACK_TEST1(SetLocatorButtonUp, setLocatorButtonUp, u8"\x1b[3'{", true)
    SHITTY_PARSER_CALLBACK_TEST0(RequestLocatorPosition, csi_DECRQLP, u8"\x1b['|")
    SHITTY_PARSER_CALLBACK_TEST4(EnableFilterRectangle, csi_DECEFR, u8"\x1b[1;2;3;4'w", 1, 2, 3, 4)
    SHITTY_PARSER_CALLBACK_TEST0(ResetModifyKeyResources, resetModifyKeyResources, u8"\x1b[>m")
    SHITTY_PARSER_CALLBACK_TEST3(SetModifyKeyResource, setModifyKeyResource, u8"\x1b[>1;2m", 1, 2, false)
    SHITTY_PARSER_CALLBACK_TEST1(ReportModifyKeyResource, reportModifyKeyResource, u8"\x1b[?1m", 1)
    SHITTY_PARSER_CALLBACK_TEST1(PushKittyKeyboard, csi_kittyKeyboardPush, u8"\x1b[>3u", 3)
    SHITTY_PARSER_CALLBACK_TEST1(PopKittyKeyboard, csi_kittyKeyboardPop, u8"\x1b[<2u", 2)
    SHITTY_PARSER_CALLBACK_TEST1(SetKittyKeyboardFlags, setKittyKeyboardFlags, u8"\x1b[=3;1u", 3)
    SHITTY_PARSER_CALLBACK_TEST1(AddKittyKeyboardFlags, addKittyKeyboardFlags, u8"\x1b[=3;2u", 3)
    SHITTY_PARSER_CALLBACK_TEST1(RemoveKittyKeyboardFlags, removeKittyKeyboardFlags, u8"\x1b[=3;3u", 3)
    SHITTY_PARSER_CALLBACK_TEST0(QueryKittyKeyboard, csi_kittyKeyboardQuery, u8"\x1b[?u")
    SHITTY_PARSER_CALLBACK_TEST0(XtermVersion, csi_XTVERSION, u8"\x1b[>q")
    SHITTY_PARSER_CALLBACK_TEST0(XtermVersionZero, csi_XTVERSION, u8"\x1b[>0q")
    STD_TEST(XtermVersionRejectsNonzeroSelector) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[>1q"));
        STD_INSIST(!fixture.iface.called("csi_XTVERSION"));
    }
    SHITTY_PARSER_CALLBACK_TEST0(SetMark, csi_SETMARK, u8"\x1b[>M")
    SHITTY_PARSER_CALLBACK_TEST0(ResetLeds, resetLeds, u8"\x1b[0q")
    SHITTY_PARSER_CALLBACK_TEST2(SetLed, setLed, u8"\x1b[2q", 1, true)
    SHITTY_PARSER_CALLBACK_TEST0(CommitLeds, commitLeds, u8"\x1b[q")

    SHITTY_PARSER_CALLBACK_TEST0(RequestCompatibilityLevel, dcs_DECRQSS_DECSCL, u8"\x1bP$q\"p\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestSgr, dcs_DECRQSS_SGR, u8"\x1bP$qm\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestTopBottomMargins, dcs_DECRQSS_DECSTBM, u8"\x1bP$qr\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestLeftRightMargins, dcs_DECRQSS_DECSLRM, u8"\x1bP$qs\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestLinesPerPage, dcs_DECRQSS_DECSLPP, u8"\x1bP$qt\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestCursorStyle, dcs_DECRQSS_DECSCUSR, u8"\x1bP$q q\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestProtectionAttribute, dcs_DECRQSS_DECSCA, u8"\x1bP$q\"q\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestAttributeChangeExtent, dcs_DECRQSS_DECSACE, u8"\x1bP$q*x\x1b\\")
    SHITTY_PARSER_CALLBACK_TEST0(RequestUnknownStatus, dcs_DECRQSS_UNKNOWN, u8"\x1bP$qz\x1b\\")

    STD_TEST(GetTermcap) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP+q544e\x1b\\"));
        const ParserCall& call = fixture.expect("dcs_XTGETTCAP");
        expectValues(call);
        expectText(fixture.iface, call, 0, StringView(u8"544e"));
        expectText(fixture.iface, call, 1, StringView(u8"xterm-256color"));
    }

    STD_TEST(DefineUserKey) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP0;0|17/41\x1b\\"));
        const ParserCall& call = fixture.expect("dcs_DECUDK");
        expectValues(call, true, true, 1, 0, 1, InputKey::F6);
        expectText(fixture.iface, call, 0, StringView(u8"A"));
    }

    STD_TEST(RestoreColorTable) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP2$p1;1;120;46;71/2;2;79;13;13\x1b\\"));
        expectValues(fixture.iface.find("dcs_DECRSTS_HLS"), 1, 120, 46, 71);
        expectValues(fixture.iface.find("dcs_DECRSTS_RGB"), 2, 79, 13, 13);
    }

    STD_TEST(RestoreTabStops) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP2$t30/60//0/1/120\x1b\\"));
        fixture.iface.find("dcs_DECRSTS_TABS_BEGIN");
        const i64 expected[] = {30, 60, 120};
        size_t tabCalls = 0;
        for (size_t index = 0; index < fixture.iface.callCount; ++index) {
            if (StringView(fixture.iface.calls[index].name) == StringView(u8"dcs_DECRSTS_TAB")) {
                STD_INSIST(tabCalls < sizeof(expected) / sizeof(expected[0]));
                STD_INSIST(fixture.iface.calls[index].values[0] == expected[tabCalls]);
                ++tabCalls;
            }
        }
        STD_INSIST(tabCalls == sizeof(expected) / sizeof(expected[0]));
    }

    STD_TEST(SixelBasicImage) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bPq#1;2;100;0;0#1~~$-~\x1b\\"));
        const ParserCall& call = fixture.iface.find("dcs_SIXEL");
        expectValues(call, 2, 12);
        const StringView pixels = fixture.iface.text(call, 0);
        STD_INSIST(pixels.length() == 24);
        for (u32 row = 0; row < 6; ++row) {
            STD_INSIST(pixels[row * 2 + 0] == 2);
            STD_INSIST(pixels[row * 2 + 1] == 2);
        }
        for (u32 row = 6; row < 12; ++row) {
            STD_INSIST(pixels[row * 2 + 0] == 2);
            STD_INSIST(pixels[row * 2 + 1] == 0);
        }
        const StringView palette = fixture.iface.text(call, 1);
        STD_INSIST(palette.length() == SixelPatch::paletteBytes);
        STD_INSIST(palette[3] == 255);
        STD_INSIST(palette[4] == 0);
        STD_INSIST(palette[5] == 0);
    }

    STD_TEST(SixelRepeatAndMultiPassBand) {
        ParserFixture fixture;
        // Five columns of the low three rows in register 2, then a
        // second pass over the same band drops a full column of
        // register 3 at x 2.
        fixture.feed(StringView(u8"\x1bPq#2;2;0;100;0#2!5F$#3;2;0;0;100#3??~\x1b\\"));
        const ParserCall& call = fixture.iface.find("dcs_SIXEL");
        expectValues(call, 5, 6);
        const StringView pixels = fixture.iface.text(call, 0);
        for (u32 row = 0; row < 6; ++row) {
            for (u32 column = 0; column < 5; ++column) {
                u8 expected = row < 3 ? 3 : 0;
                if (column == 2) {
                    expected = 4;
                }
                STD_INSIST(pixels[row * 5 + column] == expected);
            }
        }
    }

    STD_TEST(SixelRasterAttributesDeclareExtent) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP0;0;0q\"1;1;10;13\x1b\\"));
        const ParserCall& call = fixture.iface.find("dcs_SIXEL");
        expectValues(call, 10, 13);
        const StringView pixels = fixture.iface.text(call, 0);
        STD_INSIST(pixels.length() == 130);
        for (size_t index = 0; index < pixels.length(); ++index) {
            STD_INSIST(pixels[index] == 0);
        }
    }

    STD_TEST(SixelSurvivesChunkedFeed) {
        ParserFixture fixture;
        const StringView input(u8"\x1bPq#1;2;100;0;0#1~~$-~\x1b\\");
        for (size_t index = 0; index < input.length(); ++index) {
            fixture.feed(StringView(input.data() + index, 1));
        }
        const ParserCall& call = fixture.iface.find("dcs_SIXEL");
        expectValues(call, 2, 12);
        const StringView pixels = fixture.iface.text(call, 0);
        for (u32 row = 0; row < 6; ++row) {
            STD_INSIST(pixels[row * 2 + 0] == 2);
            STD_INSIST(pixels[row * 2 + 1] == 2);
        }
    }

    STD_TEST(SixelHlsColorDefinition) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bPq#5;1;120;46;71#5~\x1b\\"));
        const ParserCall& call = fixture.iface.find("dcs_SIXEL");
        const StringView palette = fixture.iface.text(call, 1);
        STD_INSIST(palette[15] == 201);
        STD_INSIST(palette[16] == 34);
        STD_INSIST(palette[17] == 34);
        const StringView pixels = fixture.iface.text(call, 0);
        STD_INSIST(pixels[0] == 6);
    }

    STD_TEST(SixelCancelledByCan) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bPq#1~~\x18"));
        STD_INSIST(!fixture.iface.called("dcs_SIXEL"));
        fixture.feed(StringView(u8"\x1bPq\x1b\\"));
        STD_INSIST(!fixture.iface.called("dcs_SIXEL"));
    }

    STD_TEST(RestoreCursorInformation) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP1$t3;4;1;J;A;J;3;1;H;ABCF\x1b\\"));
        expectValues(fixture.iface.find("dcs_DECRSTS_CURSOR"), 3, 4, 10, 1, 10, 3, 1, 8, Charset::IsoUK, 'A', Charset::UTF8, 'B', Charset::NrcFinnish, 'C', Charset::UTF8, 'F');
    }

    STD_TEST(AssignUserPreferenceCharset) {
        struct Case {
            StringView input;
            Charset charset;
            u16 id;
            bool is96;
        };

        const Case cases[] = {
            {StringView(u8"\x1bP0!u%5\x1b\\"), Charset::DecSuppl, (u16)('%' << 8 | '5'), false},
            {StringView(u8"\x1bP0!u\"?\x1b\\"), Charset::NrcGreek, (u16)('"' << 8 | '?'), false},
            {StringView(u8"\x1bP0!u\"4\x1b\\"), Charset::NrcHebrew, (u16)('"' << 8 | '4'), false},
            {StringView(u8"\x1bP0!u%0\x1b\\"), Charset::NrcTurkish, (u16)('%' << 8 | '0'), false},
            {StringView(u8"\x1bP0!u&4\x1b\\"), Charset::NrcRussian, (u16)('&' << 8 | '4'), false},
            {StringView(u8"\x1bP1!uA\x1b\\"), Charset::IsoLatin1, 'A', true},
        };
        for (const Case& test : cases) {
            ParserFixture fixture;
            fixture.feed(test.input);
            expectValues(fixture.iface.find("dcs_DECAUPSS"), test.charset, test.id, test.is96);
        }
    }

    STD_TEST(StringPayloadGarbageDoesNotWedgeParser) {
        // Bytes with no grammar transition used to strand the machine in
        // the ragel error state, which consumes nothing, so feed() spun on
        // the same byte forever. The $err recovery reroutes them into
        // dcsIgnore: the malformed string is dropped and parsing goes on.
        const StringView garbage[] = {
            StringView(u8"\x1bP0!u\xeb\x1b\\"),
            StringView(u8"\x90!u\xeb\x9c"),
            StringView(u8"\x1bP0!u%\x07\x1b\\"),
            StringView(u8"\x1bP0!u \x1b\\"),
            StringView(u8"\x1bP1$t3;4;?\x1b\\"),
            StringView(u8"\x1bP1$tzz\x1b\\"),
        };
        for (const StringView& input : garbage) {
            ParserFixture fixture;
            fixture.feed(input);
            STD_INSIST(!fixture.iface.called("dcs_DECAUPSS"));
            STD_INSIST(!fixture.iface.called("dcs_DECRSTS_CURSOR"));
            fixture.feed(StringView(u8"\x1bP1!uA\x1b\\"));
            expectValues(fixture.iface.find("dcs_DECAUPSS"), Charset::IsoLatin1, 'A', true);
        }
    }

    STD_TEST(RestoreColorTableOmittedAndClampedValues) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP2$p6;1;;50;100/12;2;150;0\x1b\\"));
        expectValues(fixture.iface.find("dcs_DECRSTS_HLS"), 6, 0, 50, 100);
        expectValues(fixture.iface.find("dcs_DECRSTS_RGB"), 12, 150, 0, 0);
    }

    STD_TEST(RestoreColorTableRecoversAfterInvalidDefinition) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1bP2$p1;2;10;bad;30/"));
        fixture.feed(StringView(u8"2;2;40;50;60\x9c"));
        STD_INSIST(!fixture.iface.called("dcs_DECRSTS_HLS"));
        const ParserCall& call = fixture.iface.find("dcs_DECRSTS_RGB");
        expectValues(call, 2, 40, 50, 60);
    }

    STD_TEST(AssignColor) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[1;23;45,|\x1b[2;34;56,|"));
        expectValues(fixture.iface.find("csi_DECAC_TEXT"), 23, 45);
        expectValues(fixture.iface.find("csi_DECAC_FRAME"), 34, 56);
    }

    STD_TEST(ResetAssignedColor) {
        ParserFixture fixture;
        fixture.feed(StringView(u8"\x1b[1,|\x1b[2,|"));
        fixture.iface.find("csi_DECAC_TEXT_RESET");
        fixture.iface.find("csi_DECAC_FRAME_RESET");
    }

    STD_TEST(RejectMalformedAssignedColor) {
        {
            ParserFixture fixture;
            fixture.feed(StringView(u8"\x1b[0;12;34,|"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_TEXT"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_TEXT_RESET"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_FRAME"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_FRAME_RESET"));
        }
        {
            ParserFixture fixture;
            fixture.feed(StringView(u8"\x1b[1;12,|"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_TEXT"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_TEXT_RESET"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_FRAME"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_FRAME_RESET"));
        }
        {
            ParserFixture fixture;
            fixture.feed(StringView(u8"\x1b[1;256;0,|"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_TEXT"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_TEXT_RESET"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_FRAME"));
            STD_INSIST(!fixture.iface.called("csi_DECAC_FRAME_RESET"));
        }
    }
}

STD_TEST_SUITE(VteKnownSequences) {
    STD_TEST(SemanticDispatch) {
        size_t checked = 0;
        checked += checkVteKnown(vteKnownEscape);
        checked += checkVteKnown(vteKnownCsi);
        checked += checkVteKnown(vteKnownDcs);
        STD_INSIST(checked == sizeof(vteKnownDispatches) / sizeof(vteKnownDispatches[0]));
    }
}

#undef SHITTY_PARSER_TEXT_TEST
#undef SHITTY_PARSER_CALLBACK_TEST5
#undef SHITTY_PARSER_CALLBACK_TEST4
#undef SHITTY_PARSER_CALLBACK_TEST3
#undef SHITTY_PARSER_CALLBACK_TEST2
#undef SHITTY_PARSER_CALLBACK_TEST1
#undef SHITTY_PARSER_CALLBACK_TEST0
