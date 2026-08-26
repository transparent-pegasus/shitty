/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

// Feeds synthetic input through the production VT parser with no-op
// callbacks: measures the parser alone, without vterm or a screen
// behind it. Usage: parser_perf <text|random> [mebibytes].

#include <lib/vterm/num.h>
#include <lib/vterm/parser.h>

#include <std/ios/sys.h>
#include <std/mem/obj_pool.h>
#include <std/str/view.h>

#include <time.h>

using namespace stl;

namespace {
    struct NoopParserIface final: public ParserIface {
        size_t asciiBytes = 0;
        size_t utf8Bytes = 0;
        size_t groundBytes = 0;
        size_t unhandledBytes = 0;

        void parserResetGraphemeInput() override;
        void parserBell() override;
        bool parserAutoNewlineMode() const override;
        CompatibilityLevel parserCompatibilityLevel() const override;
        void parserSetCompatibilityLevel(CompatibilityLevel level) override;
        void parserSet8BitControls(bool enabled) override;
        void parserSetApplicationKeypad(bool enabled) override;
        void parserMoveCursorBackward(u32 count) override;
        bool parserHexTitleInput() const override;
        void parserSingleShift(u8 index) override;
        void parserLockingShiftGl(u8 index) override;
        void parserLockingShiftGr(u8 index) override;
        void parserResetCharsets(bool isoLatin1) override;
        void parserDesignateCharset(u8 index, Charset charset, u16 id, bool is96) override;
        bool parserHighlightMouseTracking() const override;
        bool windowOperationsAllowed() const override;
        void parserWritePty(stl::StringView bytes) override;
        bool parserGroundUtf8Enabled() const override;
        void parserGroundHigh(u8 byte) override;
        void parserGroundAscii(u8 byte) override;
        bool parserUtf8BulkEligible() const override;
        size_t parserPlaceAscii(stl::StringView bytes) override;
        size_t parserPlaceUtf8Run(stl::StringView bytes, u8& pendingTrace) override;
        void unhandledInput(unsigned char byte) override;
        void inp_CR() override;
        void inp_HT() override;
        bool esc_IND() override;
        void esc_RI() override;
        void esc_NEL() override;
        void esc_BI() override;
        void esc_FI() override;
        void esc_HTS() override;
        void esc_SPA() override;
        void esc_EPA() override;
        void esc_DECSC() override;
        void esc_DECRC() override;
        void esc_RIS() override;
        void csi_DECSTR() override;
        bool horizontalMarginMode() const override;
        void csi_SLRM(u32 left, u32 right, bool valid) override;
        void csi_SCORC() override;
        void csi_CUU(u32 count) override;
        void csi_CUD(u32 count) override;
        void csi_CUF(u32 count) override;
        void csi_CUB(u32 count) override;
        void csi_CNL(u32 count) override;
        void csi_CPL(u32 count) override;
        void csi_CHA(u32 column) override;
        void csi_HPA(u32 column) override;
        void csi_HPR(u32 count) override;
        void csi_VPA(u32 row) override;
        void csi_VPR(u32 count) override;
        void csi_CUP(u32 row, u32 column) override;
        void csi_SU(u32 count) override;
        void csi_SD(u32 count) override;
        void csi_CHT(u32 count) override;
        void csi_CBT(u32 count) override;
        void csi_REP(u32 count) override;
        void csi_ICH(u32 count) override;
        void eraseDisplayAfter() override;
        void eraseDisplayBefore() override;
        void eraseDisplayAll() override;
        void eraseScrollback() override;
        void eraseLineAfter() override;
        void eraseLineBefore() override;
        void eraseLineAll() override;
        void selectiveEraseDisplayAfter() override;
        void selectiveEraseDisplayBefore() override;
        void selectiveEraseDisplayAll() override;
        void selectiveEraseLineAfter() override;
        void selectiveEraseLineBefore() override;
        void selectiveEraseLineAll() override;
        void setDecProtection(bool enabled) override;
        void csi_DECFRA(u32 codepoint, CsiRectangle rectangle) override;
        void csi_DECCRA(CsiRectangle source, u32 targetRow, u32 targetColumn) override;
        void csi_DECERA(CsiRectangle rectangle, bool selective) override;
        void setAttributeChangeExtent(bool rectangular) override;
        void changeRectangleAttributes(CsiRectangle rectangle, CellAttributeChange change) override;
        void csi_XTCHECKSUM(u32 flags) override;
        void csi_DECRQCRA(u32 requestId, CsiRectangle rectangle) override;
        void csi_IL(u32 count) override;
        void csi_DL(u32 count) override;
        void csi_DCH(u32 count) override;
        void csi_ECH(u32 count) override;
        void csi_DECIC(u32 count) override;
        void csi_DECDC(u32 count) override;
        void csi_STBM(u32 top, u32 bottom, bool valid) override;
        void clearTabStop() override;
        void clearAllTabStops() override;
        void resetTabStops() override;
        ParserModeState parserModeState() const override;
        void setKeyboardLocked(bool enabled) override;
        void setInsertMode(bool enabled) override;
        void setEraseModeAll(bool enabled) override;
        void setLocalEcho(bool enabled) override;
        void setAutoNewline(bool enabled) override;
        void setAnsiMode(bool enabled) override;
        void setApplicationCursorKeys(bool enabled) override;
        void setColumn132(bool enabled) override;
        void setSmoothScroll(bool enabled) override;
        void setScreenReverseVideo(bool enabled) override;
        void setOriginMode(bool enabled) override;
        void setAutoWrap(bool enabled) override;
        void setAutoRepeat(bool enabled) override;
        void setAllowColumnMode(bool enabled) override;
        void setMoreFix(bool enabled) override;
        void setNationalReplacement(bool enabled) override;
        void setReverseWrap(bool enabled) override;
        void setMouseTracking(MouseTrackingMode mode) override;
        void setCursorBlink(bool enabled) override;
        void setCursorVisible(bool enabled) override;
        void setAlternateScreen(bool enabled, bool clear) override;
        void setBackspaceSendsBackspace(bool enabled) override;
        void setHorizontalMargins(bool enabled) override;
        void setNoClearColumn(bool enabled) override;
        void setFocusEvents(bool enabled) override;
        void setMouseEncoding(MouseTrackingEnc encoding, bool enabled) override;
        void setAlternateScroll(bool enabled) override;
        void setEightBitInput(bool enabled) override;
        void setAltSendsEscape(bool enabled) override;
        void setSavedAlternateScreen(bool enabled) override;
        void setExtendedReverseWrap(bool enabled) override;
        void setBracketedPaste(bool enabled) override;
        void setSynchronizedOutput(bool enabled) override;
        void setGraphemeCluster(bool enabled) override;
        void setColorSchemeUpdates(bool enabled) override;
        void setInBandResize(bool enabled) override;
        void setPasteMimeNotifications(bool enabled) override;
        void savePrivateMode(u32 mode, bool enabled) override;
        bool restorePrivateMode(u32 mode, bool& enabled) const override;
        void reportMode(u32 mode, bool privateMode, u8 state) override;
        void csi_ecma48_SL(u32 count) override;
        void csi_ecma48_SR(u32 count) override;
        void setCursorStyle(u8 reportStyle, TerminalCursor::Style shape, bool blink) override;
        void refreshCursorStyle() override;
        void csi_priDA() override;
        void csi_secDA() override;
        void csi_terDA() override;
        void csi_DECRQDE() override;
        void csi_DECREQTPARM(u32 permission) override;
        void csi_DECRQTSR_COLOR(u32 model) override;
        void csi_DECRQPSR_TABS() override;
        void csi_DECRQPSR_CURSOR() override;
        void csi_DECRQUPSS() override;
        void dsrOperatingStatus() override;
        void dsrCursorPosition(bool privateMode) override;
        void dsrPrinter() override;
        void dsrUserDefinedKeys() override;
        void dsrKeyboard() override;
        void dsrLocator() override;
        void dsrLocatorType() override;
        void dsrMacroSpace() override;
        void dsrMemoryChecksum(u32 requestId) override;
        void dsrDataIntegrity() override;
        void dsrMultipleSession() override;
        void dsrColorScheme() override;
        void sgrReset() override;
        void sgrBold(bool enabled) override;
        void sgrFaint(bool enabled) override;
        void sgrItalic(bool enabled) override;
        void sgrUnderline(u8 style) override;
        void sgrBlink(bool enabled) override;
        void sgrInverse(bool enabled) override;
        void sgrConceal(bool enabled) override;
        void sgrStrike(bool enabled) override;
        void sgrOverline(bool enabled) override;
        void sgrForeground(CellColor color, int paletteIndex, bool brightenBold) override;
        void sgrDefaultForeground() override;
        void sgrBackground(CellColor color, int paletteIndex) override;
        void sgrDefaultBackground() override;
        void sgrUnderlineColor(CellColor color, int paletteIndex) override;
        void sgrDefaultUnderlineColor() override;
        void sgrFinish() override;
        void csi_XTPUSHSGR(const u32* attributes, size_t count) override;
        void csi_XTPOPSGR() override;
        void esch_DECALN() override;
        void setLineAttribute(u8 attribute) override;
        void osc_TITLE_0(stl::StringView payload) override;
        void osc_TITLE_1(stl::StringView payload) override;
        void osc_TITLE_2(stl::StringView payload) override;
        void osc_PALETTE(u32 index, Color color, bool query) override;
        void osc_SPECIAL_COLOR(u32 index, Color color, bool query) override;
        void osc_SPECIAL_COLOR_MODE(u32 index, u32 mode) override;
        void osc_RAW(u32 command, stl::StringView payload) override;
        void osc_CWD(stl::StringView path, bool valid) override;
        void osc_HYPERLINK(stl::StringView id, bool hasId, stl::StringView uri) override;
        void osc_NOTIFY(stl::StringView payload) override;
        void osc_PROGRESS(u32 state, u32 percent, bool percentPresent) override;
        void osc_DEFAULT_FOREGROUND(Color color, bool query) override;
        void osc_DEFAULT_BACKGROUND(Color color, bool query) override;
        void osc_CURSOR_COLOR(Color color, bool query) override;
        void osc_SELECTION_BACKGROUND(Color color, bool query) override;
        void osc_SELECTION_FOREGROUND(Color color, bool query) override;
        void osc_CLIPBOARD_QUERY(bool primary, bool clipboard, u8 replySelector, bool selectorsEmpty) override;
        void osc_CLIPBOARD_WRITE(stl::StringView content, bool valid, bool primary, bool clipboard) override;
        void osc_KITTY_CLIPBOARD_READ(stl::StringView id, stl::StringView mimeTypes, bool primary, bool valid) override;
        void osc_KITTY_CLIPBOARD_WRITE(stl::StringView id, bool primary) override;
        void osc_KITTY_CLIPBOARD_WRITE_DATA(stl::StringView id, stl::StringView mimeType, stl::StringView content, bool valid) override;
        void osc_KITTY_CLIPBOARD_WRITE_ALIAS(stl::StringView id, stl::StringView mimeType, stl::StringView aliases, bool valid) override;
        void osc_KITTY_CLIPBOARD_INVALID(stl::StringView id, bool write) override;
        void osc_NOTIFICATION_CAPABILITIES(stl::StringView payload) override;
        void osc_NOTIFICATION_CLOSE(stl::StringView id) override;
        void osc_NOTIFICATION_TITLE(stl::StringView id, stl::StringView content, bool encoded, bool final) override;
        void osc_NOTIFICATION_BODY(stl::StringView id, stl::StringView content, bool encoded, bool final) override;
        void osc_RESET_PALETTE() override;
        void osc_RESET_PALETTE(u32 index) override;
        void osc_RESET_SPECIAL_COLOR() override;
        void osc_RESET_SPECIAL_COLOR(u32 index) override;
        void osc_RESET_DEFAULT_FOREGROUND() override;
        void osc_RESET_DEFAULT_BACKGROUND() override;
        void osc_RESET_CURSOR_COLOR() override;
        void osc_RESET_SELECTION_BACKGROUND() override;
        void osc_RESET_SELECTION_FOREGROUND() override;
        void osc_SHELL_A(stl::StringView payload) override;
        void osc_SHELL_B(stl::StringView payload) override;
        void osc_SHELL_C(stl::StringView payload) override;
        void osc_SHELL_D(stl::StringView payload) override;
        void osc_SHELL_I(stl::StringView payload) override;
        void osc_SHELL_L(stl::StringView payload) override;
        void osc_SHELL_N(stl::StringView payload) override;
        void osc_SHELL_P(stl::StringView payload) override;
        void osc_SHELL_UNKNOWN(stl::StringView payload) override;
        void osc_UNKNOWN(u32 command, stl::StringView payload) override;
        void csi_DECSCL(CompatibilityLevel level, bool send8BitControls) override;
        void xtResizePixels(u32 height, bool heightPresent, u32 width, bool widthPresent) override;
        void xtResizeCells(u32 height, bool heightPresent, u32 width, bool widthPresent) override;
        void xtWindowOperation(u32 operation, u32 first, u32 second) override;
        void xtReportWindowState() override;
        void xtReportWindowPosition() override;
        void xtReportWindowPixelSize(bool compositorSize) override;
        void xtReportScreenPixelSize() override;
        void xtReportCellSize() override;
        void xtReportGridSize() override;
        void xtReportScreenGridSize() override;
        void xtReportIconTitle() override;
        void xtReportWindowTitle() override;
        void xtPushTitle(bool icon, bool window) override;
        void xtPopTitle(bool icon, bool window) override;
        void xtResizeRows(u32 rows) override;
        void resetTitleModes() override;
        void setTitleMode(u8 bit, bool enabled) override;
        void csi_XTHIMOUSE(u32 start, u32 startX, u32 startY, u32 firstRow, u32 lastRow) override;
        void setLocatorReporting(bool enabled, bool oneShot, bool pixels) override;
        void resetLocatorEvents() override;
        void setLocatorButtonDown(bool enabled) override;
        void setLocatorButtonUp(bool enabled) override;
        void csi_DECRQLP() override;
        void csi_DECEFR(u32 top, u32 left, u32 bottom, u32 right) override;
        void csi_DECAC_TEXT(u8 foreground, u8 background) override;
        void csi_DECAC_TEXT_RESET() override;
        void csi_DECAC_FRAME(u8 foreground, u8 background) override;
        void csi_DECAC_FRAME_RESET() override;
        void resetModifyKeyResources() override;
        void setModifyKeyResource(u8 resource, u8 value, bool useDefault) override;
        void reportModifyKeyResource(u8 resource) override;
        void csi_kittyKeyboardPush(u32 flags) override;
        void csi_kittyKeyboardPop(u32 count) override;
        void setKittyKeyboardFlags(u8 flags) override;
        void addKittyKeyboardFlags(u8 flags) override;
        void removeKittyKeyboardFlags(u8 flags) override;
        void csi_kittyKeyboardQuery() override;
        void csi_XTVERSION() override;
        void csi_XTSMGRAPHICS(u32 item, u32 action, u32 value) override;
        void csi_SETMARK() override;
        void resetLeds() override;
        void setLed(u8 index, bool enabled) override;
        void commitLeds() override;
        void dcs_DECRQSS_DECSCL() override;
        void dcs_DECRQSS_SGR() override;
        void dcs_DECRQSS_DECSTBM() override;
        void dcs_DECRQSS_DECSLRM() override;
        void dcs_DECRQSS_DECSLPP() override;
        void dcs_DECRQSS_DECSCUSR() override;
        void dcs_DECRQSS_DECSCA() override;
        void dcs_DECRQSS_DECSACE() override;
        void dcs_DECRQSS_UNKNOWN() override;
        void dcs_XTGETTCAP(stl::StringView encoded, stl::StringView value) override;
        void dcs_DECUDK(bool clearDefinitions, bool lockDefinitions, const ParserUdkDefinition* definitions, size_t definitionCount, stl::StringView values) override;
        void dcs_DECRSTS_HLS(u32 index, u32 hue, u32 luminosity, u32 saturation) override;
        void dcs_DECRSTS_RGB(u32 index, u32 red, u32 green, u32 blue) override;
        void dcs_DECRSTS_TABS_BEGIN() override;
        void dcs_DECRSTS_TAB(u32 column) override;
        void dcs_DECRSTS_CURSOR(u32 row, u32 column, u8 rendition, u8 protection, u8 flags, u8 gl, u8 gr, u8 sizeFlags, const Charset* charsets, const u16* charsetIds) override;
        void dcs_DECAUPSS(Charset charset, u16 id, bool is96) override;
        void dcs_SIXEL(const ParserSixelImage& image) override;
    };
}

