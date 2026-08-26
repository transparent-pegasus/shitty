/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include "vterm.h"

#include <lib/vterm/color.h>
#include <lib/vterm/terminal_types.h>

#include <std/str/view.h>
#include <std/sys/types.h>
#include <std/lib/buffer.h>

#include <stddef.h>

namespace stl {
    struct ObjPool;
}

struct VtermTrace;

enum class CompatibilityLevel : u8 {
    VT52,
    VT100,
    VT200,
    VT300,
    VT400,
    VT500,
};

enum class Charset : u8 {
    UTF8,
    DecSpec,
    DecSuppl,
    DecUserPref,
    DecTechn,
    IsoLatin1,
    IsoUK,
    NrcDutch,
    NrcFinnish,
    NrcFrench,
    NrcFrenchCanadian,
    NrcGerman,
    NrcItalian,
    NrcNorwegianDanish,
    NrcPortuguese,
    NrcSpanish,
    NrcSwedish,
    NrcSwiss,
    NrcGreek,
    NrcHebrew,
    NrcRussian,
    NrcSerboCroatian,
    NrcTurkish,
};

struct ParserUdkDefinition {
    size_t valueOffset;
    size_t valueLength;
    plt::InputKey key;
};

struct CsiRectangle {
    u32 top;
    u32 left;
    u32 bottom;
    u32 right;
};

// A finished sixel image, valid only for the duration of the
// dcs_SIXEL() call. Pixels are rows of pitch bytes holding SixelPatch
// indexing: 0 transparent, n paints palette entry n - 1; the palette
// is SixelPatch::paletteBytes of RGB triplets.
struct ParserSixelImage {
    const u8* pixels;
    const u8* palette;
    u32 pitch;
    u32 width;
    u32 height;
};

struct ParserModeState {
    MouseTrackingMode mouseTracking;
    MouseTrackingEnc mouseEncoding;
    bool keyboardLocked;
    bool insertMode;
    bool eraseModeAll;
    bool localEcho;
    bool autoNewline;
    bool ansiMode;
    bool applicationCursorKeys;
    bool column132;
    bool smoothScroll;
    bool screenReverseVideo;
    bool originMode;
    bool autoWrap;
    bool autoRepeat;
    bool cursorBlink;
    bool allowColumnMode;
    bool moreFix;
    bool nationalReplacement;
    bool reverseWrap;
    bool showCursor;
    bool alternateScreen;
    bool applicationKeypad;
    bool backspaceSendsBackspace;
    bool horizontalMargins;
    bool noClearColumn;
    bool focusEvents;
    bool alternateScroll;
    bool eightBitInput;
    bool altSendsEscape;
    bool extendedReverseWrap;
    bool bracketedPaste;
    bool synchronizedOutput;
    bool graphemeCluster;
    bool colorSchemeUpdates;
    bool inBandResize;
    bool pasteMimeNotifications;
    bool savedCursor;
};

struct ParserIface {
    virtual void parserResetGraphemeInput() = 0;
    virtual void parserBell() = 0;
    virtual bool parserAutoNewlineMode() const = 0;
    virtual CompatibilityLevel parserCompatibilityLevel() const = 0;
    virtual void parserSetCompatibilityLevel(CompatibilityLevel level) = 0;
    virtual void parserSet8BitControls(bool enabled) = 0;
    virtual void parserSetApplicationKeypad(bool enabled) = 0;
    virtual void parserMoveCursorBackward(u32 count) = 0;
    virtual bool parserHexTitleInput() const = 0;
    virtual void parserSingleShift(u8 index) = 0;
    virtual void parserLockingShiftGl(u8 index) = 0;
    virtual void parserLockingShiftGr(u8 index) = 0;
    virtual void parserResetCharsets(bool isoLatin1) = 0;
    virtual void parserDesignateCharset(u8 index, Charset charset, u16 id, bool is96) = 0;
    virtual bool parserHighlightMouseTracking() const = 0;
    virtual bool windowOperationsAllowed() const = 0;
    virtual void parserWritePty(stl::StringView bytes) = 0;
    virtual bool parserGroundUtf8Enabled() const = 0;
    virtual void parserGroundHigh(u8 byte) = 0;
    virtual void parserGroundAscii(u8 byte) = 0;
    virtual bool parserUtf8BulkEligible() const = 0;
    virtual size_t parserPlaceAscii(stl::StringView bytes) = 0;
    // pendingTrace returns the count of continuation bytes of a sequence
    // the run aborted at its end that the ground trace layer still owes as
    // text; the parser carries it in groundUtf8Remaining.
    virtual size_t parserPlaceUtf8Run(stl::StringView bytes, u8& pendingTrace) = 0;