void NoopParserIface::parserResetGraphemeInput() {
}

void NoopParserIface::parserBell() {
}

bool NoopParserIface::parserAutoNewlineMode() const {
    return false;
}

CompatibilityLevel NoopParserIface::parserCompatibilityLevel() const {
    return CompatibilityLevel::VT500;
}

void NoopParserIface::parserSetCompatibilityLevel(CompatibilityLevel level) {
}

void NoopParserIface::parserSet8BitControls(bool enabled) {
}

void NoopParserIface::parserSetApplicationKeypad(bool enabled) {
}

void NoopParserIface::parserMoveCursorBackward(u32 count) {
}

bool NoopParserIface::parserHexTitleInput() const {
    return false;
}

void NoopParserIface::parserSingleShift(u8 index) {
}

void NoopParserIface::parserLockingShiftGl(u8 index) {
}

void NoopParserIface::parserLockingShiftGr(u8 index) {
}

void NoopParserIface::parserResetCharsets(bool isoLatin1) {
}

void NoopParserIface::parserDesignateCharset(u8 index, Charset charset, u16 id, bool is96) {
}

bool NoopParserIface::parserHighlightMouseTracking() const {
    return false;
}

bool NoopParserIface::windowOperationsAllowed() const {
    return false;
}

void NoopParserIface::parserWritePty(stl::StringView bytes) {
}

bool NoopParserIface::parserGroundUtf8Enabled() const {
    return true;
}

void NoopParserIface::parserGroundHigh(u8 byte) {
    groundBytes += 1;
}

void NoopParserIface::parserGroundAscii(u8 byte) {
    groundBytes += 1;
}

bool NoopParserIface::parserUtf8BulkEligible() const {
    return true;
}

size_t NoopParserIface::parserPlaceAscii(stl::StringView bytes) {
    // Mirror the vterm contract: consume the printable prefix, hand
    // everything else back to the state machine.
    size_t count = 0;
    while (count < bytes.length() && bytes[count] >= 0x20 && bytes[count] <= 0x7e) {
        ++count;
    }
    asciiBytes += count;
    return count;
}

size_t NoopParserIface::parserPlaceUtf8Run(stl::StringView bytes, u8& pendingTrace) {
    pendingTrace = 0;
    size_t count = 0;
    while (count < bytes.length() && bytes[count] >= 0x20 && bytes[count] != 0x7f) {
        ++count;
    }
    utf8Bytes += count;
    return count;
}

void NoopParserIface::unhandledInput(unsigned char byte) {
    unhandledBytes += 1;
}

void NoopParserIface::inp_CR() {
}

void NoopParserIface::inp_HT() {
}

bool NoopParserIface::esc_IND() {
    return false;
}