    virtual void unhandledInput(unsigned char byte) = 0;
    virtual void inp_CR() = 0;
    virtual void inp_HT() = 0;
    virtual bool esc_IND() = 0;
    virtual void esc_RI() = 0;
    virtual void esc_NEL() = 0;
    virtual void esc_BI() = 0;
    virtual void esc_FI() = 0;
    virtual void esc_HTS() = 0;
    virtual void esc_SPA() = 0;
    virtual void esc_EPA() = 0;
    virtual void esc_DECSC() = 0;
    virtual void esc_DECRC() = 0;
    virtual void esc_RIS() = 0;
    virtual void csi_DECSTR() = 0;
    virtual bool horizontalMarginMode() const = 0;
    virtual void csi_SLRM(u32 left, u32 right, bool valid) = 0;
    virtual void csi_SCORC() = 0;

    virtual void csi_CUU(u32 count) = 0;
    virtual void csi_CUD(u32 count) = 0;
    virtual void csi_CUF(u32 count) = 0;
    virtual void csi_CUB(u32 count) = 0;
    virtual void csi_CNL(u32 count) = 0;
    virtual void csi_CPL(u32 count) = 0;
    virtual void csi_CHA(u32 column) = 0;
    virtual void csi_HPA(u32 column) = 0;
    virtual void csi_HPR(u32 count) = 0;
    virtual void csi_VPA(u32 row) = 0;
    virtual void csi_VPR(u32 count) = 0;
    virtual void csi_CUP(u32 row, u32 column) = 0;
    virtual void csi_SU(u32 count) = 0;
    virtual void csi_SD(u32 count) = 0;
    virtual void csi_CHT(u32 count) = 0;
    virtual void csi_CBT(u32 count) = 0;
    virtual void csi_REP(u32 count) = 0;
    virtual void csi_ICH(u32 count) = 0;