void NoopParserIface::esc_RI() {
}

void NoopParserIface::esc_NEL() {
}

void NoopParserIface::esc_BI() {
}

void NoopParserIface::esc_FI() {
}

void NoopParserIface::esc_HTS() {
}

void NoopParserIface::esc_SPA() {
}

void NoopParserIface::esc_EPA() {
}

void NoopParserIface::esc_DECSC() {
}

void NoopParserIface::esc_DECRC() {
}

void NoopParserIface::esc_RIS() {
}

void NoopParserIface::csi_DECSTR() {
}

bool NoopParserIface::horizontalMarginMode() const {
    return false;
}

void NoopParserIface::csi_SLRM(u32 left, u32 right, bool valid) {
}

void NoopParserIface::csi_SCORC() {
}

void NoopParserIface::csi_CUU(u32 count) {
}

void NoopParserIface::csi_CUD(u32 count) {
}

void NoopParserIface::csi_CUF(u32 count) {
}

void NoopParserIface::csi_CUB(u32 count) {
}

void NoopParserIface::csi_CNL(u32 count) {
}

void NoopParserIface::csi_CPL(u32 count) {
}

void NoopParserIface::csi_CHA(u32 column) {
}

void NoopParserIface::csi_HPA(u32 column) {
}

void NoopParserIface::csi_HPR(u32 count) {
}

void NoopParserIface::csi_VPA(u32 row) {
}

void NoopParserIface::csi_VPR(u32 count) {
}

void NoopParserIface::csi_CUP(u32 row, u32 column) {
}

void NoopParserIface::csi_SU(u32 count) {
}

void NoopParserIface::csi_SD(u32 count) {
}

void NoopParserIface::csi_CHT(u32 count) {
}

void NoopParserIface::csi_CBT(u32 count) {
}

void NoopParserIface::csi_REP(u32 count) {
}

void NoopParserIface::csi_ICH(u32 count) {
}

void NoopParserIface::eraseDisplayAfter() {
}

void NoopParserIface::eraseDisplayBefore() {
}

void NoopParserIface::eraseDisplayAll() {
}

void NoopParserIface::eraseScrollback() {
}

void NoopParserIface::eraseLineAfter() {
}

void NoopParserIface::eraseLineBefore() {
}

void NoopParserIface::eraseLineAll() {
}

void NoopParserIface::selectiveEraseDisplayAfter() {
}

void NoopParserIface::selectiveEraseDisplayBefore() {
}

void NoopParserIface::selectiveEraseDisplayAll() {
}

void NoopParserIface::selectiveEraseLineAfter() {
}

void NoopParserIface::selectiveEraseLineBefore() {
}

void NoopParserIface::selectiveEraseLineAll() {
}

void NoopParserIface::setDecProtection(bool enabled) {
}

void NoopParserIface::csi_DECFRA(u32 codepoint, CsiRectangle rectangle) {
}

void NoopParserIface::csi_DECCRA(CsiRectangle source, u32 targetRow, u32 targetColumn) {
}

void NoopParserIface::csi_DECERA(CsiRectangle rectangle, bool selective) {
}

void NoopParserIface::setAttributeChangeExtent(bool rectangular) {
}

void NoopParserIface::changeRectangleAttributes(CsiRectangle rectangle, CellAttributeChange change) {
}

void NoopParserIface::csi_XTCHECKSUM(u32 flags) {
}

void NoopParserIface::csi_DECRQCRA(u32 requestId, CsiRectangle rectangle) {
}

void NoopParserIface::csi_IL(u32 count) {
}

void NoopParserIface::csi_DL(u32 count) {
}

void NoopParserIface::csi_DCH(u32 count) {
}

void NoopParserIface::csi_ECH(u32 count) {
}

void NoopParserIface::csi_DECIC(u32 count) {
}

void NoopParserIface::csi_DECDC(u32 count) {
}

void NoopParserIface::csi_STBM(u32 top, u32 bottom, bool valid) {
}

void NoopParserIface::clearTabStop() {
}

void NoopParserIface::clearAllTabStops() {
}

void NoopParserIface::resetTabStops() {
}

ParserModeState NoopParserIface::parserModeState() const {
    return ParserModeState{};
}

void NoopParserIface::setKeyboardLocked(bool enabled) {
}

void NoopParserIface::setInsertMode(bool enabled) {
}

void NoopParserIface::setEraseModeAll(bool enabled) {
}

void NoopParserIface::setLocalEcho(bool enabled) {
}

void NoopParserIface::setAutoNewline(bool enabled) {
}

void NoopParserIface::setAnsiMode(bool enabled) {
}

void NoopParserIface::setApplicationCursorKeys(bool enabled) {
}

void NoopParserIface::setColumn132(bool enabled) {
}

void NoopParserIface::setSmoothScroll(bool enabled) {
}

void NoopParserIface::setScreenReverseVideo(bool enabled) {
}

void NoopParserIface::setOriginMode(bool enabled) {
}

void NoopParserIface::setAutoWrap(bool enabled) {
}

void NoopParserIface::setAutoRepeat(bool enabled) {
}

void NoopParserIface::setAllowColumnMode(bool enabled) {
}

void NoopParserIface::setMoreFix(bool enabled) {
}

void NoopParserIface::setNationalReplacement(bool enabled) {
}

void NoopParserIface::setReverseWrap(bool enabled) {
}

void NoopParserIface::setMouseTracking(MouseTrackingMode mode) {
}

void NoopParserIface::setCursorBlink(bool enabled) {
}

void NoopParserIface::setCursorVisible(bool enabled) {
}

void NoopParserIface::setAlternateScreen(bool enabled, bool clear) {
}

void NoopParserIface::setBackspaceSendsBackspace(bool enabled) {
}

void NoopParserIface::setHorizontalMargins(bool enabled) {
}

void NoopParserIface::setNoClearColumn(bool enabled) {
}

void NoopParserIface::setFocusEvents(bool enabled) {
}

void NoopParserIface::setMouseEncoding(MouseTrackingEnc encoding, bool enabled) {
}

void NoopParserIface::setAlternateScroll(bool enabled) {
}

void NoopParserIface::setEightBitInput(bool enabled) {
}

void NoopParserIface::setAltSendsEscape(bool enabled) {
}

void NoopParserIface::setSavedAlternateScreen(bool enabled) {
}

void NoopParserIface::setExtendedReverseWrap(bool enabled) {
}

void NoopParserIface::setBracketedPaste(bool enabled) {
}

void NoopParserIface::setSynchronizedOutput(bool enabled) {
}

void NoopParserIface::setGraphemeCluster(bool enabled) {
}

void NoopParserIface::setColorSchemeUpdates(bool enabled) {
}

void NoopParserIface::setInBandResize(bool enabled) {
}

void NoopParserIface::setPasteMimeNotifications(bool enabled) {
}