    virtual void eraseDisplayAfter() = 0;
    virtual void eraseDisplayBefore() = 0;
    virtual void eraseDisplayAll() = 0;
    virtual void eraseScrollback() = 0;
    virtual void eraseLineAfter() = 0;
    virtual void eraseLineBefore() = 0;
    virtual void eraseLineAll() = 0;
    virtual void selectiveEraseDisplayAfter() = 0;
    virtual void selectiveEraseDisplayBefore() = 0;
    virtual void selectiveEraseDisplayAll() = 0;
    virtual void selectiveEraseLineAfter() = 0;
    virtual void selectiveEraseLineBefore() = 0;
    virtual void selectiveEraseLineAll() = 0;
    virtual void setDecProtection(bool enabled) = 0;
    virtual void csi_DECFRA(u32 codepoint, CsiRectangle rectangle) = 0;
    virtual void csi_DECCRA(CsiRectangle source, u32 targetRow, u32 targetColumn) = 0;
    virtual void csi_DECERA(CsiRectangle rectangle, bool selective) = 0;
    virtual void setAttributeChangeExtent(bool rectangular) = 0;
    virtual void changeRectangleAttributes(CsiRectangle rectangle, CellAttributeChange change) = 0;
    virtual void csi_XTCHECKSUM(u32 flags) = 0;
    virtual void csi_DECRQCRA(u32 requestId, CsiRectangle rectangle) = 0;
    virtual void csi_IL(u32 count) = 0;
    virtual void csi_DL(u32 count) = 0;
    virtual void csi_DCH(u32 count) = 0;
    virtual void csi_ECH(u32 count) = 0;
    virtual void csi_DECIC(u32 count) = 0;
    virtual void csi_DECDC(u32 count) = 0;
    virtual void csi_STBM(u32 top, u32 bottom, bool valid) = 0;
    virtual void clearTabStop() = 0;
    virtual void clearAllTabStops() = 0;
    virtual void resetTabStops() = 0;
    virtual ParserModeState parserModeState() const = 0;
    virtual void setKeyboardLocked(bool enabled) = 0;
    virtual void setInsertMode(bool enabled) = 0;
    virtual void setEraseModeAll(bool enabled) = 0;
    virtual void setLocalEcho(bool enabled) = 0;
    virtual void setAutoNewline(bool enabled) = 0;
    virtual void setAnsiMode(bool enabled) = 0;
    virtual void setApplicationCursorKeys(bool enabled) = 0;
    virtual void setColumn132(bool enabled) = 0;
    virtual void setSmoothScroll(bool enabled) = 0;
    virtual void setScreenReverseVideo(bool enabled) = 0;
    virtual void setOriginMode(bool enabled) = 0;
    virtual void setAutoWrap(bool enabled) = 0;
    virtual void setAutoRepeat(bool enabled) = 0;
    virtual void setAllowColumnMode(bool enabled) = 0;
    virtual void setMoreFix(bool enabled) = 0;
    virtual void setNationalReplacement(bool enabled) = 0;
    virtual void setReverseWrap(bool enabled) = 0;
    virtual void setMouseTracking(MouseTrackingMode mode) = 0;
    virtual void setCursorBlink(bool enabled) = 0;
    virtual void setCursorVisible(bool enabled) = 0;
    virtual void setAlternateScreen(bool enabled, bool clear) = 0;
    virtual void setBackspaceSendsBackspace(bool enabled) = 0;
    virtual void setHorizontalMargins(bool enabled) = 0;
    virtual void setNoClearColumn(bool enabled) = 0;
    virtual void setFocusEvents(bool enabled) = 0;
    virtual void setMouseEncoding(MouseTrackingEnc encoding, bool enabled) = 0;
    virtual void setAlternateScroll(bool enabled) = 0;
    virtual void setEightBitInput(bool enabled) = 0;
    virtual void setAltSendsEscape(bool enabled) = 0;
    virtual void setSavedAlternateScreen(bool enabled) = 0;
    virtual void setExtendedReverseWrap(bool enabled) = 0;
    virtual void setBracketedPaste(bool enabled) = 0;
    virtual void setSynchronizedOutput(bool enabled) = 0;
    virtual void setGraphemeCluster(bool enabled) = 0;
    virtual void setColorSchemeUpdates(bool enabled) = 0;
    virtual void setInBandResize(bool enabled) = 0;
    virtual void setPasteMimeNotifications(bool enabled) = 0;
    virtual void savePrivateMode(u32 mode, bool enabled) = 0;
    virtual bool restorePrivateMode(u32 mode, bool& enabled) const = 0;
    virtual void reportMode(u32 mode, bool privateMode, u8 state) = 0;
    virtual void csi_ecma48_SL(u32 count) = 0;
    virtual void csi_ecma48_SR(u32 count) = 0;
    virtual void setCursorStyle(u8 reportStyle, TerminalCursor::Style shape, bool blink) = 0;
    virtual void refreshCursorStyle() = 0;