void NoopParserIface::savePrivateMode(u32 mode, bool enabled) {
}

bool NoopParserIface::restorePrivateMode(u32 mode, bool& enabled) const {
    return false;
}

void NoopParserIface::reportMode(u32 mode, bool privateMode, u8 state) {
}

void NoopParserIface::csi_ecma48_SL(u32 count) {
}

void NoopParserIface::csi_ecma48_SR(u32 count) {
}

void NoopParserIface::setCursorStyle(u8 reportStyle, TerminalCursor::Style shape, bool blink) {
}

void NoopParserIface::refreshCursorStyle() {
}

void NoopParserIface::csi_priDA() {
}

void NoopParserIface::csi_secDA() {
}

void NoopParserIface::csi_terDA() {
}

void NoopParserIface::csi_DECRQDE() {
}

void NoopParserIface::csi_DECREQTPARM(u32 permission) {
}

void NoopParserIface::csi_DECRQTSR_COLOR(u32 model) {
}

void NoopParserIface::csi_DECRQPSR_TABS() {
}

void NoopParserIface::csi_DECRQPSR_CURSOR() {
}

void NoopParserIface::csi_DECRQUPSS() {
}

void NoopParserIface::dsrOperatingStatus() {
}

void NoopParserIface::dsrCursorPosition(bool privateMode) {
}

void NoopParserIface::dsrPrinter() {
}

void NoopParserIface::dsrUserDefinedKeys() {
}

void NoopParserIface::dsrKeyboard() {
}

void NoopParserIface::dsrLocator() {
}

void NoopParserIface::dsrLocatorType() {
}

void NoopParserIface::dsrMacroSpace() {
}

void NoopParserIface::dsrMemoryChecksum(u32 requestId) {
}

void NoopParserIface::dsrDataIntegrity() {
}

void NoopParserIface::dsrMultipleSession() {
}

void NoopParserIface::dsrColorScheme() {
}

void NoopParserIface::sgrReset() {
}

void NoopParserIface::sgrBold(bool enabled) {
}

void NoopParserIface::sgrFaint(bool enabled) {
}

void NoopParserIface::sgrItalic(bool enabled) {
}

void NoopParserIface::sgrUnderline(u8 style) {
}

void NoopParserIface::sgrBlink(bool enabled) {
}

void NoopParserIface::sgrInverse(bool enabled) {
}

void NoopParserIface::sgrConceal(bool enabled) {
}

void NoopParserIface::sgrStrike(bool enabled) {
}

void NoopParserIface::sgrOverline(bool enabled) {
}

void NoopParserIface::sgrForeground(CellColor color, int paletteIndex, bool brightenBold) {
}

void NoopParserIface::sgrDefaultForeground() {
}

void NoopParserIface::sgrBackground(CellColor color, int paletteIndex) {
}

void NoopParserIface::sgrDefaultBackground() {
}

void NoopParserIface::sgrUnderlineColor(CellColor color, int paletteIndex) {
}

void NoopParserIface::sgrDefaultUnderlineColor() {
}

void NoopParserIface::sgrFinish() {
}

void NoopParserIface::csi_XTPUSHSGR(const u32* attributes, size_t count) {
}

void NoopParserIface::csi_XTPOPSGR() {
}

void NoopParserIface::esch_DECALN() {
}

void NoopParserIface::setLineAttribute(u8 attribute) {
}

void NoopParserIface::osc_TITLE_0(stl::StringView payload) {
}

void NoopParserIface::osc_TITLE_1(stl::StringView payload) {
}

void NoopParserIface::osc_TITLE_2(stl::StringView payload) {
}

void NoopParserIface::osc_PALETTE(u32 index, Color color, bool query) {
}

void NoopParserIface::osc_SPECIAL_COLOR(u32 index, Color color, bool query) {
}

void NoopParserIface::osc_SPECIAL_COLOR_MODE(u32 index, u32 mode) {
}

void NoopParserIface::osc_RAW(u32 command, stl::StringView payload) {
}

void NoopParserIface::osc_CWD(stl::StringView path, bool valid) {
}

void NoopParserIface::osc_HYPERLINK(stl::StringView id, bool hasId, stl::StringView uri) {
}

void NoopParserIface::osc_NOTIFY(stl::StringView payload) {
}

void NoopParserIface::osc_PROGRESS(u32 state, u32 percent, bool percentPresent) {
}

void NoopParserIface::osc_DEFAULT_FOREGROUND(Color color, bool query) {
}

void NoopParserIface::osc_DEFAULT_BACKGROUND(Color color, bool query) {
}

void NoopParserIface::osc_CURSOR_COLOR(Color color, bool query) {
}

void NoopParserIface::osc_SELECTION_BACKGROUND(Color color, bool query) {
}

void NoopParserIface::osc_SELECTION_FOREGROUND(Color color, bool query) {
}

void NoopParserIface::osc_CLIPBOARD_QUERY(bool primary, bool clipboard, u8 replySelector, bool selectorsEmpty) {
}

void NoopParserIface::osc_CLIPBOARD_WRITE(stl::StringView content, bool valid, bool primary, bool clipboard) {
}

void NoopParserIface::osc_KITTY_CLIPBOARD_READ(stl::StringView id, stl::StringView mimeTypes, bool primary, bool valid) {
}