    virtual void csi_priDA() = 0;
    virtual void csi_secDA() = 0;
    virtual void csi_terDA() = 0;
    virtual void csi_DECRQDE() = 0;
    virtual void csi_DECREQTPARM(u32 permission) = 0;
    virtual void csi_DECRQTSR_COLOR(u32 model) = 0;
    virtual void csi_DECRQPSR_TABS() = 0;
    virtual void csi_DECRQPSR_CURSOR() = 0;
    virtual void csi_DECRQUPSS() = 0;
    virtual void dsrOperatingStatus() = 0;
    virtual void dsrCursorPosition(bool privateMode) = 0;
    virtual void dsrPrinter() = 0;
    virtual void dsrUserDefinedKeys() = 0;
    virtual void dsrKeyboard() = 0;
    virtual void dsrLocator() = 0;
    virtual void dsrLocatorType() = 0;
    virtual void dsrMacroSpace() = 0;
    virtual void dsrMemoryChecksum(u32 requestId) = 0;
    virtual void dsrDataIntegrity() = 0;
    virtual void dsrMultipleSession() = 0;
    virtual void dsrColorScheme() = 0;
    virtual void sgrReset() = 0;
    virtual void sgrBold(bool enabled) = 0;
    virtual void sgrFaint(bool enabled) = 0;
    virtual void sgrItalic(bool enabled) = 0;
    virtual void sgrUnderline(u8 style) = 0;
    virtual void sgrBlink(bool enabled) = 0;
    virtual void sgrInverse(bool enabled) = 0;
    virtual void sgrConceal(bool enabled) = 0;
    virtual void sgrStrike(bool enabled) = 0;
    virtual void sgrOverline(bool enabled) = 0;
    virtual void sgrForeground(CellColor color, int paletteIndex, bool brightenBold) = 0;
    virtual void sgrDefaultForeground() = 0;
    virtual void sgrBackground(CellColor color, int paletteIndex) = 0;
    virtual void sgrDefaultBackground() = 0;
    virtual void sgrUnderlineColor(CellColor color, int paletteIndex) = 0;
    virtual void sgrDefaultUnderlineColor() = 0;
    virtual void sgrFinish() = 0;
    virtual void csi_XTPUSHSGR(const u32* attributes, size_t count) = 0;
    virtual void csi_XTPOPSGR() = 0;
    virtual void esch_DECALN() = 0;
    virtual void setLineAttribute(u8 attribute) = 0;