void NoopParserIface::osc_KITTY_CLIPBOARD_WRITE(stl::StringView id, bool primary) {
}

void NoopParserIface::osc_KITTY_CLIPBOARD_WRITE_DATA(stl::StringView id, stl::StringView mimeType, stl::StringView content, bool valid) {
}

void NoopParserIface::osc_KITTY_CLIPBOARD_WRITE_ALIAS(stl::StringView id, stl::StringView mimeType, stl::StringView aliases, bool valid) {
}

void NoopParserIface::osc_KITTY_CLIPBOARD_INVALID(stl::StringView id, bool write) {
}

void NoopParserIface::osc_NOTIFICATION_CAPABILITIES(stl::StringView payload) {
}

void NoopParserIface::osc_NOTIFICATION_CLOSE(stl::StringView id) {
}

void NoopParserIface::osc_NOTIFICATION_TITLE(stl::StringView id, stl::StringView content, bool encoded, bool final) {
}

void NoopParserIface::osc_NOTIFICATION_BODY(stl::StringView id, stl::StringView content, bool encoded, bool final) {
}

void NoopParserIface::osc_RESET_PALETTE() {
}

void NoopParserIface::osc_RESET_PALETTE(u32 index) {
}

void NoopParserIface::osc_RESET_SPECIAL_COLOR() {
}

void NoopParserIface::osc_RESET_SPECIAL_COLOR(u32 index) {
}

void NoopParserIface::osc_RESET_DEFAULT_FOREGROUND() {
}

void NoopParserIface::osc_RESET_DEFAULT_BACKGROUND() {
}

void NoopParserIface::osc_RESET_CURSOR_COLOR() {
}

void NoopParserIface::osc_RESET_SELECTION_BACKGROUND() {
}

void NoopParserIface::osc_RESET_SELECTION_FOREGROUND() {
}