    virtual void osc_TITLE_0(stl::StringView payload) = 0;
    virtual void osc_TITLE_1(stl::StringView payload) = 0;
    virtual void osc_TITLE_2(stl::StringView payload) = 0;
    virtual void osc_PALETTE(u32 index, Color color, bool query) = 0;
    virtual void osc_SPECIAL_COLOR(u32 index, Color color, bool query) = 0;
    virtual void osc_SPECIAL_COLOR_MODE(u32 index, u32 mode) = 0;
    virtual void osc_RAW(u32 command, stl::StringView payload) = 0;
    virtual void osc_CWD(stl::StringView path, bool valid) = 0;
    virtual void osc_HYPERLINK(stl::StringView id, bool hasId, stl::StringView uri) = 0;
    virtual void osc_NOTIFY(stl::StringView payload) = 0;
    virtual void osc_PROGRESS(u32 state, u32 percent, bool percentPresent) = 0;
    virtual void osc_DEFAULT_FOREGROUND(Color color, bool query) = 0;
    virtual void osc_DEFAULT_BACKGROUND(Color color, bool query) = 0;
    virtual void osc_CURSOR_COLOR(Color color, bool query) = 0;
    virtual void osc_SELECTION_BACKGROUND(Color color, bool query) = 0;
    virtual void osc_SELECTION_FOREGROUND(Color color, bool query) = 0;
    virtual void osc_CLIPBOARD_QUERY(bool primary, bool clipboard, u8 replySelector, bool selectorsEmpty) = 0;
    virtual void osc_CLIPBOARD_WRITE(stl::StringView content, bool valid, bool primary, bool clipboard) = 0;
    virtual void osc_KITTY_CLIPBOARD_READ(stl::StringView id, stl::StringView mimeTypes, bool primary, bool valid) = 0;
    virtual void osc_KITTY_CLIPBOARD_WRITE(stl::StringView id, bool primary) = 0;
    virtual void osc_KITTY_CLIPBOARD_WRITE_DATA(stl::StringView id, stl::StringView mimeType, stl::StringView content, bool valid) = 0;
    virtual void osc_KITTY_CLIPBOARD_WRITE_ALIAS(stl::StringView id, stl::StringView mimeType, stl::StringView aliases, bool valid) = 0;
    virtual void osc_KITTY_CLIPBOARD_INVALID(stl::StringView id, bool write) = 0;
    virtual void osc_NOTIFICATION_CAPABILITIES(stl::StringView payload) = 0;
    virtual void osc_NOTIFICATION_CLOSE(stl::StringView id) = 0;
    virtual void osc_NOTIFICATION_TITLE(stl::StringView id, stl::StringView content, bool encoded, bool final) = 0;
    virtual void osc_NOTIFICATION_BODY(stl::StringView id, stl::StringView content, bool encoded, bool final) = 0;
    virtual void osc_RESET_PALETTE() = 0;
    virtual void osc_RESET_PALETTE(u32 index) = 0;
    virtual void osc_RESET_SPECIAL_COLOR() = 0;
    virtual void osc_RESET_SPECIAL_COLOR(u32 index) = 0;
    virtual void osc_RESET_DEFAULT_FOREGROUND() = 0;
    virtual void osc_RESET_DEFAULT_BACKGROUND() = 0;
    virtual void osc_RESET_CURSOR_COLOR() = 0;
    virtual void osc_RESET_SELECTION_BACKGROUND() = 0;
    virtual void osc_RESET_SELECTION_FOREGROUND() = 0;
    virtual void osc_SHELL_A(stl::StringView payload) = 0;
    virtual void osc_SHELL_B(stl::StringView payload) = 0;
    virtual void osc_SHELL_C(stl::StringView payload) = 0;
    virtual void osc_SHELL_D(stl::StringView payload) = 0;
    virtual void osc_SHELL_I(stl::StringView payload) = 0;
    virtual void osc_SHELL_L(stl::StringView payload) = 0;
    virtual void osc_SHELL_N(stl::StringView payload) = 0;
    virtual void osc_SHELL_P(stl::StringView payload) = 0;
    virtual void osc_SHELL_UNKNOWN(stl::StringView payload) = 0;
    virtual void osc_UNKNOWN(u32 command, stl::StringView payload) = 0;