void NoopParserIface::osc_SHELL_A(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_B(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_C(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_D(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_I(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_L(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_N(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_P(stl::StringView payload) {
}

void NoopParserIface::osc_SHELL_UNKNOWN(stl::StringView payload) {
}

void NoopParserIface::osc_UNKNOWN(u32 command, stl::StringView payload) {
}

void NoopParserIface::csi_DECSCL(CompatibilityLevel level, bool send8BitControls) {
}

void NoopParserIface::xtResizePixels(u32 height, bool heightPresent, u32 width, bool widthPresent) {
}

void NoopParserIface::xtResizeCells(u32 height, bool heightPresent, u32 width, bool widthPresent) {
}

void NoopParserIface::xtWindowOperation(u32 operation, u32 first, u32 second) {
}

void NoopParserIface::xtReportWindowState() {
}

void NoopParserIface::xtReportWindowPosition() {
}

void NoopParserIface::xtReportWindowPixelSize(bool compositorSize) {
}

void NoopParserIface::xtReportScreenPixelSize() {
}

void NoopParserIface::xtReportCellSize() {
}

void NoopParserIface::xtReportGridSize() {
}

void NoopParserIface::xtReportScreenGridSize() {
}

void NoopParserIface::xtReportIconTitle() {
}

void NoopParserIface::xtReportWindowTitle() {
}

void NoopParserIface::xtPushTitle(bool icon, bool window) {
}

void NoopParserIface::xtPopTitle(bool icon, bool window) {
}

void NoopParserIface::xtResizeRows(u32 rows) {
}

void NoopParserIface::resetTitleModes() {
}

void NoopParserIface::setTitleMode(u8 bit, bool enabled) {
}

void NoopParserIface::csi_XTHIMOUSE(u32 start, u32 startX, u32 startY, u32 firstRow, u32 lastRow) {
}

void NoopParserIface::setLocatorReporting(bool enabled, bool oneShot, bool pixels) {
}

void NoopParserIface::resetLocatorEvents() {
}

void NoopParserIface::setLocatorButtonDown(bool enabled) {
}

void NoopParserIface::setLocatorButtonUp(bool enabled) {
}

void NoopParserIface::csi_DECRQLP() {
}

void NoopParserIface::csi_DECEFR(u32 top, u32 left, u32 bottom, u32 right) {
}

void NoopParserIface::csi_DECAC_TEXT(u8 foreground, u8 background) {
}

void NoopParserIface::csi_DECAC_TEXT_RESET() {
}

void NoopParserIface::csi_DECAC_FRAME(u8 foreground, u8 background) {
}

void NoopParserIface::csi_DECAC_FRAME_RESET() {
}

void NoopParserIface::resetModifyKeyResources() {
}

void NoopParserIface::setModifyKeyResource(u8 resource, u8 value, bool useDefault) {
}

void NoopParserIface::reportModifyKeyResource(u8 resource) {
}

void NoopParserIface::csi_kittyKeyboardPush(u32 flags) {
}

void NoopParserIface::csi_kittyKeyboardPop(u32 count) {
}

void NoopParserIface::setKittyKeyboardFlags(u8 flags) {
}

void NoopParserIface::addKittyKeyboardFlags(u8 flags) {
}

void NoopParserIface::removeKittyKeyboardFlags(u8 flags) {
}

void NoopParserIface::csi_kittyKeyboardQuery() {
}

void NoopParserIface::csi_XTVERSION() {
}

void NoopParserIface::csi_XTSMGRAPHICS(u32 item, u32 action, u32 value) {
}

void NoopParserIface::csi_SETMARK() {
}

void NoopParserIface::resetLeds() {
}

void NoopParserIface::setLed(u8 index, bool enabled) {
}

void NoopParserIface::commitLeds() {
}

void NoopParserIface::dcs_DECRQSS_DECSCL() {
}

void NoopParserIface::dcs_DECRQSS_SGR() {
}

void NoopParserIface::dcs_DECRQSS_DECSTBM() {
}

void NoopParserIface::dcs_DECRQSS_DECSLRM() {
}

void NoopParserIface::dcs_DECRQSS_DECSLPP() {
}

void NoopParserIface::dcs_DECRQSS_DECSCUSR() {
}

void NoopParserIface::dcs_DECRQSS_DECSCA() {
}

void NoopParserIface::dcs_DECRQSS_DECSACE() {
}

void NoopParserIface::dcs_DECRQSS_UNKNOWN() {
}

void NoopParserIface::dcs_XTGETTCAP(stl::StringView encoded, stl::StringView value) {
}

void NoopParserIface::dcs_DECUDK(bool clearDefinitions, bool lockDefinitions, const ParserUdkDefinition* definitions, size_t definitionCount, stl::StringView values) {
}

void NoopParserIface::dcs_DECRSTS_HLS(u32 index, u32 hue, u32 luminosity, u32 saturation) {
}

void NoopParserIface::dcs_DECRSTS_RGB(u32 index, u32 red, u32 green, u32 blue) {
}

void NoopParserIface::dcs_DECRSTS_TABS_BEGIN() {
}

void NoopParserIface::dcs_DECRSTS_TAB(u32 column) {
}

void NoopParserIface::dcs_DECRSTS_CURSOR(u32 row, u32 column, u8 rendition, u8 protection, u8 flags, u8 gl, u8 gr, u8 sizeFlags, const Charset* charsets, const u16* charsetIds) {
}

void NoopParserIface::dcs_DECAUPSS(Charset charset, u16 id, bool is96) {
}

void NoopParserIface::dcs_SIXEL(const ParserSixelImage& image) {
}

namespace {
    constexpr size_t bufferBytes = 4u << 20;
    constexpr size_t chunkBytes = 64u << 10;

    static void fillText(u8* data, size_t length) {
        static const char sample[] = "drwxr-xr-x 12 pg users  4096 Aug  6 01:23 render_synthesis.cpp\n"
                                     "The quick brown fox jumps over the lazy dog; съешь ещё этих мягких булок, да выпей же чаю.\n"
                                     "0123456789 !\"#$%&'()*+,-./ :;<=>?@ [\\]^_` {|}~ αβγδε 界面测试 ライン\n";
        const size_t sampleLength = sizeof(sample) - 1;
        for (size_t at = 0; at < length; ++at) {
            data[at] = (u8)(sample[at % sampleLength]);
        }
    }

    static void fillRandom(u8* data, size_t length) {
        u64 state = 0x9e3779b97f4a7c15ull;
        for (size_t at = 0; at < length; ++at) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            data[at] = (u8)(state >> 56);
        }
    }

    static double now() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (double)(ts.tv_sec) + (double)(ts.tv_nsec) / 1e9;
    }
}

int main(int argc, char** argv) {
    const StringView mode(argc > 1 ? argv[1] : "text");
    i64 mebibytes = 200;
    if (argc > 2 && (!parseI64(StringView(argv[2]), mebibytes) || mebibytes <= 0)) {
        sysE << StringView(u8"usage: parser_perf <text|random> [mebibytes]") << endL;
        return 2;
    }

    stl::ObjPool::Ref pool = stl::ObjPool::fromMemory();
    NoopParserIface iface;
    Parser* parser = Parser::create(pool.mutPtr(), iface, nullptr, false);

    u8* data = (u8*)(pool->allocate(bufferBytes));
    if (mode == StringView(u8"random")) {
        fillRandom(data, bufferBytes);
    } else if (mode == StringView(u8"text")) {
        fillText(data, bufferBytes);
    } else {
        sysE << StringView(u8"usage: parser_perf <text|random> [mebibytes]") << endL;
        return 2;
    }

    const size_t total = (size_t)(mebibytes) << 20;
    const double start = now();
    size_t fed = 0;
    while (fed < total) {
        const size_t offset = fed % bufferBytes;
        size_t length = bufferBytes - offset;
        if (length > chunkBytes) {
            length = chunkBytes;
        }
        if (length > total - fed) {
            length = total - fed;
        }
        parser->feed(StringView(data + offset, length));
        fed += length;
    }
    const double elapsed = now() - start;

    sysO << StringView(u8"ascii=") << (i64)(iface.asciiBytes) << StringView(u8" utf8=") << (i64)(iface.utf8Bytes) << StringView(u8" ground=") << (i64)(iface.groundBytes) << StringView(u8" unhandled=") << (i64)(iface.unhandledBytes) << endL;
    const i64 rate = (i64)((double)(mebibytes) / elapsed);
    sysO << StringView(u8"parsed ") << mebibytes << StringView(u8" MiB of ") << mode << StringView(u8" in ") << (i64)(elapsed * 1e6) << StringView(u8" us, ") << rate << StringView(u8" MiB/s") << endL;
    return 0;
}