    virtual void csi_DECSCL(CompatibilityLevel level, bool send8BitControls) = 0;
    virtual void xtResizePixels(u32 height, bool heightPresent, u32 width, bool widthPresent) = 0;
    virtual void xtResizeCells(u32 height, bool heightPresent, u32 width, bool widthPresent) = 0;
    virtual void xtWindowOperation(u32 operation, u32 first, u32 second) = 0;
    virtual void xtReportWindowState() = 0;
    virtual void xtReportWindowPosition() = 0;
    virtual void xtReportWindowPixelSize(bool compositorSize) = 0;
    virtual void xtReportScreenPixelSize() = 0;
    virtual void xtReportCellSize() = 0;
    virtual void xtReportGridSize() = 0;
    virtual void xtReportScreenGridSize() = 0;
    virtual void xtReportIconTitle() = 0;
    virtual void xtReportWindowTitle() = 0;
    virtual void xtPushTitle(bool icon, bool window) = 0;
    virtual void xtPopTitle(bool icon, bool window) = 0;
    virtual void xtResizeRows(u32 rows) = 0;
    virtual void resetTitleModes() = 0;
    virtual void setTitleMode(u8 bit, bool enabled) = 0;
    virtual void csi_XTHIMOUSE(u32 start, u32 startX, u32 startY, u32 firstRow, u32 lastRow) = 0;
    virtual void setLocatorReporting(bool enabled, bool oneShot, bool pixels) = 0;
    virtual void resetLocatorEvents() = 0;
    virtual void setLocatorButtonDown(bool enabled) = 0;
    virtual void setLocatorButtonUp(bool enabled) = 0;
    virtual void csi_DECRQLP() = 0;
    virtual void csi_DECEFR(u32 top, u32 left, u32 bottom, u32 right) = 0;
    virtual void csi_DECAC_TEXT(u8 foreground, u8 background) = 0;
    virtual void csi_DECAC_TEXT_RESET() = 0;
    virtual void csi_DECAC_FRAME(u8 foreground, u8 background) = 0;
    virtual void csi_DECAC_FRAME_RESET() = 0;
    virtual void resetModifyKeyResources() = 0;
    virtual void setModifyKeyResource(u8 resource, u8 value, bool useDefault) = 0;
    virtual void reportModifyKeyResource(u8 resource) = 0;
    virtual void csi_kittyKeyboardPush(u32 flags) = 0;
    virtual void csi_kittyKeyboardPop(u32 count) = 0;
    virtual void setKittyKeyboardFlags(u8 flags) = 0;
    virtual void addKittyKeyboardFlags(u8 flags) = 0;
    virtual void removeKittyKeyboardFlags(u8 flags) = 0;
    virtual void csi_kittyKeyboardQuery() = 0;
    virtual void csi_XTVERSION() = 0;
    virtual void csi_XTSMGRAPHICS(u32 item, u32 action, u32 value) = 0;
    virtual void csi_SETMARK() = 0;
    virtual void resetLeds() = 0;
    virtual void setLed(u8 index, bool enabled) = 0;
    virtual void commitLeds() = 0;

    virtual void dcs_DECRQSS_DECSCL() = 0;
    virtual void dcs_DECRQSS_SGR() = 0;
    virtual void dcs_DECRQSS_DECSTBM() = 0;
    virtual void dcs_DECRQSS_DECSLRM() = 0;
    virtual void dcs_DECRQSS_DECSLPP() = 0;
    virtual void dcs_DECRQSS_DECSCUSR() = 0;
    virtual void dcs_DECRQSS_DECSCA() = 0;
    virtual void dcs_DECRQSS_DECSACE() = 0;
    virtual void dcs_DECRQSS_UNKNOWN() = 0;
    virtual void dcs_XTGETTCAP(stl::StringView encoded, stl::StringView value) = 0;
    virtual void dcs_DECUDK(bool clearDefinitions, bool lockDefinitions, const ParserUdkDefinition* definitions, size_t definitionCount, stl::StringView values) = 0;
    virtual void dcs_DECRSTS_HLS(u32 index, u32 hue, u32 luminosity, u32 saturation) = 0;
    virtual void dcs_DECRSTS_RGB(u32 index, u32 red, u32 green, u32 blue) = 0;
    virtual void dcs_DECRSTS_TABS_BEGIN() = 0;
    virtual void dcs_DECRSTS_TAB(u32 column) = 0;
    virtual void dcs_DECRSTS_CURSOR(u32 row, u32 column, u8 rendition, u8 protection, u8 flags, u8 gl, u8 gr, u8 sizeFlags, const Charset* charsets, const u16* charsetIds) = 0;
    virtual void dcs_DECAUPSS(Charset charset, u16 id, bool is96) = 0;
    virtual void dcs_SIXEL(const ParserSixelImage& image) = 0;
};

struct Parser {
    static Parser* create(stl::ObjPool* pool, ParserIface& iface, VtermTrace* trace, bool osc52SelectClipboard);

    virtual void reset() = 0;
    virtual void feed(stl::StringView bytes) = 0;
    virtual void setOsc52SelectClipboard(bool clipboard) = 0;
};
