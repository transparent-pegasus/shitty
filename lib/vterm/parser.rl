/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

%%{
    machine parser;
    alphtype unsigned char;

    action groundDone {
        fbreak;
    }

    action returnGround {
        fnext main;
        fbreak;
    }

    action cancel {
        parser.stringUtf8Remaining = 0;
        parser.stringLimit = 0;
        if constexpr (traced) {
            parserTrace->control(fc);
            parserTrace->stringCancel();
            parserTrace->escapeCancel();
        }
        fnext main;
        fbreak;
    }

    action beginEscape {
        iface.parserResetGraphemeInput();
        parser.stringUtf8Remaining = 0;
        parser.stringLimit = 0;
        if constexpr (traced) {
            parserTrace->stringCancel();
            parserTrace->escapeCancel();
            parserTrace->escapeBegin();
        }
        parser.parameters[0] = 0;
        parser.separators[0] = 0;
        parser.parameterCount = 1;
        if (iface.parserCompatibilityLevel() == CompatibilityLevel::VT52) {
            fgoto escapeVt52;
        }
        fgoto escape;
    }

    action abortStringEscaped {
        iface.parserResetGraphemeInput();
        parser.stringUtf8Remaining = 0;
        parser.stringLimit = 0;
        if constexpr (traced) {
            parserTrace->stringCancel();
            parserTrace->escapeCancel();
            parserTrace->escapeBegin();
        }
        parser.parameters[0] = 0;
        parser.separators[0] = 0;
        parser.parameterCount = 1;
        fhold;
        if (iface.parserCompatibilityLevel() == CompatibilityLevel::VT52) {
            fgoto escapeVt52;
        }
        fgoto escape;
    }

    action repeatEscape {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->escapeBegin();
        }
        parser.parameters[0] = 0;
        parser.separators[0] = 0;
        parser.parameterCount = 1;
    }

    action beginCsi {
        beginCsi();
        fgoto csiEntry;
    }

    action beginDcs {
        ragelBeginDcs();
        fgoto dcsEntry;
    }

    action beginOsc {
        ragelBeginOsc();
        fgoto oscCommand;
    }

    action beginSos {
        ragelBeginString(VtermTraceString::Sos, false);
        fgoto string;
    }

    action beginPm {
        ragelBeginString(VtermTraceString::Pm, false);
        fgoto string;
    }

    action beginApc {
        ragelBeginString(VtermTraceString::Apc, false);
        fgoto string;
    }

    action c1Ind {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.esc_IND();
        fnext main;
        fbreak;
    }

    action c1Nel {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.esc_NEL();
        fnext main;
        fbreak;
    }

    action c1Hts {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.esc_HTS();
        fnext main;
        fbreak;
    }

    action c1Ri {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.esc_RI();
        fnext main;
        fbreak;
    }

    action c1Ss2 {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.parserSingleShift(2);
        fnext main;
        fbreak;
    }

    action c1Ss3 {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.parserSingleShift(3);
        fnext main;
        fbreak;
    }

    action c1Spa {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.esc_SPA();
        fnext main;
        fbreak;
    }

    action c1Epa {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.esc_EPA();
        fnext main;
        fbreak;
    }

    action c1Da {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        iface.csi_priDA();
        fnext main;
        fbreak;
    }

    action c1St {
        if constexpr (traced) {
            parserTrace->escapeCancel();
            parserTrace->control(fc);
        }
        fnext main;
        fbreak;
    }

    action sequenceC0 {
        executeC0(fc);
    }

    action dcsPayloadC0 {
        consumeStringUtf8Byte(fc);
        ragelAppendString(fc, parser.maxDcsBytes);
    }

    action c1ToGround {
        if constexpr (traced) {
            parserTrace->escapeCancel();
        }
        fhold;
        fgoto main;
    }

    action highToGround {
        if constexpr (traced) {
            parserTrace->escapeCancel();
        }
        if (iface.parserGroundUtf8Enabled() && fc >= 0xc2 && fc <= 0xdf) {
            parser.discardedUtf8Remaining = 1;
        } else if (iface.parserGroundUtf8Enabled() && fc >= 0xe0 && fc <= 0xef) {
            parser.discardedUtf8Remaining = 2;
        } else if (iface.parserGroundUtf8Enabled() && fc >= 0xf0 && fc <= 0xf4) {
            parser.discardedUtf8Remaining = 3;
        } else {
            fhold;
            fgoto main;
        }
        fgoto discardUtf8;
    }

    action groundIgnored {
        groundControl(fc);
        fbreak;
    }

    action groundBell {
        groundControl(fc);
        fbreak;
    }

    action groundBackspace {
        groundControl(fc);
        fbreak;
    }

    action groundTab {
        groundControl(fc);
        fbreak;
    }

    action groundLineFeed {
        groundControl(fc);
        fbreak;
    }

    action groundCarriageReturn {
        groundControl(fc);
        fbreak;
    }

    action groundShiftOut {
        groundControl(fc);
        fbreak;
    }

    action groundShiftIn {
        groundControl(fc);
        fbreak;
    }

    action groundAscii {
        ragelGroundAscii(fc);
        fbreak;
    }

    action groundHigh {
        ragelGroundHigh(fc);
        fbreak;
    }

    action groundC1Ind {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.esc_IND();
        fbreak;
    }

    action groundC1Nel {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.esc_NEL();
        fbreak;
    }

    action groundC1Hts {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.esc_HTS();
        fbreak;
    }

    action groundC1Ri {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.esc_RI();
        fbreak;
    }

    action groundC1Ss2 {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.parserSingleShift(2);
        fbreak;
    }

    action groundC1Ss3 {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.parserSingleShift(3);
        fbreak;
    }

    action groundC1Dcs {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        ragelBeginDcs();
        fgoto dcsEntry;
    }

    action groundC1Spa {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.esc_SPA();
        fbreak;
    }

    action groundC1Epa {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.esc_EPA();
        fbreak;
    }

    action groundC1Sos {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        ragelBeginString(VtermTraceString::Sos, false);
        fgoto string;
    }

    action groundC1Da {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        iface.csi_priDA();
        fbreak;
    }

    action groundC1Csi {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        beginCsi();
        fgoto csiEntry;
    }

    action groundC1St {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        if constexpr (traced) {
            parserTrace->control(fc);
        }
        fbreak;
    }

    action groundC1Osc {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        ragelBeginOsc();
        fgoto oscCommand;
    }

    action groundC1Pm {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        ragelBeginString(VtermTraceString::Pm, false);
        fgoto string;
    }

    action groundC1Apc {
        if (ragelGroundContinuation(fc)) {
            fbreak;
        }
        iface.parserResetGraphemeInput();
        ragelBeginString(VtermTraceString::Apc, false);
        fgoto string;
    }

    action escapeIntermediate {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        fgoto escapeIntermediate;
    }

    action escapeFinal {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.unhandledInput(fc);
        fnext main;
        fbreak;
    }

    action escapeC0 {
        executeC0(fc);
    }

    action escapeSpace {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        fgoto escapeSpace;
    }

    action escapeHash {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        fgoto escapeHash;
    }

    action escapePercent {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        fgoto escapePercent;
    }

    action charsetG0 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 0;
        parser.scsMod = 0;
        parser.scs96 = false;
        parser.scsMultibyte = false;
        fgoto selectCharset;
    }

    action charsetG0Multibyte {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 0;
        parser.scsMod = 0;
        parser.scs96 = false;
        parser.scsMultibyte = true;
        fgoto selectCharset;
    }

    action charsetG1 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 1;
        parser.scsMod = 0;
        parser.scs96 = false;
        parser.scsMultibyte = false;
        fgoto selectCharset;
    }

    action charsetG2 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 2;
        parser.scsMod = 0;
        parser.scs96 = false;
        parser.scsMultibyte = false;
        fgoto selectCharset;
    }

    action charsetG3 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 3;
        parser.scsMod = 0;
        parser.scs96 = false;
        parser.scsMultibyte = false;
        fgoto selectCharset;
    }

    action charsetG1_96 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 1;
        parser.scsMod = 0;
        parser.scs96 = true;
        parser.scsMultibyte = false;
        fgoto selectCharset;
    }

    action charsetG2_96 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 2;
        parser.scsMod = 0;
        parser.scs96 = true;
        parser.scsMultibyte = false;
        fgoto selectCharset;
    }

    action charsetG3_96 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsIndex = 3;
        parser.scsMod = 0;
        parser.scs96 = true;
        parser.scsMultibyte = false;
        fgoto selectCharset;
    }

    action escapeStringTerminator {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        fnext main;
        fbreak;
    }

    action escapeInd {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_IND();
        fnext main;
        fbreak;
    }

    action escapeRi {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_RI();
        fnext main;
        fbreak;
    }

    action escapeNel {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_NEL();
        fnext main;
        fbreak;
    }

    action escapeHts {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_HTS();
        fnext main;
        fbreak;
    }

    action escapeSs2 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserSingleShift(2);
        fnext main;
        fbreak;
    }

    action escapeSs3 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserSingleShift(3);
        fnext main;
        fbreak;
    }

    action escapeSpa {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_SPA();
        fnext main;
        fbreak;
    }

    action escapeEpa {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_EPA();
        fnext main;
        fbreak;
    }

    action escapeDa {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.csi_priDA();
        fnext main;
        fbreak;
    }

    action escapeRis {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_RIS();
        fnext main;
        fbreak;
    }

    action escapeBi {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_BI();
        fnext main;
        fbreak;
    }

    action escapeDecsc {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_DECSC();
        fnext main;
        fbreak;
    }

    action escapeDecrc {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_DECRC();
        fnext main;
        fbreak;
    }

    action escapeFi {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.esc_FI();
        fnext main;
        fbreak;
    }

    action escapeAppKeypad {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserSetApplicationKeypad(true);
        fnext main;
        fbreak;
    }

    action escapeNormalKeypad {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserSetApplicationKeypad(false);
        fnext main;
        fbreak;
    }

    action escapeAnsi {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserSetCompatibilityLevel(CompatibilityLevel::VT400);
        fnext main;
        fbreak;
    }

    action escapeLs1r {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserLockingShiftGr(1);
        fnext main;
        fbreak;
    }

    action escapeLs2 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserLockingShiftGl(2);
        fnext main;
        fbreak;
    }

    action escapeLs2r {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserLockingShiftGr(2);
        fnext main;
        fbreak;
    }

    action escapeLs3 {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserLockingShiftGl(3);
        fnext main;
        fbreak;
    }

    action escapeLs3r {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        iface.parserLockingShiftGr(3);
        fnext main;
        fbreak;
    }

    action intermediateByte {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
    }

    action intermediateFinal {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        fnext main;
        fbreak;
    }

    action specialIntermediate {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        fgoto escapeIntermediate;
    }

    action specialFinal {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
    }

    action charsetModifier {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
        }
        parser.scsMod = fc;
    }

    action charsetFinal {
        if constexpr (traced) {
            parserTrace->escapeByte(fc);
            parserTrace->escapeEnd();
        }
        fnext main;
        fbreak;
    }

    action csiDigit {
        parser.csiHadParameters = true;
        parser.present[parser.parameterCount - 1] = true;
        if (parser.parameters[parser.parameterCount - 1] > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.parameters[parser.parameterCount - 1] = UINT32_MAX;
        } else {
            parser.parameters[parser.parameterCount - 1] = parser.parameters[parser.parameterCount - 1] * 10 + fc - '0';
        }
    }

    action csiSeparator {
        if (parser.parameterCount >= parser.maxParameters) {
            fgoto csiIgnore;
        }
        parser.csiHadParameters = true;
        parser.separators[parser.parameterCount] = fc;
        parser.parameters[parser.parameterCount] = 0;
        parser.present[parser.parameterCount] = false;
        ++parser.parameterCount;
    }

    action csiPrefix {
        if (parser.csiPrefix != 0) {
            fgoto csiIgnore;
        }
        parser.csiPrefix = fc;
    }

    action csiIntermediate {
        if (parser.csiIntermediateCount >= sizeof(parser.csiIntermediates)) {
            fgoto csiIgnore;
        }
        parser.csiIntermediates[parser.csiIntermediateCount++] = fc;
    }

    action csiTrace {
        traceCsi(fc);
    }

    action csiSgr {
        dispatchSgr();
    }

    action csiDone {
        fnext main;
        fbreak;
    }

    action csiFinalSelect {
        fhold;
        if (parser.csiPrefix == '>') {
            fgoto csiGreaterDispatch;
        }
        if (parser.csiPrefix == '<') {
            fgoto csiLessDispatch;
        }
        if (parser.csiPrefix == '=') {
            fgoto csiEqualDispatch;
        }
        if (parser.csiPrefix == '?') {
            fgoto csiQuestionDispatch;
        }
        fgoto csiPlainDispatch;
    }

    action csiIntermediateFinalSelect {
        fhold;
        if (parser.csiIntermediateCount != 1) {
            fgoto csiUnknownDispatch;
        }
        if (parser.csiPrefix == '?' && parser.csiIntermediates[0] == '$') {
            fgoto csiQuestionDollarDispatch;
        }
        if (parser.csiPrefix != 0) {
            fgoto csiUnknownDispatch;
        }
        if (parser.csiIntermediates[0] == '!') {
            fgoto csiBangDispatch;
        }
        if (parser.csiIntermediates[0] == '"') {
            fgoto csiQuoteDispatch;
        }
        if (parser.csiIntermediates[0] == ' ') {
            fgoto csiSpaceDispatch;
        }
        if (parser.csiIntermediates[0] == '\'') {
            fgoto csiApostropheDispatch;
        }
        if (parser.csiIntermediates[0] == '$') {
            fgoto csiDollarDispatch;
        }
        if (parser.csiIntermediates[0] == '*') {
            fgoto csiStarDispatch;
        }
        if (parser.csiIntermediates[0] == ',') {
            fgoto csiCommaDispatch;
        }
        if (parser.csiIntermediates[0] == '#') {
            fgoto csiHashDispatch;
        }
        if (parser.csiIntermediates[0] == '&') {
            fgoto csiAmpersandDispatch;
        }
        fgoto csiUnknownDispatch;
    }

    action csiInvalid {
        fgoto csiIgnore;
    }

    action csiDispatchInvalid {
        fhold;
        fgoto csiIgnore;
    }

    action csiIgnoredFinal {
        if constexpr (traced) {
            parserTrace->escapeCancel();
        }
        fnext main;
        fbreak;
    }

    action vt52AppKeypad {
        traceVt52Byte(fc, true);
        iface.parserSetApplicationKeypad(true);
        fnext main;
        fbreak;
    }

    action vt52NormalKeypad {
        traceVt52Byte(fc, true);
        iface.parserSetApplicationKeypad(false);
        fnext main;
        fbreak;
    }

    action vt52Ansi {
        traceVt52Byte(fc, true);
        iface.parserSetCompatibilityLevel(CompatibilityLevel::VT100);
        fnext main;
        fbreak;
    }

    action vt52Cuu {
        traceVt52Byte(fc, true);
        iface.csi_CUU(1);
        fnext main;
        fbreak;
    }

    action vt52Cud {
        traceVt52Byte(fc, true);
        iface.csi_CUD(1);
        fnext main;
        fbreak;
    }

    action vt52Cuf {
        traceVt52Byte(fc, true);
        iface.csi_CUF(1);
        fnext main;
        fbreak;
    }

    action vt52Cub {
        traceVt52Byte(fc, true);
        iface.csi_CUB(1);
        fnext main;
        fbreak;
    }

    action vt52Graphics {
        traceVt52Byte(fc, true);
        iface.parserResetCharsets(false);
        iface.parserDesignateCharset(0, Charset::DecSpec, '0', false);
        fnext main;
        fbreak;
    }

    action vt52Ascii {
        traceVt52Byte(fc, true);
        iface.parserResetCharsets(false);
        fnext main;
        fbreak;
    }

    action vt52Cup {
        traceVt52Byte(fc, true);
        iface.csi_CUP(parameter(0), parameter(1));
        fnext main;
        fbreak;
    }

    action vt52Ri {
        traceVt52Byte(fc, true);
        iface.esc_RI();
        fnext main;
        fbreak;
    }

    action vt52Ed {
        traceVt52Byte(fc, true);
        iface.eraseDisplayAfter();
        fnext main;
        fbreak;
    }

    action vt52El {
        traceVt52Byte(fc, true);
        iface.eraseLineAfter();
        fnext main;
        fbreak;
    }

    action vt52CupBegin {
        traceVt52Byte(fc, false);
        fgoto vt52CupRow;
    }

    action vt52Identify {
        traceVt52Byte(fc, true);
        iface.parserWritePty(StringView(u8"\x1b/Z"));
        fnext main;
        fbreak;
    }

    action vt52Ris {
        traceVt52Byte(fc, true);
        iface.esc_RIS();
        fnext main;
        fbreak;
    }

    action vt52Unhandled {
        traceVt52Byte(fc, true);
        iface.unhandledInput(fc);
        fnext main;
        fbreak;
    }

    action vt52Row {
        traceVt52Byte(fc, false);
        parser.parameters[0] = fc - 31;
        fgoto vt52CupColumn;
    }

    action vt52Column {
        traceVt52Byte(fc, true);
        parser.parameters[1] = fc - 31;
        parser.parameterCount = 2;
        iface.csi_CUP(parameter(0), parameter(1));
        fnext main;
        fbreak;
    }

    action vt52CupInvalid {
        fhold;
        fgoto main;
    }

    action dcsHeaderByte {
        ragelAppendString(fc, parser.maxDcsBytes);
    }

    action dcsDigit {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.present[parser.parameterCount - 1] = true;
        if (parser.parameters[parser.parameterCount - 1] > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.parameters[parser.parameterCount - 1] = UINT32_MAX;
        } else {
            parser.parameters[parser.parameterCount - 1] = parser.parameters[parser.parameterCount - 1] * 10 + fc - '0';
        }
    }

    action dcsSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.parameterCount >= parser.maxParameters) {
            if constexpr (traced) {
                parserTrace->stringCancel();
            }
            fgoto dcsIgnore;
        }
        parser.separators[parser.parameterCount] = fc;
        parser.parameters[parser.parameterCount] = 0;
        parser.present[parser.parameterCount] = false;
        ++parser.parameterCount;
    }

    action dcsIntermediate {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.dcsIntermediateCount >= sizeof(parser.dcsIntermediates)) {
            if constexpr (traced) {
                parserTrace->stringCancel();
            }
            fgoto dcsIgnore;
        }
        parser.dcsIntermediates[parser.dcsIntermediateCount++] = fc;
    }

    action dcsFinal {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.dcsIntermediateCount == 1 && parser.dcsIntermediates[0] == '$' && fc == 'q') {
            fgoto dcsDecrqssEntry;
        } else if (parser.dcsIntermediateCount == 1 && parser.dcsIntermediates[0] == '+' && fc == 'q') {
            parser.dcsCapabilityOffset = ragelStringSize();
            parser.dcsCapabilityDecodedLength = 0;
            parser.dcsCapabilityCandidates = 0x0f;
            parser.dcsCapabilityHasHighNibble = false;
            parser.dcsCapabilityValid = true;
            parser.dcsCapabilityComplete = false;
            fgoto dcsXtgettcap;
        } else if (parser.dcsIntermediateCount == 0 && fc == '|') {
            const bool secondParameterValid =
                parser.parameterCount < 2 || !parser.present[1] || parser.parameters[1] <= 1;
            parser.dcsUdkHeaderValid =
                parser.parameterCount <= 2 &&
                (!parser.present[0] || parser.parameters[0] <= 1) &&
                secondParameterValid &&
                (parser.parameterCount < 2 || parser.separators[1] == ';');
            parser.dcsUdkClearDefinitions =
                !parser.present[0] || parser.parameters[0] == 0;
            parser.dcsUdkLockDefinitions =
                parser.parameterCount < 2 || !parser.present[1] || parser.parameters[1] == 0;
            parser.dcsUdkValueOffset = parser.decodedSize;
            parser.dcsUdkCode = 0;
            parser.dcsUdkKey = InputKey::Unknown;
            parser.dcsUdkHasCode = false;
            parser.dcsUdkHasHighNibble = false;
            parser.dcsUdkValid = true;
            parser.dcsUdkInValue = false;
            fgoto dcsUdkCode;
        } else if (parser.dcsIntermediateCount == 1 &&
                   parser.dcsIntermediates[0] == '$' && fc == 'p' &&
                   parser.parameterCount == 1 && parser.present[0] &&
                   parser.parameters[0] == 2) {
            parser.parameters[0] = 0;
            parser.present[0] = false;
            parser.parameterCount = 1;
            parser.dcsColorValid = true;
            fgoto dcsColor;
        } else if (parser.dcsIntermediateCount == 1 &&
                   parser.dcsIntermediates[0] == '$' && fc == 't' &&
                   parser.parameterCount == 1 && parser.present[0] &&
                   parser.parameters[0] == 2) {
            parser.parameters[0] = 0;
            parser.present[0] = false;
            parser.parameterCount = 1;
            parser.dcsTabValid = true;
            iface.dcs_DECRSTS_TABS_BEGIN();
            fgoto dcsTabs;
        } else if (parser.dcsIntermediateCount == 1 &&
                   parser.dcsIntermediates[0] == '$' && fc == 't' &&
                   parser.parameterCount == 1 && parser.present[0] &&
                   parser.parameters[0] == 1) {
            parser.dcsCursorNumberCount = 0;
            parser.dcsCursorByteCount = 0;
            parser.dcsCursorCharsetCount = 0;
            fgoto dcsCursor;
        } else if (parser.dcsIntermediateCount == 0 && fc == 'q') {
            ragelBeginSixel();
            fgoto sixelGround;
        } else if (parser.dcsIntermediateCount == 1 &&
                   parser.dcsIntermediates[0] == '!' && fc == 'u' &&
                   parser.parameterCount == 1 &&
                   (!parser.present[0] || parser.parameters[0] <= 1)) {
            parser.dcsUpssId = 0;
            parser.dcsUpssBytes = 0;
            parser.dcsUpss96 =
                parser.present[0] && parser.parameters[0] == 1;
            parser.dcsUpssValid = true;
            parser.dcsUpssComplete = false;
            fgoto dcsUpss;
        } else {
            fgoto dcsPayload;
        }
    }

    action dcsHeaderInvalid {
        if constexpr (traced) {
            parserTrace->stringCancel();
        }
        fgoto dcsIgnore;
    }

    action dcsHeaderTerminated {
        parser.stringUtf8Remaining = 0;
        parser.stringLimit = 0;
        if constexpr (traced) {
            parserTrace->stringCancel();
        }
        fnext main;
        fbreak;
    }

    action dcsIgnoreSt {
        parser.stringUtf8Remaining = 0;
        parser.stringLimit = 0;
        fnext main;
        fbreak;
    }

    action dcsPayloadData {
        if (fc >= 0x20 && fc < 0x7f) {
            const size_t count = printableAsciiPrefix(p, pe - p);
            parser.stringUtf8Remaining = 0;
            ragelAppendStringSpan(p, count, parser.maxDcsBytes);
            p += count - 1;
        } else if (fc >= 0x80) {
            consumeStringUtf8Byte(fc);
        } else {
            ragelAppendString(fc, parser.maxDcsBytes);
        }
    }

    action dcsIgnoreData {
        if (fc >= 0x20 && fc < 0x7f) {
            const size_t count = printableAsciiPrefix(p, pe - p);
            p += count - 1;
        }
    }

    action dcsSt {
        if (!consumeStringUtf8Byte(fc)) {
            ragelFinishDcs();
            fnext main;
            fbreak;
        }
    }

    action dcsHeaderEscape {
        fgoto dcsHeaderEscape;
    }

    action dcsPayloadEscape {
        fgoto dcsPayloadEscape;
    }

    action dcsEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxDcsBytes);
    }

    action dcsEscapedData {
        ragelAppendEscapedString(fc, parser.maxDcsBytes);
        fgoto dcsPayload;
    }

    action dcsColorDigit {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.present[parser.parameterCount - 1] = true;
        if (parser.parameters[parser.parameterCount - 1] >
            (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.parameters[parser.parameterCount - 1] = UINT32_MAX;
        } else {
            parser.parameters[parser.parameterCount - 1] =
                parser.parameters[parser.parameterCount - 1] * 10 + fc - '0';
        }
    }

    action dcsColorSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.parameterCount == 5) {
            parser.dcsColorValid = false;
        } else {
            parser.parameters[parser.parameterCount] = 0;
            parser.present[parser.parameterCount] = false;
            ++parser.parameterCount;
        }
    }

    action dcsColorDefinition {
        ragelAppendString(fc, parser.maxDcsBytes);
        finishDcsColor();
    }

    action dcsColorInvalid {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsColorValid = false;
    }

    action dcsColorSt {
        finishDcsColor();
        ragelFinishDcs();
        fnext main;
        fbreak;
    }

    action dcsColorEscape {
        fgoto dcsColorEscape;
    }

    action dcsColorEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxDcsBytes);
        parser.dcsColorValid = false;
    }

    action dcsColorEscapedData {
        ragelAppendEscapedString(fc, parser.maxDcsBytes);
        parser.dcsColorValid = false;
        fgoto dcsColor;
    }

    action dcsTabDigit {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.present[0] = true;
        if (parser.parameters[0] > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.parameters[0] = UINT32_MAX;
        } else {
            parser.parameters[0] =
                parser.parameters[0] * 10 + fc - '0';
        }
    }

    action dcsTabSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
        finishDcsTab();
    }

    action dcsTabInvalid {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsTabValid = false;
    }

    action dcsTabSt {
        finishDcsTab();
        ragelFinishDcs();
        fnext main;
        fbreak;
    }

    action dcsTabEscape {
        fgoto dcsTabEscape;
    }

    action dcsTabEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxDcsBytes);
        parser.dcsTabValid = false;
    }

    action dcsTabEscapedData {
        ragelAppendEscapedString(fc, parser.maxDcsBytes);
        parser.dcsTabValid = false;
        fgoto dcsTabs;
    }

    action sixelIgnoredByte {
        ragelAppendString(fc, parser.maxDcsBytes);
    }

    action sixelPaintOne {
        ragelAppendString(fc, parser.maxDcsBytes);
        paintSixel(fc, 1);
    }

    action sixelPaintRepeated {
        ragelAppendString(fc, parser.maxDcsBytes);
        paintSixel(fc, parser.parameters[0] > 0 ? parser.parameters[0] : 1);
        parser.parameters[0] = 0;
        parser.present[0] = false;
        parser.parameterCount = 1;
        fgoto sixelGround;
    }

    action sixelParameterDigit {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.present[parser.parameterCount - 1] = true;
        if (parser.parameters[parser.parameterCount - 1] > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.parameters[parser.parameterCount - 1] = UINT32_MAX;
        } else {
            parser.parameters[parser.parameterCount - 1] = parser.parameters[parser.parameterCount - 1] * 10 + fc - '0';
        }
    }

    action sixelParameterSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.parameterCount < 6) {
            parser.parameters[parser.parameterCount] = 0;
            parser.present[parser.parameterCount] = false;
            ++parser.parameterCount;
        }
    }

    action sixelRepeatIntro {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.parameters[0] = 0;
        parser.present[0] = false;
        parser.parameterCount = 1;
        fgoto sixelRepeat;
    }

    action sixelColorIntro {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.parameters[0] = 0;
        parser.present[0] = false;
        parser.parameterCount = 1;
        fgoto sixelColor;
    }

    action sixelRasterIntro {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.parameters[0] = 0;
        parser.present[0] = false;
        parser.parameterCount = 1;
        fgoto sixelRaster;
    }

    action sixelCr {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.sixelX = 0;
    }

    action sixelLf {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.sixelX = 0;
        // Saturate one band past the limit; painting is clipped anyway.
        if (parser.sixelBand * 6 < parser.maxSixelHeight) {
            ++parser.sixelBand;
        }
    }

    action sixelParamsAbandon {
        parser.parameters[0] = 0;
        parser.present[0] = false;
        parser.parameterCount = 1;
        fhold;
        fgoto sixelGround;
    }

    action sixelColorDone {
        finishSixelColor();
        fhold;
        fgoto sixelGround;
    }

    action sixelRasterDone {
        finishSixelRaster();
        fhold;
        fgoto sixelGround;
    }

    action sixelSt {
        finishSixel();
        ragelFinishDcs();
        fnext main;
        fbreak;
    }

    action sixelColorSt {
        finishSixelColor();
        finishSixel();
        ragelFinishDcs();
        fnext main;
        fbreak;
    }

    action sixelRasterSt {
        finishSixelRaster();
        finishSixel();
        ragelFinishDcs();
        fnext main;
        fbreak;
    }

    action sixelEscapeBegin {
        fgoto sixelEscape;
    }

    action sixelColorEscapeBegin {
        finishSixelColor();
        fgoto sixelEscape;
    }

    action sixelRasterEscapeBegin {
        finishSixelRaster();
        fgoto sixelEscape;
    }

    action dcsCursorNumberStart {
        if (parser.dcsCursorNumberCount < 5) {
            parser.dcsCursorNumbers[parser.dcsCursorNumberCount] = 0;
        }
    }

    action dcsCursorDigit {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.dcsCursorNumberCount < 5) {
            u32& value =
                parser.dcsCursorNumbers[parser.dcsCursorNumberCount];
            if (value > (UINT32_MAX - (u32)(fc - '0')) / 10) {
                value = UINT32_MAX;
            } else {
                value = value * 10 + fc - '0';
            }
        }
    }

    action dcsCursorNumberDone {
        ++parser.dcsCursorNumberCount;
    }

    action dcsCursorByte {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.dcsCursorByteCount < 4) {
            parser.dcsCursorBytes[parser.dcsCursorByteCount++] = fc - '@';
        }
    }

    action dcsCursorSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
    }

    action dcsCursorCharsetStart {
        if (parser.dcsCursorCharsetCount < 4) {
            parser.dcsCursorCharsetIds[parser.dcsCursorCharsetCount] = 0;
        }
    }

    action dcsCursorCharsetByte {
        ragelAppendString(fc, parser.maxDcsBytes);
        if (parser.dcsCursorCharsetCount < 4) {
            u16& id =
                parser.dcsCursorCharsetIds[parser.dcsCursorCharsetCount];
            id = (u16)((id << 8) | fc);
        }
    }

    action dcsCursorCharsetDone {
        if (parser.dcsCursorCharsetCount < 4) {
            const u8 index = parser.dcsCursorCharsetCount;
            parser.dcsCursorCharsets[index] = decodeCharset(
                parser.dcsCursorCharsetIds[index],
                parser.dcsCursorBytes[3] & (1u << index)
            );
        }
        ++parser.dcsCursorCharsetCount;
    }

    action dcsCursorSt {
        const bool valid =
            parser.dcsCursorNumberCount == 5 &&
            parser.dcsCursorByteCount == 4 &&
            parser.dcsCursorCharsetCount == 4 &&
            parser.dcsCursorNumbers[0] != 0 &&
            parser.dcsCursorNumbers[1] != 0 &&
            parser.dcsCursorNumbers[2] == 1 &&
            parser.dcsCursorNumbers[3] < 4 &&
            parser.dcsCursorNumbers[4] < 4;
        if (valid) {
            iface.dcs_DECRSTS_CURSOR(
                parser.dcsCursorNumbers[0],
                parser.dcsCursorNumbers[1],
                parser.dcsCursorBytes[0],
                parser.dcsCursorBytes[1],
                parser.dcsCursorBytes[2],
                parser.dcsCursorNumbers[3],
                parser.dcsCursorNumbers[4],
                parser.dcsCursorBytes[3],
                parser.dcsCursorCharsets,
                parser.dcsCursorCharsetIds
            );
        }
        ragelFinishDcs();
        fnext main;
        fbreak;
    }

    action dcsCursorDoneEscape {
        fgoto dcsCursorDoneEscape;
    }

    action dcsCursorInvalid {
        fhold;
        fgoto dcsIgnore;
    }

    action dcsUpssStart {
        if (parser.dcsUpssComplete) {
            parser.dcsUpssValid = false;
        }
        parser.dcsUpssId = 0;
        parser.dcsUpssBytes = 0;
    }

    action dcsUpssByte {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsUpssId = (u16)((parser.dcsUpssId << 8) | fc);
        ++parser.dcsUpssBytes;
    }

    action dcsUpssComplete {
        parser.dcsUpssComplete = true;
    }

    action dcsUpssSt {
        if (parser.dcsUpssValid && parser.dcsUpssComplete &&
            parser.dcsUpssBytes >= 1 && parser.dcsUpssBytes <= 2) {
            iface.dcs_DECAUPSS(
                decodeCharset(parser.dcsUpssId, parser.dcsUpss96),
                parser.dcsUpssId,
                parser.dcsUpss96
            );
        }
        ragelFinishDcs();
        fnext main;
        fbreak;
    }

    action dcsUpssEscape {
        fgoto dcsUpssEscape;
    }

    action dcsUpssInvalid {
        fhold;
        fgoto dcsIgnore;
    }

    action dcsDecrqssInvalidStart {
        fhold;
        fgoto dcsDecrqssInvalid;
    }

    action dcsDecrqssInvalid {
        consumeStringUtf8Byte(fc);
        ragelAppendString(fc, parser.maxDcsBytes);
        fgoto dcsDecrqssInvalid;
    }

    action dcsDecrqssEscape {
        fgoto dcsDecrqssEscape;
    }

    action dcsDecrqssEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxDcsBytes);
        fgoto dcsDecrqssEscape;
    }

    action dcsDecrqssEscapedData {
        ragelAppendEscapedString(fc, parser.maxDcsBytes);
        fgoto dcsDecrqssInvalid;
    }

    action dcsDecrqssUnknownSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxDcsBytes);
            fgoto dcsDecrqssInvalid;
        } else {
            ragelFinishDcs();
            if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
                iface.dcs_DECRQSS_UNKNOWN();
            }
            fnext main;
            fbreak;
        }
    }

    action dcsDecrqssDecsclSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_DECSCL();
        }
        fnext main;
        fbreak;
    }

    action dcsDecrqssSgrSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_SGR();
        }
        fnext main;
        fbreak;
    }

    action dcsDecrqssDecstbmSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_DECSTBM();
        }
        fnext main;
        fbreak;
    }

    action dcsDecrqssDecslrmSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_DECSLRM();
        }
        fnext main;
        fbreak;
    }

    action dcsDecrqssDecslppSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_DECSLPP();
        }
        fnext main;
        fbreak;
    }

    action dcsDecrqssDecscusrSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_DECSCUSR();
        }
        fnext main;
        fbreak;
    }

    action dcsDecrqssDecscaSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_DECSCA();
        }
        fnext main;
        fbreak;
    }

    action dcsDecrqssDecsaceSt {
        ragelFinishDcs();
        if (!parser.overflow && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT400) {
            iface.dcs_DECRQSS_DECSACE();
        }
        fnext main;
        fbreak;
    }

    action dcsXtHex {
        ragelAppendString(fc, parser.maxDcsBytes);
        const u8 nibble = fc <= '9' ? fc - '0' : (fc | 0x20) - 'a' + 10;
        if (!parser.dcsCapabilityHasHighNibble) {
            parser.dcsCapabilityHighNibble = nibble;
            parser.dcsCapabilityHasHighNibble = true;
        } else {
            const u8 decoded = (parser.dcsCapabilityHighNibble << 4) | nibble;
            static constexpr u8 terminalName[] = {'T', 'N'};
            static constexpr u8 colorCount[] = {'C', 'o'};
            static constexpr u8 colors[] = {'c', 'o', 'l', 'o', 'r', 's'};
            static constexpr u8 rgb[] = {'R', 'G', 'B'};
            const size_t index = parser.dcsCapabilityDecodedLength++;
            if (index >= sizeof(terminalName) || terminalName[index] != decoded) {
                parser.dcsCapabilityCandidates &= ~0x01;
            }
            if (index >= sizeof(colorCount) || colorCount[index] != decoded) {
                parser.dcsCapabilityCandidates &= ~0x02;
            }
            if (index >= sizeof(colors) || colors[index] != decoded) {
                parser.dcsCapabilityCandidates &= ~0x04;
            }
            if (index >= sizeof(rgb) || rgb[index] != decoded) {
                parser.dcsCapabilityCandidates &= ~0x08;
            }
            parser.dcsCapabilityHasHighNibble = false;
        }
    }

    action dcsXtInvalid {
        consumeStringUtf8Byte(fc);
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsCapabilityValid = false;
    }

    action dcsXtField {
        if (parser.dcsCapabilityComplete && !parser.overflow) {
            u8 value = 0;
            if (parser.dcsCapabilityValid && !parser.dcsCapabilityHasHighNibble &&
                (parser.dcsCapabilityCandidates & 0x01) &&
                parser.dcsCapabilityDecodedLength == 2) {
                value = 1;
            } else if (parser.dcsCapabilityValid && !parser.dcsCapabilityHasHighNibble &&
                       (((parser.dcsCapabilityCandidates & 0x02) &&
                         parser.dcsCapabilityDecodedLength == 2) ||
                        ((parser.dcsCapabilityCandidates & 0x04) &&
                         parser.dcsCapabilityDecodedLength == 6))) {
                value = 2;
            } else if (parser.dcsCapabilityValid && !parser.dcsCapabilityHasHighNibble &&
                       (parser.dcsCapabilityCandidates & 0x08) &&
                       parser.dcsCapabilityDecodedLength == 3) {
                value = 3;
            }
            if (parser.dcsTermcapQueryCount != parser.maxTermcapQueries) {
                parser.dcsTermcapQueries[parser.dcsTermcapQueryCount++] = {
                    parser.dcsCapabilityOffset,
                    ragelStringSize() - parser.dcsCapabilityOffset,
                    value,
                };
            } else {
                parser.overflow = true;
            }
        }
    }

    action dcsXtSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsCapabilityOffset = ragelStringSize();
        parser.dcsCapabilityDecodedLength = 0;
        parser.dcsCapabilityCandidates = 0x0f;
        parser.dcsCapabilityHasHighNibble = false;
        parser.dcsCapabilityValid = true;
        parser.dcsCapabilityComplete = false;
    }

    action dcsXtSt {
        parser.dcsCapabilityComplete = false;
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxDcsBytes);
            parser.dcsCapabilityValid = false;
        } else {
            ragelFinishDcs();
            parser.dcsCapabilityComplete = true;
        }
    }

    action dcsXtDone {
        if (parser.dcsCapabilityComplete) {
            if (!parser.overflow &&
                iface.parserCompatibilityLevel() >= CompatibilityLevel::VT200) {
                const auto* data = ragelStringData();
                for (size_t index = 0; index < parser.dcsTermcapQueryCount; ++index) {
                    const ParserTermcapQuery& query = parser.dcsTermcapQueries[index];
                    const StringView encoded(data + query.offset, query.length);
                    if (query.value == 1) {
                        iface.dcs_XTGETTCAP(encoded, StringView(u8"xterm-256color"));
                    } else if (query.value == 2) {
                        iface.dcs_XTGETTCAP(encoded, StringView(u8"256"));
                    } else if (query.value == 3) {
                        iface.dcs_XTGETTCAP(encoded, StringView(u8"8"));
                    } else {
                        iface.dcs_XTGETTCAP(encoded, {});
                    }
                }
            }
            fnext main;
            fbreak;
        }
    }

    action dcsXtEscape {
        fgoto dcsXtEscape;
    }

    action dcsXtEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxDcsBytes);
        parser.dcsCapabilityValid = false;
    }

    action dcsXtEscapedData {
        ragelAppendEscapedString(fc, parser.maxDcsBytes);
        parser.dcsCapabilityValid = false;
        fgoto dcsXtgettcap;
    }

    action dcsUdkDigit {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsUdkHasCode = true;
        if (parser.dcsUdkCode > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.dcsUdkValid = false;
        } else {
            parser.dcsUdkCode = parser.dcsUdkCode * 10 + fc - '0';
        }
    }

    action dcsUdkSlash {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsUdkInValue = true;
        parser.dcsUdkValid = parser.dcsUdkValid && parser.dcsUdkHasCode;
        if (parser.dcsUdkCode >= 17 && parser.dcsUdkCode <= 21) {
            parser.dcsUdkKey = (InputKey)(
                (int)(InputKey::F6) + parser.dcsUdkCode - 17
            );
        } else if (parser.dcsUdkCode >= 23 && parser.dcsUdkCode <= 26) {
            parser.dcsUdkKey = (InputKey)(
                (int)(InputKey::F11) + parser.dcsUdkCode - 23
            );
        } else if (parser.dcsUdkCode >= 28 && parser.dcsUdkCode <= 29) {
            parser.dcsUdkKey = (InputKey)(
                (int)(InputKey::F15) + parser.dcsUdkCode - 28
            );
        } else if (parser.dcsUdkCode >= 31 && parser.dcsUdkCode <= 34) {
            parser.dcsUdkKey = (InputKey)(
                (int)(InputKey::F17) + parser.dcsUdkCode - 31
            );
        } else {
            parser.dcsUdkKey = InputKey::Unknown;
            parser.dcsUdkValid = false;
        }
        parser.dcsUdkValueOffset = parser.decodedSize;
        parser.dcsUdkHasHighNibble = false;
        fgoto dcsUdkValue;
    }

    action dcsUdkHex {
        ragelAppendString(fc, parser.maxDcsBytes);
        const u8 nibble = fc <= '9' ? fc - '0' : (fc | 0x20) - 'a' + 10;
        if (!parser.dcsUdkHasHighNibble) {
            parser.dcsUdkHighNibble = nibble;
            parser.dcsUdkHasHighNibble = true;
        } else {
            if (!parser.overflow &&
                parser.decodedSize - parser.dcsUdkValueOffset < 255) {
                const u8 decoded = (parser.dcsUdkHighNibble << 4) | nibble;
                appendDecoded(decoded);
            } else {
                parser.dcsUdkValid = false;
            }
            parser.dcsUdkHasHighNibble = false;
        }
    }

    action dcsUdkCodeSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsUdkCode = 0;
        parser.dcsUdkKey = InputKey::Unknown;
        parser.dcsUdkHasCode = false;
        parser.dcsUdkHasHighNibble = false;
        parser.dcsUdkValid = true;
        parser.dcsUdkInValue = false;
    }

    action dcsUdkValueSeparator {
        if (!parser.overflow && parser.dcsUdkValid && !parser.dcsUdkHasHighNibble) {
            if (parser.dcsUdkDefinitionCount != parser.maxUdkDefinitions) {
                parser.dcsUdkDefinitions[parser.dcsUdkDefinitionCount++] = {
                    parser.dcsUdkValueOffset,
                    parser.decodedSize - parser.dcsUdkValueOffset,
                    parser.dcsUdkKey,
                };
            } else {
                parser.overflow = true;
            }
        }
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsUdkCode = 0;
        parser.dcsUdkKey = InputKey::Unknown;
        parser.dcsUdkHasCode = false;
        parser.dcsUdkHasHighNibble = false;
        parser.dcsUdkValid = true;
        parser.dcsUdkInValue = false;
        fgoto dcsUdkCode;
    }

    action dcsUdkInvalidSeparator {
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsUdkCode = 0;
        parser.dcsUdkKey = InputKey::Unknown;
        parser.dcsUdkHasCode = false;
        parser.dcsUdkHasHighNibble = false;
        parser.dcsUdkValid = true;
        parser.dcsUdkInValue = false;
        fgoto dcsUdkCode;
    }

    action dcsUdkInvalid {
        consumeStringUtf8Byte(fc);
        ragelAppendString(fc, parser.maxDcsBytes);
        parser.dcsUdkValid = false;
        fgoto dcsUdkInvalid;
    }

    action dcsUdkSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxDcsBytes);
            parser.dcsUdkValid = false;
            fgoto dcsUdkInvalid;
        } else {
            if (!parser.overflow && parser.dcsUdkInValue && parser.dcsUdkValid &&
                !parser.dcsUdkHasHighNibble) {
                if (parser.dcsUdkDefinitionCount != parser.maxUdkDefinitions) {
                    parser.dcsUdkDefinitions[parser.dcsUdkDefinitionCount++] = {
                        parser.dcsUdkValueOffset,
                        parser.decodedSize - parser.dcsUdkValueOffset,
                        parser.dcsUdkKey,
                    };
                } else {
                    parser.overflow = true;
                }
            }
            ragelFinishDcs();
            if (!parser.overflow && parser.dcsUdkHeaderValid &&
                iface.parserCompatibilityLevel() >= CompatibilityLevel::VT200) {
                iface.dcs_DECUDK(
                    parser.dcsUdkClearDefinitions,
                    parser.dcsUdkLockDefinitions,
                    parser.dcsUdkDefinitions,
                    parser.dcsUdkDefinitionCount,
                    decodedString()
                );
            }
            fnext main;
            fbreak;
        }
    }

    action dcsUdkEscape {
        fgoto dcsUdkEscape;
    }

    action dcsUdkEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxDcsBytes);
        parser.dcsUdkValid = false;
    }

    action dcsUdkEscapedData {
        ragelAppendEscapedString(fc, parser.maxDcsBytes);
        parser.dcsUdkValid = false;
        fgoto dcsUdkInvalid;
    }

    action oscCommandDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscCommandValid = true;
        if (parser.oscCommand > (2147483647u - (u32)(fc - '0')) / 10) {
            parser.oscCommandValid = false;
        } else {
            parser.oscCommand = parser.oscCommand * 10 + fc - '0';
        }
    }

    action oscCommandSeparator {
        ragelAppendString(fc, parser.maxOscBytes);
        if (!parser.oscCommandValid) {
            fgoto oscInvalid;
        }
        parser.oscPayloadOffset = ragelStringSize();
        if (parser.oscCommand == 0 || parser.oscCommand == 1 || parser.oscCommand == 2) {
            parser.oscTitleHex = iface.parserHexTitleInput();
            parser.oscTitleHasHighNibble = false;
            parser.oscTitleValid = true;
            resetDecoded();
            if (parser.oscCommand == 0) {
                fgoto oscTitle0;
            }
            if (parser.oscCommand == 1) {
                fgoto oscTitle1;
            }
            fgoto oscTitle2;
        } else if (parser.oscCommand == 7) {
            resetDecoded();
            parser.oscCwdValid = false;
            fgoto oscCwdEntry;
        } else if (parser.oscCommand >= 10 && parser.oscCommand <= 19) {
            resetOscColor();
            fgoto oscDynamicColor;
        } else if (parser.oscCommand == 4 || parser.oscCommand == 5) {
            parser.oscFieldNumber = 0;
            parser.oscFieldNumeric = true;
            parser.oscFieldPresent = false;
            fgoto oscIndexedColorIndex;
        } else if (parser.oscCommand == 6 || parser.oscCommand == 106) {
            parser.oscFieldNumber = 0;
            parser.oscFieldNumeric = true;
            parser.oscFieldPresent = false;
            parser.oscFieldFirst = 0;
            parser.oscFieldFirstValid = false;
            parser.oscFieldHaveFirst = false;
            fgoto oscNumericFields;
        } else if (parser.oscCommand == 104 || parser.oscCommand == 105) {
            parser.oscFieldNumber = 0;
            parser.oscFieldNumeric = true;
            parser.oscFieldPresent = false;
            fgoto oscNumericFields;
        } else if (parser.oscCommand == 8) {
            parser.oscHyperlinkIdOffset = 0;
            parser.oscHyperlinkIdLength = 0;
            parser.oscHyperlinkUriOffset = 0;
            parser.oscHyperlinkHasId = false;
            fgoto oscHyperlinkParamStart;
        } else if (parser.oscCommand == 9) {
            parser.oscProgressState = 0;
            parser.oscProgressPercent = 0;
            parser.oscProgressStatePresent = false;
            parser.oscProgressPercentPresent = false;
            parser.oscProgressValid = true;
            fgoto oscProgressEntry;
        } else if (parser.oscCommand == 52) {
            parser.osc52ReplySelector = 0;
            parser.osc52Primary = false;
            parser.osc52Clipboard = false;
            parser.osc52SelectorSeen = false;
            parser.osc52PayloadSeen = false;
            parser.osc52Query = false;
            fgoto osc52Selectors;
        } else if (parser.oscCommand == 99) {
            parser.oscNotificationFieldOffset = 0;
            parser.oscNotificationIdOffset = 0;
            parser.oscNotificationIdLength = 0;
            parser.oscNotificationPayloadOffset = 0;
            parser.oscNotificationPayloadBytes = 0;
            parser.oscNotificationKey = 0;
            parser.oscNotificationValid = true;
            parser.oscNotificationEncoded = false;
            parser.oscNotificationFinal = true;
            parser.oscNotificationQuery = false;
            parser.oscNotificationClose = false;
            parser.oscNotificationBody = false;
            fgoto oscNotificationField;
        } else if (parser.oscCommand == 133) {
            fgoto oscShellEntry;
        }
        fgoto oscPayload;
    }

    action oscCommandInvalid {
        consumeStringUtf8Byte(fc);
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscInvalid;
    }

    action oscCommandSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscTerminated = false;
            fgoto oscInvalid;
        } else {
            parser.oscPayloadOffset = ragelStringSize();
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action oscCommandBell {
        parser.oscPayloadOffset = ragelStringSize();
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action oscData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
    }

    action oscBulkData {
        if (fc >= 0x20 && fc < 0x7f) {
            const size_t count = printableAsciiPrefix(p, pe - p);
            parser.stringUtf8Remaining = 0;
            ragelAppendStringSpan(p, count, parser.maxOscBytes);
            p += count - 1;
        } else if (fc >= 0xa0) {
            const size_t count = highStringPrefix(p, pe - p);
            ragelAppendStringSpan(p, count, parser.maxOscBytes);
            p += count - 1;
        } else {
            consumeStringUtf8Byte(fc);
            if (!executeC0(fc)) {
                ragelAppendString(fc, parser.maxOscBytes);
            }
        }
    }

    action oscRawData {
        if (fc >= 0x20 && fc < 0x7f) {
            const size_t count = printableAsciiPrefix(p, pe - p);
            parser.stringUtf8Remaining = 0;
            ragelAppendStringSpan(p, count, parser.maxOscBytes);
            p += count - 1;
        } else if (fc >= 0xa0) {
            const size_t count = highStringPrefix(p, pe - p);
            ragelAppendStringSpan(p, count, parser.maxOscBytes);
            p += count - 1;
        } else {
            consumeStringUtf8Byte(fc);
            if (!executeC0(fc)) {
                ragelAppendString(fc, parser.maxOscBytes);
            }
        }
    }

    action oscSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscTerminated = false;
        } else {
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action oscBell {
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action oscDispatch {
        if (parser.oscTerminated && parser.oscCommandValid && !parser.overflow) {
            const StringView payload = ragelOscPayload();
            if (parser.oscCommand == 0) {
                iface.osc_TITLE_0(payload);
            } else if (parser.oscCommand == 1) {
                iface.osc_TITLE_1(payload);
            } else if (parser.oscCommand == 2) {
                iface.osc_TITLE_2(payload);
            } else if (parser.oscCommand == 8) {
                (void)payload;
            } else if (parser.oscCommand == 9) {
                iface.osc_NOTIFY(payload);
            } else if (parser.oscCommand == 52) {
                iface.osc_RAW(52, payload);
            } else if (parser.oscCommand == 104 && payload.empty()) {
                iface.osc_RESET_PALETTE();
            } else if (parser.oscCommand == 105 && payload.empty()) {
                iface.osc_RESET_SPECIAL_COLOR();
            } else if (parser.oscCommand == 110) {
                iface.osc_RESET_DEFAULT_FOREGROUND();
            } else if (parser.oscCommand == 111) {
                iface.osc_RESET_DEFAULT_BACKGROUND();
            } else if (parser.oscCommand == 112) {
                iface.osc_RESET_CURSOR_COLOR();
            } else if (parser.oscCommand == 117) {
                iface.osc_RESET_SELECTION_BACKGROUND();
            } else if (parser.oscCommand == 119) {
                iface.osc_RESET_SELECTION_FOREGROUND();
            } else if (parser.oscCommand == 133) {
                iface.osc_SHELL_UNKNOWN(payload);
            } else if (parser.oscCommand == 5522) {
                dispatchKittyClipboard(payload);
            } else {
                iface.osc_UNKNOWN(parser.oscCommand, payload);
            }
        }
    }

    action oscDone {
        if (parser.oscTerminated) {
            fnext main;
            fbreak;
        }
    }

    action oscEscape {
        fgoto oscEscape;
    }

    action oscEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxOscBytes);
    }

    action oscEscapedData {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscPayload;
    }

    action oscInvalidSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        } else {
            parser.stringUtf8Remaining = 0;
            parser.stringLimit = 0;
            if constexpr (traced) {
                parserTrace->stringEnd();
            }
            fnext main;
            fbreak;
        }
    }

    action oscInvalidBell {
        parser.stringUtf8Remaining = 0;
        parser.stringLimit = 0;
        if constexpr (traced) {
            parserTrace->stringEnd();
        }
        fnext main;
        fbreak;
    }

    action oscInvalidEscapedData {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscInvalid;
    }

    action oscCwdRaw {
        ragelAppendString(fc, parser.maxOscBytes);
    }

    action oscCwdInvalidData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
        parser.oscCwdValid = false;
        fgoto oscCwdInvalid;
    }

    action oscCwdPrefixError {
        fhold;
        fgoto oscCwdInvalid;
    }

    action oscCwdPathStart {
        parser.oscCwdPathOffset = ragelStringSize();
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscCwdValid = true;
        fgoto oscCwdPath;
    }

    action oscCwdAuthorityData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
    }

    action oscCwdPathData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
    }

    action oscCwdPercentStart {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscCwdValid = false;
        fgoto oscCwdPercentHigh;
    }

    action oscCwdPercentHigh {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscCwdPercentLow;
    }

    action oscCwdPercentLow {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscCwdValid = true;
        fgoto oscCwdPath;
    }

    action oscCwdSt {
        if (ragelStringContinuation(fc)) {
            parser.oscTerminated = false;
        } else {
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action oscCwdBell {
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action oscCwdDispatch {
        if (parser.oscTerminated && !parser.overflow) {
            iface.osc_RAW(7, ragelOscPayload());
            if (parser.oscCwdValid) {
                decodeCwd();
            }
            iface.osc_CWD(decodedString(), parser.oscCwdValid);
        }
    }

    action oscCwdInvalidEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        parser.oscCwdValid = false;
        fgoto oscCwdInvalid;
    }

    action oscCwdAuthorityEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscCwdAuthority;
    }

    action oscCwdPathEscapedEscape {
    }

    action oscCwdPathEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscCwdPath;
    }

    action oscShellA {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellAComplete;
    }

    action oscShellB {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellBComplete;
    }

    action oscShellC {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellCComplete;
    }

    action oscShellD {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellDComplete;
    }

    action oscShellI {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellIComplete;
    }

    action oscShellL {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellLComplete;
    }

    action oscShellN {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellNComplete;
    }

    action oscShellP {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscShellPComplete;
    }

    action oscShellInvalid {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
        fgoto oscShellUnknown;
    }

    action oscShellASt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_A(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellBSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_B(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellCSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_C(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellDSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_D(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellISt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_I(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellLSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_L(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellNSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_N(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellPSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscShellUnknown;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_P(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellUnknownSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_SHELL_UNKNOWN(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscShellEscapedUnknown {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellUnknown;
    }

    action oscShellEscapedA {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellATail;
    }

    action oscShellEscapedB {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellBTail;
    }

    action oscShellEscapedC {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellCTail;
    }

    action oscShellEscapedD {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellDTail;
    }

    action oscShellEscapedI {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellITail;
    }

    action oscShellEscapedL {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellLTail;
    }

    action oscShellEscapedN {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellNTail;
    }

    action oscShellEscapedP {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscShellPTail;
    }

    action oscTitleData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            if (parser.oscTitleHex) {
                if (!((fc >= '0' && fc <= '9') ||
                      ((fc | 0x20) >= 'a' && (fc | 0x20) <= 'f'))) {
                    parser.oscTitleValid = false;
                }
                if (parser.oscTitleValid) {
                    parser.oscTitleHasHighNibble =
                        !parser.oscTitleHasHighNibble;
                }
            }
        }
    }

    action oscTitleEscapedEscape {
        ragelAppendSynthetic('\x1b', parser.maxOscBytes);
        if (parser.oscTitleHex) {
            parser.oscTitleValid = false;
        }
    }

    action oscTitleEscaped0 {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        if (parser.oscTitleHex) {
            parser.oscTitleValid = false;
        }
        fgoto oscTitle0;
    }

    action oscTitleEscaped1 {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        if (parser.oscTitleHex) {
            parser.oscTitleValid = false;
        }
        fgoto oscTitle1;
    }

    action oscTitleEscaped2 {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        if (parser.oscTitleHex) {
            parser.oscTitleValid = false;
        }
        fgoto oscTitle2;
    }

    action oscTitle0St {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            if (parser.oscTitleHex) {
                parser.oscTitleValid = false;
            }
        } else {
            ragelFinishOsc();
            if (!parser.overflow && parser.oscTitleValid && (!parser.oscTitleHex || !parser.oscTitleHasHighNibble)) {
                if (parser.oscTitleHex) {
                    decodeTitle();
                }
                iface.osc_TITLE_0(parser.oscTitleHex ? decodedString() : ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscTitle1St {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            if (parser.oscTitleHex) {
                parser.oscTitleValid = false;
            }
        } else {
            ragelFinishOsc();
            if (!parser.overflow && parser.oscTitleValid && (!parser.oscTitleHex || !parser.oscTitleHasHighNibble)) {
                if (parser.oscTitleHex) {
                    decodeTitle();
                }
                iface.osc_TITLE_1(parser.oscTitleHex ? decodedString() : ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscTitle2St {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            if (parser.oscTitleHex) {
                parser.oscTitleValid = false;
            }
        } else {
            ragelFinishOsc();
            if (!parser.overflow && parser.oscTitleValid && (!parser.oscTitleHex || !parser.oscTitleHasHighNibble)) {
                if (parser.oscTitleHex) {
                    decodeTitle();
                }
                iface.osc_TITLE_2(parser.oscTitleHex ? decodedString() : ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscHyperlinkParamData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
        fgoto oscHyperlinkParamSkip;
    }

    action oscHyperlinkI {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscHyperlinkParamI;
    }

    action oscHyperlinkD {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscHyperlinkParamId;
    }

    action oscHyperlinkEqual {
        ragelAppendString(fc, parser.maxOscBytes);
        if (!parser.oscHyperlinkHasId) {
            parser.oscHyperlinkIdOffset = ragelStringSize();
        }
        fgoto oscHyperlinkIdValue;
    }

    action oscHyperlinkColon {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscHyperlinkParamStart;
    }

    action oscHyperlinkIdColon {
        if (!parser.oscHyperlinkHasId) {
            parser.oscHyperlinkIdLength = ragelStringSize() - parser.oscHyperlinkIdOffset;
            parser.oscHyperlinkHasId = true;
        }
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscHyperlinkParamStart;
    }

    action oscHyperlinkUri {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscHyperlinkUriOffset = ragelStringSize();
        fgoto oscHyperlinkUri;
    }

    action oscHyperlinkIdUri {
        if (!parser.oscHyperlinkHasId) {
            parser.oscHyperlinkIdLength = ragelStringSize() - parser.oscHyperlinkIdOffset;
            parser.oscHyperlinkHasId = true;
        }
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscHyperlinkUriOffset = ragelStringSize();
        fgoto oscHyperlinkUri;
    }

    action oscHyperlinkMalformedSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscHyperlinkParamSkip;
        } else {
            ragelFinishOsc();
            fnext main;
            fbreak;
        }
    }

    action oscHyperlinkSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                const auto* data = ragelStringData();
                iface.osc_HYPERLINK(
                    StringView(data + parser.oscHyperlinkIdOffset, parser.oscHyperlinkHasId ? parser.oscHyperlinkIdLength : 0),
                    parser.oscHyperlinkHasId,
                    StringView(data + parser.oscHyperlinkUriOffset, ragelStringSize() - parser.oscHyperlinkUriOffset)
                );
            }
            fnext main;
            fbreak;
        }
    }

    action oscHyperlinkParamEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscHyperlinkParamSkip;
    }

    action oscHyperlinkIdEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscHyperlinkIdValue;
    }

    action oscHyperlinkUriEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscHyperlinkUri;
    }

    action oscProgressFour {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscProgressFour;
    }

    action oscProgressBeginState {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscProgressState;
    }

    action oscProgressNotifyData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
        fgoto oscProgressNotify;
    }

    action oscProgressStateDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscProgressStatePresent = true;
        if (parser.oscProgressState > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.oscProgressValid = false;
        } else {
            parser.oscProgressState = parser.oscProgressState * 10 + fc - '0';
        }
    }

    action oscProgressBeginPercent {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscProgressValid = parser.oscProgressValid && parser.oscProgressStatePresent;
        fgoto oscProgressPercent;
    }

    action oscProgressPercentDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscProgressPercentPresent = true;
        if (parser.oscProgressPercent > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.oscProgressValid = false;
        } else {
            parser.oscProgressPercent = parser.oscProgressPercent * 10 + fc - '0';
        }
    }

    action oscProgressDiscardData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
        fgoto oscProgressDiscard;
    }

    action oscProgressNotifySt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_NOTIFY(ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action oscProgressSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscProgressValid = false;
            fgoto oscProgressDiscard;
        } else {
            ragelFinishOsc();
            if (!parser.overflow && parser.oscProgressValid && parser.oscProgressStatePresent &&
                parser.oscProgressState <= 4 &&
                (!parser.oscProgressPercentPresent || parser.oscProgressPercent <= 100)) {
                iface.osc_PROGRESS(
                    parser.oscProgressState,
                    parser.oscProgressPercent,
                    parser.oscProgressPercentPresent
                );
            }
            fnext main;
            fbreak;
        }
    }

    action oscProgressDiscardSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        } else {
            ragelFinishOsc();
            fnext main;
            fbreak;
        }
    }

    action oscProgressNotifyEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscProgressNotify;
    }

    action oscProgressDiscardEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscProgressDiscard;
    }

    action osc52Selector {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.osc52SelectorSeen = true;
        if (parser.osc52ReplySelector == 0 && (fc == 's' || fc == 'p' || fc == 'c')) {
            parser.osc52ReplySelector = fc;
        }
        if (fc == 'p' || (fc == 's' && !parser.osc52SelectClipboard)) {
            parser.osc52Primary = true;
        }
        if (fc == 'c' || (fc == 's' && parser.osc52SelectClipboard)) {
            parser.osc52Clipboard = true;
        }
    }

    action osc52BeginPayload {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.osc52PayloadOffset = ragelStringSize();
        if (!parser.osc52SelectorSeen) {
            parser.osc52Primary = true;
            parser.osc52Clipboard = true;
        }
        parser.osc52PayloadSeen = false;
        parser.osc52Query = false;
        fgoto osc52Payload;
    }

    action osc52Data {
        if (fc >= 0x20 && fc < 0x7f && parser.osc52PayloadSeen) {
            // Base64 payloads arrive by the megabyte: append whole
            // printable runs instead of one byte per machine step.
            const size_t count = printableAsciiPrefix(p, pe - p);
            parser.stringUtf8Remaining = 0;
            ragelAppendStringSpan(p, count, parser.maxOscBytes);
            parser.osc52Query = false;
            p += count - 1;
        } else {
            consumeStringUtf8Byte(fc);
            if (!executeC0(fc)) {
                ragelAppendString(fc, parser.maxOscBytes);
                if (!parser.osc52PayloadSeen) {
                    parser.osc52PayloadSeen = true;
                    parser.osc52Query = fc == '?';
                } else if (parser.osc52Query) {
                    parser.osc52Query = false;
                }
            }
        }
    }

    action osc52MalformedSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            fgoto oscInvalid;
        } else {
            ragelFinishOsc();
            if (!parser.overflow) {
                iface.osc_RAW(52, ragelOscPayload());
            }
            fnext main;
            fbreak;
        }
    }

    action osc52MalformedBell {
        ragelFinishOsc();
        if (!parser.overflow) {
            iface.osc_RAW(52, ragelOscPayload());
        }
        fnext main;
        fbreak;
    }

    action osc52St {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscTerminated = false;
            fgoto oscInvalid;
        } else {
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action osc52Bell {
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action osc52Dispatch {
        if (parser.oscTerminated && !parser.overflow) {
            const StringView raw = ragelOscPayload();
            iface.osc_RAW(52, raw);
            if (parser.osc52PayloadSeen && parser.osc52Query) {
                iface.osc_CLIPBOARD_QUERY(
                    parser.osc52Primary, parser.osc52Clipboard, parser.osc52ReplySelector,
                    !parser.osc52SelectorSeen
                );
            } else {
                const bool valid = decodeBase64(parser.osc52PayloadOffset);
                iface.osc_CLIPBOARD_WRITE(
                    decodedString(), valid, parser.osc52Primary,
                    parser.osc52Clipboard
                );
            }
        }
    }

    action osc52SelectorEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscInvalid;
    }

    action osc52PayloadEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        parser.osc52PayloadSeen = true;
        parser.osc52Query = false;
        fgoto osc52Payload;
    }

    action oscNotificationKey {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscNotificationKey = fc;
        fgoto oscNotificationEqual;
    }

    action oscNotificationBeginValue {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscNotificationFieldOffset = ragelStringSize();
        fgoto oscNotificationValue;
    }

    action oscNotificationValueData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            if (parser.oscNotificationKey == 'i' &&
                !((fc >= 'a' && fc <= 'z') || (fc >= 'A' && fc <= 'Z') ||
                  (fc >= '0' && fc <= '9') || fc == '_' || fc == '-' ||
                  fc == '+' || fc == '.')) {
                parser.oscNotificationValid = false;
            }
        }
    }

    action oscNotificationFinishField {
        const auto* data = ragelStringData();
        const StringView value(
            data + parser.oscNotificationFieldOffset,
            ragelStringSize() - parser.oscNotificationFieldOffset
        );
        if (parser.oscNotificationKey == 'i') {
            parser.oscNotificationIdOffset = parser.oscNotificationFieldOffset;
            parser.oscNotificationIdLength = value.length();
            if (value.length() > 256) {
                parser.oscNotificationValid = false;
            }
        } else if (parser.oscNotificationKey == 'p') {
            parser.oscNotificationQuery = value == StringView(u8"?");
            parser.oscNotificationClose = value == StringView(u8"close");
            parser.oscNotificationBody = value == StringView(u8"body");
            if (!parser.oscNotificationQuery && !parser.oscNotificationClose &&
                !parser.oscNotificationBody && value != StringView(u8"title")) {
                parser.oscNotificationValid = false;
            }
        } else if (parser.oscNotificationKey == 'e') {
            if (value == StringView(u8"0")) {
                parser.oscNotificationEncoded = false;
            } else if (value == StringView(u8"1")) {
                parser.oscNotificationEncoded = true;
            } else {
                parser.oscNotificationValid = false;
            }
        } else if (parser.oscNotificationKey == 'd') {
            if (value == StringView(u8"0")) {
                parser.oscNotificationFinal = false;
            } else if (value == StringView(u8"1")) {
                parser.oscNotificationFinal = true;
            } else {
                parser.oscNotificationValid = false;
            }
        }
    }

    action oscNotificationNextField {
        ragelAppendString(fc, parser.maxOscBytes);
        fgoto oscNotificationField;
    }

    action oscNotificationBeginPayload {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscNotificationPayloadOffset = ragelStringSize();
        parser.oscNotificationPayloadBytes = 0;
        fgoto oscNotificationPayload;
    }

    action oscNotificationPayloadData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            ++parser.oscNotificationPayloadBytes;
            const u32 limit = parser.oscNotificationEncoded ? 4096 : 2048;
            if (parser.oscNotificationPayloadBytes > limit) {
                parser.oscNotificationValid = false;
            }
        }
    }

    action oscNotificationInvalidData {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
        parser.oscNotificationValid = false;
        fgoto oscNotificationInvalid;
    }

    action oscNotificationSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscNotificationValid = false;
            parser.oscTerminated = false;
            fgoto oscNotificationInvalid;
        } else {
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action oscNotificationBell {
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action oscNotificationDispatch {
        if (parser.oscTerminated && !parser.overflow && parser.oscNotificationValid) {
            const auto* data = ragelStringData();
            const StringView id(
                data + parser.oscNotificationIdOffset, parser.oscNotificationIdLength
            );
            const StringView payload(
                data + parser.oscNotificationPayloadOffset,
                ragelStringSize() - parser.oscNotificationPayloadOffset
            );
            if (parser.oscNotificationQuery) {
                iface.osc_NOTIFICATION_CAPABILITIES(id);
            } else if (parser.oscNotificationClose) {
                iface.osc_NOTIFICATION_CLOSE(id);
            } else if (parser.oscNotificationBody) {
                iface.osc_NOTIFICATION_BODY(
                    id, payload, parser.oscNotificationEncoded,
                    parser.oscNotificationFinal
                );
            } else {
                iface.osc_NOTIFICATION_TITLE(
                    id, payload, parser.oscNotificationEncoded,
                    parser.oscNotificationFinal
                );
            }
        }
    }

    action oscNotificationInvalidSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        } else {
            ragelFinishOsc();
            fnext main;
            fbreak;
        }
    }

    action oscNotificationInvalidBell {
        ragelFinishOsc();
        fnext main;
        fbreak;
    }

    action oscNotificationPayloadEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        parser.oscNotificationPayloadBytes += 2;
        const u32 limit = parser.oscNotificationEncoded ? 4096 : 2048;
        if (parser.oscNotificationPayloadBytes > limit) {
            parser.oscNotificationValid = false;
        }
        fgoto oscNotificationPayload;
    }

    action oscNotificationInvalidEscaped {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscNotificationInvalid;
    }

    action oscColorByte {
        ragelAppendString(fc, parser.maxOscBytes);
    }

    action oscColorQuery {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscColorQuery = true;
    }

    action oscColorNameStart {
        parser.oscColorNameOffset = ragelStringSize();
    }

    action oscColorNameByte {
        ragelAppendString(fc, parser.maxOscBytes);
    }

    action oscColorNameDone {
        const auto* data = ragelStringData();
        parser.oscColorValid = parser.oscColorValid && colorFromName(
            StringView(
                data + parser.oscColorNameOffset,
                ragelStringSize() - parser.oscColorNameOffset
            ),
            parser.oscColor
        );
    }

    action oscColorTealDone {
        parser.oscColor = {0x00, 0x80, 0x80};
    }

    action oscColorHashDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        const u8 digit =
            fc <= '9' ? fc - '0' : (fc | 0x20) - 'a' + 10;
        parser.oscColorHex = (parser.oscColorHex << 4) | digit;
        ++parser.oscColorDigits;
    }

    action oscColorHashDone {
        const u8 width = parser.oscColorDigits / 3;
        const u8 bits = width * 4;
        const u32 mask = (1u << bits) - 1;
        const u32 red = (parser.oscColorHex >> (bits * 2)) & mask;
        const u32 green = (parser.oscColorHex >> bits) & mask;
        const u32 blue = parser.oscColorHex & mask;
        const u8 shift = width < 2 ? 4 : (width - 2) * 4;
        parser.oscColor.red =
            width < 2 ? (u8)(red << shift) : (u8)(red >> shift);
        parser.oscColor.green =
            width < 2 ? (u8)(green << shift) : (u8)(green >> shift);
        parser.oscColor.blue =
            width < 2 ? (u8)(blue << shift) : (u8)(blue >> shift);
    }

    action oscColorRgbComponentStart {
        parser.oscColorHex = 0;
        parser.oscColorDigits = 0;
    }

    action oscColorRgbComponentDone {
        const u32 maximum = (1u << (4 * parser.oscColorDigits)) - 1;
        const u8 value =
            (u8)((parser.oscColorHex * 255 + maximum / 2) / maximum);
        if (parser.oscColorComponent == 0) {
            parser.oscColor.red = value;
        } else if (parser.oscColorComponent == 1) {
            parser.oscColor.green = value;
        } else {
            parser.oscColor.blue = value;
        }
    }

    action oscColorNextComponent {
        ragelAppendString(fc, parser.maxOscBytes);
        ++parser.oscColorComponent;
    }

    action oscColorNumberStart {
        parser.oscColorMantissa = 0.0;
        parser.oscColorFraction = 0.1;
        parser.oscColorExponent = 0;
        parser.oscColorNegative = false;
        parser.oscColorExponentNegative = false;
    }

    action oscColorNegative {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscColorNegative = true;
    }

    action oscColorNumberSign {
        ragelAppendString(fc, parser.maxOscBytes);
    }

    action oscColorIntegerDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscColorMantissa =
            parser.oscColorMantissa * 10.0 + (double)(fc - '0');
    }

    action oscColorPoint {
        ragelAppendString(fc, parser.maxOscBytes);
    }

    action oscColorFractionDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscColorMantissa += (double)(fc - '0') * parser.oscColorFraction;
        parser.oscColorFraction *= 0.1;
    }

    action oscColorExponentStart {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscColorExponent = 0;
        parser.oscColorExponentNegative = false;
    }

    action oscColorExponentNegative {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscColorExponentNegative = true;
    }

    action oscColorExponentSign {
        ragelAppendString(fc, parser.maxOscBytes);
    }

    action oscColorExponentDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        if (parser.oscColorExponent < 10000) {
            parser.oscColorExponent = parser.oscColorExponent * 10 + fc - '0';
            if (parser.oscColorExponent > 10000) {
                parser.oscColorExponent = 10000;
            }
        }
    }

    action oscColorNumberDone {
        parser.oscColorValid =
            finishColorNumber(
                parser.oscColorMantissa,
                parser.oscColorNegative,
                parser.oscColorExponent,
                parser.oscColorExponentNegative,
                parser.oscColorComponents[parser.oscColorComponent]
            ) &&
            parser.oscColorValid;
    }

    action oscColorRgbIntensity {
        parser.oscColorValid = parser.oscColorValid && colorFromRgbIntensity(
            parser.oscColorComponents[0], parser.oscColorComponents[1],
            parser.oscColorComponents[2], parser.oscColor
        );
    }

    action oscColorCieXyz {
        parser.oscColorValid = parser.oscColorValid && colorFromCieXyz(
            parser.oscColorComponents[0], parser.oscColorComponents[1],
            parser.oscColorComponents[2], parser.oscColor
        );
    }

    action oscColorCieUvY {
        parser.oscColorValid = parser.oscColorValid && colorFromCieUvY(
            parser.oscColorComponents[0], parser.oscColorComponents[1],
            parser.oscColorComponents[2], parser.oscColor
        );
    }

    action oscColorCieXyY {
        parser.oscColorValid = parser.oscColorValid && colorFromCieXyY(
            parser.oscColorComponents[0], parser.oscColorComponents[1],
            parser.oscColorComponents[2], parser.oscColor
        );
    }

    action oscColorCieLab {
        parser.oscColorValid = parser.oscColorValid && colorFromCieLab(
            parser.oscColorComponents[0], parser.oscColorComponents[1],
            parser.oscColorComponents[2], parser.oscColor
        );
    }

    action oscColorCieLuv {
        parser.oscColorValid = parser.oscColorValid && colorFromCieLuv(
            parser.oscColorComponents[0], parser.oscColorComponents[1],
            parser.oscColorComponents[2], parser.oscColor
        );
    }

    action oscColorTekHvc {
        parser.oscColorValid = parser.oscColorValid && colorFromTekHvc(
            parser.oscColorComponents[0], parser.oscColorComponents[1],
            parser.oscColorComponents[2], parser.oscColor
        );
    }

    action oscDynamicColorNext {
        ragelAppendString(fc, parser.maxOscBytes);
        ++parser.oscCommand;
        if (parser.oscCommand > 19) {
            fgoto oscInvalid;
        }
        resetOscColor();
        fgoto oscDynamicColor;
    }

    action oscDynamicColorCommit {
        if (!parser.overflow && parser.oscColorValid) {
            if (parser.oscCommand == 10) {
                iface.osc_DEFAULT_FOREGROUND(parser.oscColor, parser.oscColorQuery);
            } else if (parser.oscCommand == 11) {
                iface.osc_DEFAULT_BACKGROUND(parser.oscColor, parser.oscColorQuery);
            } else if (parser.oscCommand == 12) {
                iface.osc_CURSOR_COLOR(parser.oscColor, parser.oscColorQuery);
            } else if (parser.oscCommand == 17) {
                iface.osc_SELECTION_BACKGROUND(parser.oscColor, parser.oscColorQuery);
            } else if (parser.oscCommand == 19) {
                iface.osc_SELECTION_FOREGROUND(parser.oscColor, parser.oscColorQuery);
            }
        }
    }

    action oscDynamicColorSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscTerminated = false;
            fgoto oscInvalid;
        } else {
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action oscDynamicColorBell {
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action oscDynamicColorInvalid {
        fhold;
        fgoto oscDynamicColorDiscard;
    }

    action oscDynamicColorDiscardNext {
        ragelAppendString(fc, parser.maxOscBytes);
        ++parser.oscCommand;
        if (parser.oscCommand > 19) {
            fgoto oscInvalid;
        } else {
            resetOscColor();
            fgoto oscDynamicColor;
        }
    }

    action oscDynamicColorEscapedInvalid {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscDynamicColorDiscard;
    }

    action oscIndexedColorIndexDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscFieldPresent = true;
        if (parser.oscFieldNumber > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.oscFieldNumeric = false;
        } else {
            parser.oscFieldNumber = parser.oscFieldNumber * 10 + fc - '0';
        }
    }

    action oscIndexedColorIndexInvalid {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
        }
        parser.oscFieldNumeric = false;
    }

    action oscIndexedColorIndexEscapedInvalid {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        parser.oscFieldNumeric = false;
        fgoto oscIndexedColorIndex;
    }

    action oscIndexedColorBegin {
        ragelAppendString(fc, parser.maxOscBytes);
        if (!parser.oscFieldPresent || !parser.oscFieldNumeric) {
            fgoto oscIndexedColorDiscard;
        }
        resetOscColor();
        fgoto oscIndexedColor;
    }

    action oscIndexedColorCommit {
        if (!parser.overflow && parser.oscColorValid) {
            if (parser.oscCommand == 4) {
                iface.osc_PALETTE(
                    parser.oscFieldNumber, parser.oscColor, parser.oscColorQuery
                );
            } else {
                iface.osc_SPECIAL_COLOR(
                    parser.oscFieldNumber, parser.oscColor, parser.oscColorQuery
                );
            }
        }
    }

    action oscIndexedColorNext {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscFieldNumber = 0;
        parser.oscFieldNumeric = true;
        parser.oscFieldPresent = false;
        fgoto oscIndexedColorIndex;
    }

    action oscIndexedColorSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscTerminated = false;
            fgoto oscInvalid;
        } else {
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action oscIndexedColorBell {
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action oscIndexedColorInvalid {
        fhold;
        fgoto oscIndexedColorDiscard;
    }

    action oscIndexedColorDiscardNext {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscFieldNumber = 0;
        parser.oscFieldNumeric = true;
        parser.oscFieldPresent = false;
        fgoto oscIndexedColorIndex;
    }

    action oscIndexedColorEscapedInvalid {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        fgoto oscIndexedColorDiscard;
    }

    action oscNumericDigit {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscFieldPresent = true;
        if (parser.oscFieldNumber > (UINT32_MAX - (u32)(fc - '0')) / 10) {
            parser.oscFieldNumeric = false;
        } else {
            parser.oscFieldNumber = parser.oscFieldNumber * 10 + fc - '0';
        }
    }

    action oscNumericInvalid {
        consumeStringUtf8Byte(fc);
        if (!executeC0(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscFieldPresent = true;
            parser.oscFieldNumeric = false;
        }
    }

    action oscNumericField {
        const bool valid = parser.oscFieldPresent && parser.oscFieldNumeric;
        if (parser.oscCommand == 6 || parser.oscCommand == 106) {
            if (!parser.oscFieldHaveFirst) {
                parser.oscFieldFirst = parser.oscFieldNumber;
                parser.oscFieldFirstValid = valid;
                parser.oscFieldHaveFirst = true;
            } else {
                if (!parser.overflow && parser.oscFieldFirstValid && valid) {
                    iface.osc_SPECIAL_COLOR_MODE(
                        parser.oscFieldFirst, parser.oscFieldNumber
                    );
                }
                parser.oscFieldHaveFirst = false;
            }
        } else if (!parser.overflow && valid) {
            if (parser.oscCommand == 104) {
                iface.osc_RESET_PALETTE(parser.oscFieldNumber);
            } else {
                iface.osc_RESET_SPECIAL_COLOR(parser.oscFieldNumber);
            }
        }
    }

    action oscNumericFinalField {
        if (parser.oscFieldPresent) {
            const bool valid = parser.oscFieldNumeric;
            if (parser.oscCommand == 6 || parser.oscCommand == 106) {
                if (!parser.oscFieldHaveFirst) {
                    parser.oscFieldFirst = parser.oscFieldNumber;
                    parser.oscFieldFirstValid = valid;
                    parser.oscFieldHaveFirst = true;
                } else if (!parser.overflow &&
                           parser.oscFieldFirstValid && valid) {
                    iface.osc_SPECIAL_COLOR_MODE(
                        parser.oscFieldFirst, parser.oscFieldNumber
                    );
                }
            } else if (!parser.overflow && valid) {
                if (parser.oscCommand == 104) {
                    iface.osc_RESET_PALETTE(parser.oscFieldNumber);
                } else {
                    iface.osc_RESET_SPECIAL_COLOR(parser.oscFieldNumber);
                }
            }
        } else if (!parser.overflow &&
                   ragelStringSize() == parser.oscPayloadOffset) {
            if (parser.oscCommand == 104) {
                iface.osc_RESET_PALETTE();
            } else if (parser.oscCommand == 105) {
                iface.osc_RESET_SPECIAL_COLOR();
            }
        }
    }

    action oscNumericSeparator {
        ragelAppendString(fc, parser.maxOscBytes);
        parser.oscFieldNumber = 0;
        parser.oscFieldNumeric = true;
        parser.oscFieldPresent = false;
    }

    action oscNumericSt {
        if (consumeStringUtf8Byte(fc)) {
            ragelAppendString(fc, parser.maxOscBytes);
            parser.oscFieldPresent = true;
            parser.oscFieldNumeric = false;
            fgoto oscNumericFields;
        } else {
            ragelFinishOsc();
            parser.oscTerminated = true;
        }
    }

    action oscNumericBell {
        ragelFinishOsc();
        parser.oscTerminated = true;
    }

    action oscNumericEscapedInvalid {
        ragelAppendEscapedString(fc, parser.maxOscBytes);
        parser.oscFieldPresent = true;
        parser.oscFieldNumeric = false;
        fgoto oscNumericFields;
    }

    action ignoredData {
        if (fc >= 0x20 && fc < 0x7f) {
            const size_t count = printableAsciiPrefix(p, pe - p);
            parser.stringUtf8Remaining = 0;
            if constexpr (traced) {
                parserTrace->stringData(p, count);
            }
            p += count - 1;
        } else if (fc >= 0xa0) {
            const size_t count = highStringPrefix(p, pe - p);
            if constexpr (traced) {
                parserTrace->stringData(p, count);
            }
            p += count - 1;
        } else {
            consumeStringUtf8Byte(fc);
            if constexpr (traced) {
                parserTrace->stringData(&fc, 1);
            }
        }
    }

    action ignoredSt {
        if (consumeStringUtf8Byte(fc)) {
            if constexpr (traced) {
                parserTrace->stringData(&fc, 1);
            }
        } else {
            parser.stringUtf8Remaining = 0;
            parser.stringLimit = 0;
            if constexpr (traced) {
                parserTrace->stringEnd();
            }
            fnext main;
            fbreak;
        }
    }

    action ignoredEscape {
        fgoto stringEscape;
    }

    action ignoredEscapedData {
        if constexpr (traced) {
            const u8 bytes[] = {'\x1b', (u8)(fc)};
            parserTrace->stringData(bytes, sizeof(bytes));
        }
        fgoto string;
    }

    action stringRestartDcs {
        if (!ragelStringContinuation(fc)) {
            ragelBeginDcs();
            fgoto dcsEntry;
        }
    }

    action stringRestartOsc {
        if (!ragelStringContinuation(fc)) {
            ragelBeginOsc();
            fgoto oscCommand;
        }
    }

    action stringRestartSos {
        if (!ragelStringContinuation(fc)) {
            ragelBeginString(VtermTraceString::Sos, false);
            fgoto string;
        }
    }

    action stringRestartPm {
        if (!ragelStringContinuation(fc)) {
            ragelBeginString(VtermTraceString::Pm, false);
            fgoto string;
        }
    }

    action stringRestartApc {
        if (!ragelStringContinuation(fc)) {
            ragelBeginString(VtermTraceString::Apc, false);
            fgoto string;
        }
    }

    action stringRestartCsi {
        if (!ragelStringContinuation(fc)) {
            beginCsi();
            fgoto csiEntry;
        }
    }

    action stringControlSpa {
        if (!ragelStringContinuation(fc)) {
            parser.stringUtf8Remaining = 0;
            parser.stringLimit = 0;
            if constexpr (traced) {
                parserTrace->stringCancel();
                parserTrace->control(fc);
            }
            iface.esc_SPA();
            fnext main;
            fbreak;
        }
    }

    action stringControlEpa {
        if (!ragelStringContinuation(fc)) {
            parser.stringUtf8Remaining = 0;
            parser.stringLimit = 0;
            if constexpr (traced) {
                parserTrace->stringCancel();
                parserTrace->control(fc);
            }
            iface.esc_EPA();
            fnext main;
            fbreak;
        }
    }

    action stringControlDa {
        if (!ragelStringContinuation(fc)) {
            parser.stringUtf8Remaining = 0;
            parser.stringLimit = 0;
            if constexpr (traced) {
                parserTrace->stringCancel();
                parserTrace->control(fc);
            }
            iface.csi_priDA();
            fnext main;
            fbreak;
        }
    }

    action stringC1ToGround {
        if (!iface.parserGroundUtf8Enabled()) {
            parser.stringUtf8Remaining = 0;
            parser.stringLimit = 0;
            if constexpr (traced) {
                parserTrace->stringCancel();
            }
            fhold;
            fgoto main;
        }
    }

    csiPlainKnown = [@ABCDEFGHIJKLMPSTXZ`abcdefghijklmnqrstux];
    csiPlainFinal = (
        'T' @csiTrace @{ if (parser.parameterCount == 5 && iface.parserHighlightMouseTracking()) { iface.csi_XTHIMOUSE(parameter(0), parameter(1), parameter(2), parameter(3), parameter(4)); } else { iface.csi_SD(countParameter(0)); } } |
        'A' @csiTrace @{ iface.csi_CUU(countParameter(0)); } |
        'B' @csiTrace @{ iface.csi_CUD(countParameter(0)); } |
        'C' @csiTrace @{ iface.csi_CUF(countParameter(0)); } |
        'D' @csiTrace @{ iface.csi_CUB(countParameter(0)); } |
        'E' @csiTrace @{ iface.csi_CNL(countParameter(0)); } |
        'F' @csiTrace @{ iface.csi_CPL(countParameter(0)); } |
        'G' @csiTrace @{ iface.csi_CHA(countParameter(0)); } |
        ('H' | 'f') @csiTrace @{ iface.csi_CUP(countParameter(0), countParameter(1)); } |
        'I' @csiTrace @{ iface.csi_CHT(countParameter(0)); } |
        'J' @csiTrace @{ dispatchEraseDisplay(false); } |
        'K' @csiTrace @{ dispatchEraseLine(false); } |
        'L' @csiTrace @{ iface.csi_IL(countParameter(0)); } |
        'M' @csiTrace @{ iface.csi_DL(countParameter(0)); } |
        'P' @csiTrace @{ iface.csi_DCH(countParameter(0)); } |
        'S' @csiTrace @{ iface.csi_SU(countParameter(0)); } |
        'X' @csiTrace @{ iface.csi_ECH(countParameter(0)); } |
        'Z' @csiTrace @{ iface.csi_CBT(countParameter(0)); } |
        '@' @csiTrace @{ iface.csi_ICH(parser.parameters[0] ? parser.parameters[0] : 1); } |
        '`' @csiTrace @{ iface.csi_HPA(countParameter(0)); } |
        'a' @csiTrace @{ iface.csi_HPR(countParameter(0)); } |
        'b' @csiTrace @{ iface.csi_REP(countParameter(0)); } |
        'c' @csiTrace @{ iface.csi_priDA(); } |
        'd' @csiTrace @{ iface.csi_VPA(countParameter(0)); } |
        'e' @csiTrace @{ iface.csi_VPR(countParameter(0)); } |
        'g' @csiTrace @{ dispatchTabClear(); } |
        'h' @csiTrace @{ dispatchStandardModes(true); } |
        'i' @csiTrace |
        'j' @csiTrace @{ iface.csi_CUB(countParameter(0)); } |
        'k' @csiTrace @{ iface.csi_CUU(countParameter(0)); } |
        'l' @csiTrace @{ dispatchStandardModes(false); } |
        'm' @csiTrace @csiSgr |
        'n' @csiTrace @{ dispatchDsr(false); } |
        'q' @csiTrace @{ dispatchDecll(); } |
        'r' @csiTrace @{ iface.csi_STBM(parameter(0), parameter(1), parser.parameterCount <= 2); } |
        's' @csiTrace @{ dispatchScoscSlrm(); } |
        't' @csiTrace @{ dispatchWindowOps(); } |
        'u' @csiTrace @{ iface.csi_SCORC(); } |
        'x' @csiTrace @{ if (parameter(0) <= 1) { iface.csi_DECREQTPARM(parameter(0)); } } |
        (0x40..0x7e - csiPlainKnown) @csiTrace
    ) @csiDone;

    csiGreaterKnown = [MTcmqtu];
    csiGreaterFinal = (
        'M' @csiTrace @{ iface.csi_SETMARK(); } |
        'T' @csiTrace @{ dispatchTitleMode(false); } |
        'c' @csiTrace @{ iface.csi_secDA(); } |
        'm' @csiTrace @{ dispatchXtmodkeys(); } |
        'q' @csiTrace @{ if (parameter(0) == 0) { iface.csi_XTVERSION(); } } |
        't' @csiTrace @{ dispatchTitleMode(true); } |
        'u' @csiTrace @{ iface.csi_kittyKeyboardPush(parameter(0)); } |
        (0x40..0x7e - csiGreaterKnown) @csiTrace
    ) @csiDone;

    csiLessFinal = (
        'u' @csiTrace @{ iface.csi_kittyKeyboardPop(countParameter(0)); } |
        (0x40..0x7e - 'u') @csiTrace
    ) @csiDone;

    csiEqualKnown = [cu];
    csiEqualFinal = (
        'c' @csiTrace @{ iface.csi_terDA(); } |
        'u' @csiTrace @{ dispatchKittyKeyboardSet(); } |
        (0x40..0x7e - csiEqualKnown) @csiTrace
    ) @csiDone;

    csiQuestionKnown = [JKSWhilmnrsu];
    csiQuestionFinal = (
        'J' @csiTrace @{ dispatchEraseDisplay(true); } |
        'S' @csiTrace @{ iface.csi_XTSMGRAPHICS(parameter(0), parameter(1), parameter(2)); } |
        'K' @csiTrace @{ dispatchEraseLine(true); } |
        'W' @csiTrace @{ if (parameter(0) == 0 || parameter(0) == 5) { iface.resetTabStops(); } } |
        'h' @csiTrace @{ dispatchPrivateModes(true); } |
        'i' @csiTrace |
        'l' @csiTrace @{ dispatchPrivateModes(false); } |
        'm' @csiTrace @{ dispatchXtqmodkeys(); } |
        'n' @csiTrace @{ dispatchDsr(true); } |
        'r' @csiTrace @{ dispatchPrivateRestore(); } |
        's' @csiTrace @{ dispatchPrivateSave(); } |
        'u' @csiTrace @{ iface.csi_kittyKeyboardQuery(); } |
        (0x40..0x7e - csiQuestionKnown) @csiTrace
    ) @csiDone;

    csiBangFinal = (
        'p' @csiTrace @{ iface.csi_DECSTR(); } |
        (0x40..0x7e - 'p') @csiTrace
    ) @csiDone;

    csiQuoteKnown = [pqv];
    csiQuoteFinal = (
        'p' @csiTrace @{ dispatchDecscl(); } |
        'q' @csiTrace @{ iface.setDecProtection(parameter(0) == 1); } |
        'v' @csiTrace @{ if (!parser.csiHadParameters && iface.parserCompatibilityLevel() >= CompatibilityLevel::VT300) { iface.csi_DECRQDE(); } } |
        (0x40..0x7e - csiQuoteKnown) @csiTrace
    ) @csiDone;

    csiSpaceKnown = [@Aq];
    csiSpaceFinal = (
        '@' @csiTrace @{ iface.csi_ecma48_SL(countParameter(0)); } |
        'A' @csiTrace @{ iface.csi_ecma48_SR(countParameter(0)); } |
        'q' @csiTrace @{ dispatchCursorStyle(); } |
        (0x40..0x7e - csiSpaceKnown) @csiTrace
    ) @csiDone;

    csiApostropheKnown = [wz-~];
    csiApostropheFinal = (
        'w' @csiTrace @{ iface.csi_DECEFR(parameter(0), parameter(1), parameter(2), parameter(3)); } |
        'z' @csiTrace @{ dispatchLocatorReporting(); } |
        '{' @csiTrace @{ dispatchDecsle(); } |
        '|' @csiTrace @{ iface.csi_DECRQLP(); } |
        '}' @csiTrace @{ iface.csi_DECIC(countParameter(0)); } |
        '~' @csiTrace @{ iface.csi_DECDC(countParameter(0)); } |
        (0x40..0x7e - csiApostropheKnown) @csiTrace
    ) @csiDone;

    csiDollarKnown = [prtuvwxz{];
    csiDollarFinal = (
        'p' @csiTrace @{ dispatchModeReport(false); } |
        'r' @csiTrace @{ dispatchDeccara(false); } |
        't' @csiTrace @{ dispatchDeccara(true); } |
        'u' @csiTrace @{
            if (parser.parameterCount == 2 && parameter(0) == 2 &&
                (parameter(1) == 1 || parameter(1) == 2)) {
                iface.csi_DECRQTSR_COLOR(parameter(1));
            }
        } |
        'v' @csiTrace @{ dispatchDeccra(); } |
        'w' @csiTrace @{
            if (parser.parameterCount == 1) {
                if (parameter(0) == 1) {
                    iface.csi_DECRQPSR_CURSOR();
                } else if (parameter(0) == 2) {
                    iface.csi_DECRQPSR_TABS();
                }
            }
        } |
        'x' @csiTrace @{ dispatchDecfra(); } |
        'z' @csiTrace @{ dispatchDecera(false); } |
        '{' @csiTrace @{ dispatchDecera(true); } |
        (0x40..0x7e - csiDollarKnown) @csiTrace
    ) @csiDone;

    csiStarFinal = (
        'x' @csiTrace @{ iface.setAttributeChangeExtent(parameter(0) == 2); } |
        'y' @csiTrace @{ dispatchDecrqcra(); } |
        (0x40..0x7e - [xy]) @csiTrace
    ) @csiDone;

    csiCommaFinal = (
        '|' @csiTrace @{ dispatchDecac(); } |
        (0x40..0x7e - '|') @csiTrace
    ) @csiDone;

    csiHashFinal = (
        'y' @csiTrace @{ iface.csi_XTCHECKSUM(parameter(0)); } |
        '{' @csiTrace @{ iface.csi_XTPUSHSGR(parser.parameters, parser.csiHadParameters ? parser.parameterCount : 0); } |
        '}' @csiTrace @{ iface.csi_XTPOPSGR(); } |
        (0x40..0x7e - [y{}]) @csiTrace
    ) @csiDone;

    csiAmpersandFinal = (
        'u' @csiTrace @{ iface.csi_DECRQUPSS(); } |
        (0x40..0x7e - 'u') @csiTrace
    ) @csiDone;

    csiQuestionDollarFinal = (
        'p' @csiTrace @{ dispatchModeReport(true); } |
        (0x40..0x7e - 'p') @csiTrace
    ) @csiDone;

    csiUnknownFinal = 0x40..0x7e @csiTrace @csiDone;

    cancel = (0x18 | 0x1a) @cancel;
    restartEscape = 0x1b @beginEscape;
    sequenceC0 = (0x00..0x17 | 0x19 | 0x1c..0x1f) @sequenceC0;
    dcsPayloadC0 = (0x00..0x17 | 0x19 | 0x1c..0x1f) @dcsPayloadC0;
    dcsHeaderC0 = 0x00..0x17 | 0x19 | 0x1c..0x1f;
    highToGround = 0xa0..0xff @highToGround;
    c1Other = (
        0x80..0x83 |
        0x86..0x87 |
        0x89..0x8c |
        0x91..0x95 |
        0x99
    ) @c1ToGround;

    c1Dispatch = (
        0x84 @c1Ind |
        0x85 @c1Nel |
        0x88 @c1Hts |
        0x8d @c1Ri |
        0x8e @c1Ss2 |
        0x8f @c1Ss3 |
        0x90 @beginDcs |
        0x96 @c1Spa |
        0x97 @c1Epa |
        0x98 @beginSos |
        0x9a @c1Da |
        0x9b @beginCsi |
        0x9c @c1St |
        0x9d @beginOsc |
        0x9e @beginPm |
        0x9f @beginApc
    );

    stringC1 = (
        (0x80..0x8f | 0x91..0x95 | 0x99) @stringC1ToGround |
        0x90 @stringRestartDcs |
        0x96 @stringControlSpa |
        0x97 @stringControlEpa |
        0x98 @stringRestartSos |
        0x9a @stringControlDa |
        0x9b @stringRestartCsi |
        0x9d @stringRestartOsc |
        0x9e @stringRestartPm |
        0x9f @stringRestartApc
    );

    oscColorGap = (
        cancel |
        stringC1 |
        0x7f |
        sequenceC0
    );

    oscColorA = [aA] @oscColorByte oscColorGap*;
    oscColorB = [bB] @oscColorByte oscColorGap*;
    oscColorC = [cC] @oscColorByte oscColorGap*;
    oscColorE = [eE] @oscColorByte oscColorGap*;
    oscColorG = [gG] @oscColorByte oscColorGap*;
    oscColorH = [hH] @oscColorByte oscColorGap*;
    oscColorI = [iI] @oscColorByte oscColorGap*;
    oscColorK = [kK] @oscColorByte oscColorGap*;
    oscColorL = [lL] @oscColorByte oscColorGap*;
    oscColorR = [rR] @oscColorByte oscColorGap*;
    oscColorT = [tT] @oscColorByte oscColorGap*;
    oscColorU = [uU] @oscColorByte oscColorGap*;
    oscColorV = [vV] @oscColorByte oscColorGap*;
    oscColorX = [xX] @oscColorByte oscColorGap*;
    oscColorY = [yY] @oscColorByte oscColorGap*;
    oscColorZ = [zZ] @oscColorByte oscColorGap*;
    oscColorColon = ':' @oscColorByte;

    oscColorHexDigit =
        oscColorGap* xdigit @oscColorHashDigit;

    oscColorIntegerDigit =
        oscColorGap* digit @oscColorIntegerDigit;

    oscColorFractionDigit =
        oscColorGap* digit @oscColorFractionDigit;

    oscColorExponentDigit =
        oscColorGap* digit @oscColorExponentDigit;

    oscColorNumber = (
        oscColorGap*
        (
            '+' @oscColorNumberSign |
            '-' @oscColorNegative
        )?
        (
            oscColorIntegerDigit+
            (
                oscColorGap* '.' @oscColorPoint
                oscColorFractionDigit*
            )? |
            oscColorGap* '.' @oscColorPoint
            oscColorFractionDigit+
        )
        (
            oscColorGap* [eE] @oscColorExponentStart
            (
                oscColorGap* '+' @oscColorExponentSign |
                oscColorGap* '-' @oscColorExponentNegative
            )?
            oscColorExponentDigit+
        )?
    ) >oscColorNumberStart %oscColorNumberDone;

    oscColorSlash =
        oscColorGap* '/' @oscColorNextComponent;

    oscColorTriple =
        oscColorNumber
        oscColorSlash
        oscColorNumber
        oscColorSlash
        oscColorNumber;

    oscColorHash = (
        '#' @oscColorByte
        (
            oscColorHexDigit{3} |
            oscColorHexDigit{6} |
            oscColorHexDigit{9} |
            oscColorHexDigit{12}
        )
    ) %oscColorHashDone;

    oscColorRgbComponent = (
        oscColorHexDigit{1,4}
    ) >oscColorRgbComponentStart %oscColorRgbComponentDone;

    oscColorRgb = (
        oscColorR oscColorG oscColorB oscColorColon
        oscColorRgbComponent
        oscColorSlash
        oscColorRgbComponent
        oscColorSlash
        oscColorRgbComponent
    );

    oscColorRgbIntensity = (
        oscColorR oscColorG oscColorB oscColorI oscColorColon
        oscColorTriple
    ) %oscColorRgbIntensity;

    oscColorCieXyz = (
        oscColorC oscColorI oscColorE
        oscColorX oscColorY oscColorZ oscColorColon
        oscColorTriple
    ) %oscColorCieXyz;

    oscColorCieUvY = (
        oscColorC oscColorI oscColorE
        oscColorU oscColorV oscColorY oscColorColon
        oscColorTriple
    ) %oscColorCieUvY;

    oscColorCieXyY = (
        oscColorC oscColorI oscColorE
        oscColorX oscColorY oscColorY oscColorColon
        oscColorTriple
    ) %oscColorCieXyY;

    oscColorCieLab = (
        oscColorC oscColorI oscColorE
        oscColorL oscColorA oscColorB oscColorColon
        oscColorTriple
    ) %oscColorCieLab;

    oscColorCieLuv = (
        oscColorC oscColorI oscColorE
        oscColorL oscColorU oscColorV oscColorColon
        oscColorTriple
    ) %oscColorCieLuv;

    oscColorTealOrTekHvc = (
        oscColorT oscColorE
        (
            oscColorA oscColorL %oscColorTealDone |
            (
                oscColorK oscColorH oscColorV oscColorC oscColorColon
                oscColorTriple
            ) %oscColorTekHvc
        )
    );

    oscColorNameByteChar = [a-zA-Z0-9 ] @oscColorNameByte;

    oscColorName = (
        ([a-sA-Su-zU-Z0-9 ] @oscColorNameByte)
        oscColorNameByteChar*
    ) >oscColorNameStart %oscColorNameDone;

    oscColorNameT = (
        ([tT] @oscColorNameByte)
        ([a-df-zA-DF-Z0-9 ] @oscColorNameByte)
        oscColorNameByteChar*
    ) >oscColorNameStart %oscColorNameDone;

    oscColorValue = (
        oscColorGap*
        (
            '?' @oscColorQuery |
            oscColorHash |
            oscColorRgb |
            oscColorRgbIntensity |
            oscColorCieXyz |
            oscColorCieUvY |
            oscColorCieXyY |
            oscColorCieLab |
            oscColorCieLuv |
            oscColorTealOrTekHvc |
            oscColorName |
            oscColorNameT
        )
        oscColorGap*
    );

    main := (
        0x00 @groundIgnored |
        0x01..0x06 @groundIgnored |
        0x07 @groundBell |
        0x08 @groundBackspace |
        0x09 @groundTab |
        0x0a..0x0c @groundLineFeed |
        0x0d @groundCarriageReturn |
        0x0e @groundShiftOut |
        0x0f @groundShiftIn |
        0x10..0x17 @groundIgnored |
        0x18 @groundIgnored |
        0x19 @groundIgnored |
        0x1a @groundIgnored |
        0x1b @beginEscape |
        0x1c..0x1f @groundIgnored |
        0x20..0x7e @groundAscii |
        0x7f @groundDone |
        0x80..0x83 @groundHigh |
        0x84 @groundC1Ind |
        0x85 @groundC1Nel |
        0x86..0x87 @groundHigh |
        0x88 @groundC1Hts |
        0x89..0x8c @groundHigh |
        0x8d @groundC1Ri |
        0x8e @groundC1Ss2 |
        0x8f @groundC1Ss3 |
        0x90 @groundC1Dcs |
        0x91..0x95 @groundHigh |
        0x96 @groundC1Spa |
        0x97 @groundC1Epa |
        0x98 @groundC1Sos |
        0x99 @groundHigh |
        0x9a @groundC1Da |
        0x9b @groundC1Csi |
        0x9c @groundC1St |
        0x9d @groundC1Osc |
        0x9e @groundC1Pm |
        0x9f @groundC1Apc |
        0xa0..0xff @groundHigh
    )*;

    discardUtf8 := (
        0x80..0xbf @{
            if (--parser.discardedUtf8Remaining == 0) {
                fgoto main;
            }
        } |
        (any - 0x80..0xbf) @{
            parser.discardedUtf8Remaining = 0;
            fhold;
            fgoto main;
        }
    )*;

    escape := (
        cancel |
        0x1b @repeatEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        0x00..0x17 @escapeC0 |
        0x19 @escapeC0 |
        0x1c..0x1f @escapeC0 |
        ' ' @escapeSpace |
        '#' @escapeHash |
        '%' @escapePercent |
        ('(' | ',') @charsetG0 |
        '$' @charsetG0Multibyte |
        ')' @charsetG1 |
        '*' @charsetG2 |
        '+' @charsetG3 |
        '-' @charsetG1_96 |
        '.' @charsetG2_96 |
        '/' @charsetG3_96 |
        '[' @beginCsi |
        ']' @beginOsc |
        'X' @beginSos |
        '^' @beginPm |
        '_' @beginApc |
        'P' @beginDcs |
        '\\' @escapeStringTerminator |
        'D' @escapeInd |
        'M' @escapeRi |
        'E' @escapeNel |
        'H' @escapeHts |
        'N' @escapeSs2 |
        'O' @escapeSs3 |
        'V' @escapeSpa |
        'W' @escapeEpa |
        'Z' @escapeDa |
        'c' @escapeRis |
        '6' @escapeBi |
        '7' @escapeDecsc |
        '8' @escapeDecrc |
        '9' @escapeFi |
        '=' @escapeAppKeypad |
        '>' @escapeNormalKeypad |
        '<' @escapeAnsi |
        '~' @escapeLs1r |
        'n' @escapeLs2 |
        '}' @escapeLs2r |
        'o' @escapeLs3 |
        '|' @escapeLs3r |
        0x20..0x2f @escapeIntermediate |
        0x30..0x7e @escapeFinal
    )*;

    escapeIntermediate := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        0x20..0x2f @intermediateByte |
        0x30..0x7e @intermediateFinal
    )*;

    escapeSpace := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        0x20..0x2f @specialIntermediate |
        'F' @specialFinal @{ if (iface.parserCompatibilityLevel() >= CompatibilityLevel::VT200) { iface.parserSet8BitControls(false); } fnext main; fbreak; } |
        'G' @specialFinal @{ if (iface.parserCompatibilityLevel() >= CompatibilityLevel::VT200) { iface.parserSet8BitControls(true); } fnext main; fbreak; } |
        ('L' | 'M' | 'N') @specialFinal @{ fnext main; fbreak; } |
        (0x30..0x7e - [FGLMN]) @specialFinal @vt52Unhandled
    )*;

    escapeHash := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        0x20..0x2f @specialIntermediate |
        '3' @specialFinal @{ iface.setLineAttribute(1); fnext main; fbreak; } |
        '4' @specialFinal @{ iface.setLineAttribute(2); fnext main; fbreak; } |
        '5' @specialFinal @{ iface.setLineAttribute(0); fnext main; fbreak; } |
        '6' @specialFinal @{ iface.setLineAttribute(3); fnext main; fbreak; } |
        '8' @specialFinal @{ iface.esch_DECALN(); fnext main; fbreak; } |
        (0x30..0x7e - [34568]) @specialFinal @vt52Unhandled
    )*;

    escapePercent := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        0x20..0x2f @specialIntermediate |
        '@' @specialFinal @{ iface.parserResetCharsets(true); fnext main; fbreak; } |
        'G' @specialFinal @{ iface.parserResetCharsets(false); fnext main; fbreak; } |
        (0x30..0x7e - [@G]) @specialFinal @vt52Unhandled
    )*;

    selectCharset := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        0x20..0x2f @charsetModifier |
        0x30..0x7e @{ designateCharset(fc); } @charsetFinal
    )*;

    csiEntry := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        '0'..'9' @csiDigit @{ fgoto csiParameter; } |
        (';' | ':') @csiSeparator @{ fgoto csiParameter; } |
        0x3c..0x3f @csiPrefix |
        0x20..0x2f @csiIntermediate @{ fgoto csiIntermediate; } |
        0x40..0x7e @csiFinalSelect
    )*;

    csiParameter := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        '0'..'9' @csiDigit |
        (';' | ':') @csiSeparator |
        0x20..0x2f @csiIntermediate @{ fgoto csiIntermediate; } |
        0x40..0x7e @csiFinalSelect |
        0x3c..0x3f @csiInvalid
    )*;

    csiIntermediate := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        0x20..0x2f @csiIntermediate |
        0x40..0x7e @csiIntermediateFinalSelect |
        0x30..0x3f @csiInvalid
    )*;

    csiIgnore := (
        cancel |
        restartEscape |
        c1Dispatch |
        c1Other |
        0x7f |
        highToGround |
        sequenceC0 |
        0x40..0x7e @csiIgnoredFinal |
        0x20..0x3f
    )*;

    csiPlainDispatch := csiPlainFinal $err(csiDispatchInvalid);
    csiGreaterDispatch := csiGreaterFinal $err(csiDispatchInvalid);
    csiLessDispatch := csiLessFinal $err(csiDispatchInvalid);
    csiEqualDispatch := csiEqualFinal $err(csiDispatchInvalid);
    csiQuestionDispatch := csiQuestionFinal $err(csiDispatchInvalid);
    csiBangDispatch := csiBangFinal $err(csiDispatchInvalid);
    csiQuoteDispatch := csiQuoteFinal $err(csiDispatchInvalid);
    csiSpaceDispatch := csiSpaceFinal $err(csiDispatchInvalid);
    csiApostropheDispatch := csiApostropheFinal $err(csiDispatchInvalid);
    csiDollarDispatch := csiDollarFinal $err(csiDispatchInvalid);
    csiStarDispatch := csiStarFinal $err(csiDispatchInvalid);
    csiCommaDispatch := csiCommaFinal $err(csiDispatchInvalid);
    csiHashDispatch := csiHashFinal $err(csiDispatchInvalid);
    csiAmpersandDispatch := csiAmpersandFinal $err(csiDispatchInvalid);
    csiQuestionDollarDispatch := csiQuestionDollarFinal $err(csiDispatchInvalid);
    csiUnknownDispatch := csiUnknownFinal $err(csiDispatchInvalid);

    escapeVt52 := (
        cancel |
        0x1b @repeatEscape |
        c1Dispatch |
        (0x80..0x83 | 0x86..0x87 | 0x89..0x8c |
         0x91..0x95 | 0x99) @escapeFinal |
        0x7f |
        highToGround |
        '=' @vt52AppKeypad |
        '>' @vt52NormalKeypad |
        '<' @vt52Ansi |
        'A' @vt52Cuu |
        'B' @vt52Cud |
        'C' @vt52Cuf |
        'D' @vt52Cub |
        'F' @vt52Graphics |
        'G' @vt52Ascii |
        'H' @vt52Cup |
        'I' @vt52Ri |
        'J' @vt52Ed |
        'K' @vt52El |
        'Y' @vt52CupBegin |
        'Z' @vt52Identify |
        'c' @vt52Ris |
        (any - (0x18 | 0x1a | 0x1b | 0x7f | 0x80..0xff |
                '=' | '>' | '<' | 'A' | 'B' | 'C' | 'D' | 'F' | 'G' |
                'H' | 'I' | 'J' | 'K' | 'Y' | 'Z' | 'c')) @vt52Unhandled
    )*;

    vt52CupRow := any @vt52Row $err(vt52CupInvalid);
    vt52CupColumn := any @vt52Column $err(vt52CupInvalid);

    dcsEntry := (
        cancel |
        stringC1 |
        0x9c @dcsHeaderTerminated |
        0x1b @dcsHeaderEscape |
        0x7f |
        dcsHeaderC0 |
        '0'..'9' @dcsDigit @{ fgoto dcsParameter; } |
        ';' @dcsSeparator @{ fgoto dcsParameter; } |
        ':' @dcsHeaderInvalid |
        0x3c..0x3f @dcsHeaderByte |
        0x20..0x2f @dcsIntermediate @{ fgoto dcsIntermediate; } |
        0x40..0x7e @dcsFinal |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsHeaderInvalid
    )*;

    dcsParameter := (
        cancel |
        stringC1 |
        0x9c @dcsHeaderTerminated |
        0x1b @dcsHeaderEscape |
        0x7f |
        dcsHeaderC0 |
        '0'..'9' @dcsDigit |
        ';' @dcsSeparator |
        ':' @dcsHeaderInvalid |
        0x20..0x2f @dcsIntermediate @{ fgoto dcsIntermediate; } |
        0x40..0x7e @dcsFinal |
        (0x3c..0x3f | 0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsHeaderInvalid
    )*;

    dcsIntermediate := (
        cancel |
        stringC1 |
        0x9c @dcsHeaderTerminated |
        0x1b @dcsHeaderEscape |
        0x7f |
        dcsHeaderC0 |
        0x20..0x2f @dcsIntermediate |
        0x40..0x7e @dcsFinal |
        (0x30..0x3f | 0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsHeaderInvalid
    )*;

    dcsHeaderEscape := (
        cancel |
        stringC1 |
        0x9c @dcsHeaderTerminated |
        '\\' @dcsHeaderTerminated |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    dcsDecrqssGap = (
        cancel |
        stringC1 |
        0x7f |
        dcsPayloadC0
    );

    dcsDecrqssTerminator = (
        0x9c |
        0x1b (
            '\\' |
            0x9c |
            restartEscape |
            (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                    0x9a..0x9f)) @abortStringEscaped |
            cancel |
            stringC1
        )
    );

    dcsDecrqssEntry := (
        dcsDecrqssGap*
        (
            '"' @dcsHeaderByte dcsDecrqssGap* (
                'p' @dcsHeaderByte dcsDecrqssGap*
                    dcsDecrqssTerminator @dcsDecrqssDecsclSt |
                'q' @dcsHeaderByte dcsDecrqssGap*
                    dcsDecrqssTerminator @dcsDecrqssDecscaSt
            ) |
            'm' @dcsHeaderByte dcsDecrqssGap*
                dcsDecrqssTerminator @dcsDecrqssSgrSt |
            'r' @dcsHeaderByte dcsDecrqssGap*
                dcsDecrqssTerminator @dcsDecrqssDecstbmSt |
            's' @dcsHeaderByte dcsDecrqssGap*
                dcsDecrqssTerminator @dcsDecrqssDecslrmSt |
            't' @dcsHeaderByte dcsDecrqssGap*
                dcsDecrqssTerminator @dcsDecrqssDecslppSt |
            ' ' @dcsHeaderByte dcsDecrqssGap*
                'q' @dcsHeaderByte dcsDecrqssGap*
                dcsDecrqssTerminator @dcsDecrqssDecscusrSt |
            '*' @dcsHeaderByte dcsDecrqssGap*
                'x' @dcsHeaderByte dcsDecrqssGap*
                dcsDecrqssTerminator @dcsDecrqssDecsaceSt
        )
    ) $err(dcsDecrqssInvalidStart);

    dcsDecrqssInvalid := (
        cancel |
        stringC1 |
        0x9c @dcsDecrqssUnknownSt |
        0x1b @dcsDecrqssEscape |
        0x7f |
        dcsPayloadC0 |
        0x20..0x7e @dcsDecrqssInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsDecrqssInvalid
    )*;

    dcsDecrqssEscape := (
        cancel |
        stringC1 |
        0x9c @dcsDecrqssUnknownSt |
        '\\' @dcsDecrqssUnknownSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    dcsXtgettcap := (
        cancel |
        stringC1 |
        0x9c @dcsXtSt @dcsXtField @dcsXtDone |
        0x1b @dcsXtEscape |
        0x7f |
        dcsPayloadC0 |
        xdigit @dcsXtHex |
        ';' @{ parser.dcsCapabilityComplete = true; } @dcsXtField @dcsXtSeparator |
        (0x20..0x7e - (xdigit | ';')) @dcsXtInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsXtInvalid
    )*;

    dcsXtEscape := (
        cancel |
        stringC1 |
        0x9c @dcsXtSt @dcsXtField @dcsXtDone |
        '\\' @dcsXtSt @dcsXtField @dcsXtDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    dcsUdkCode := (
        cancel |
        stringC1 |
        0x9c @dcsUdkSt |
        0x1b @dcsUdkEscape |
        0x7f |
        dcsPayloadC0 |
        digit @dcsUdkDigit |
        '/' @dcsUdkSlash |
        ';' @dcsUdkCodeSeparator |
        (0x20..0x7e - (digit | '/' | ';')) @dcsUdkInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsUdkInvalid
    )*;

    dcsUdkValue := (
        cancel |
        stringC1 |
        0x9c @dcsUdkSt |
        0x1b @dcsUdkEscape |
        0x7f |
        dcsPayloadC0 |
        xdigit @dcsUdkHex |
        ';' @dcsUdkValueSeparator |
        (0x20..0x7e - (xdigit | ';')) @dcsUdkInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsUdkInvalid
    )*;

    dcsUdkInvalid := (
        cancel |
        stringC1 |
        0x9c @dcsUdkSt |
        0x1b @dcsUdkEscape |
        0x7f |
        dcsPayloadC0 |
        ';' @dcsUdkInvalidSeparator |
        (0x20..0x7e - ';') @dcsUdkInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsUdkInvalid
    )*;

    dcsUdkEscape := (
        cancel |
        stringC1 |
        0x9c @dcsUdkSt |
        '\\' @dcsUdkSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    dcsPayload := (
        cancel |
        stringC1 |
        0x9c @dcsSt |
        0x1b @dcsPayloadEscape |
        0x7f |
        (0x00..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @dcsPayloadData
    )*;

    dcsPayloadEscape := (
        cancel |
        stringC1 |
        0x9c @{ ragelFinishDcs(); fnext main; fbreak; } |
        '\\' @{ ragelFinishDcs(); fnext main; fbreak; } |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    dcsColor := (
        cancel |
        stringC1 |
        0x9c @dcsColorSt |
        0x1b @dcsColorEscape |
        0x7f |
        digit @dcsColorDigit |
        ';' @dcsColorSeparator |
        '/' @dcsColorDefinition |
        (any - (0x18 | 0x1a | 0x1b | 0x3b | 0x2f | 0x30..0x39 |
                0x7f | 0x90 | 0x96..0x98 | 0x9a..0x9f)) @dcsColorInvalid
    )*;

    dcsColorEscape := (
        cancel |
        stringC1 |
        0x9c @dcsColorSt |
        '\\' @dcsColorSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    dcsTabs := (
        cancel |
        stringC1 |
        0x9c @dcsTabSt |
        0x1b @dcsTabEscape |
        0x7f |
        digit @dcsTabDigit |
        '/' @dcsTabSeparator |
        (any - (0x18 | 0x1a | 0x1b | 0x2f | 0x30..0x39 |
                0x7f | 0x90 | 0x96..0x98 | 0x9a..0x9f)) @dcsTabInvalid
    )*;

    dcsTabEscape := (
        cancel |
        stringC1 |
        0x9c @dcsTabSt |
        '\\' @dcsTabSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    sixelGround := (
        cancel |
        stringC1 |
        0x9c @sixelSt |
        0x1b @sixelEscapeBegin |
        0x7f |
        0x3f..0x7e @sixelPaintOne |
        '!' @sixelRepeatIntro |
        '#' @sixelColorIntro |
        '"' @sixelRasterIntro |
        '$' @sixelCr |
        '-' @sixelLf |
        (any - (0x18 | 0x1a | 0x1b | 0x21..0x24 | 0x2d | 0x3f..0x7f |
                0x90 | 0x96..0x98 | 0x9a..0x9f)) @sixelIgnoredByte
    )*;

    sixelRepeat := (
        cancel |
        stringC1 |
        0x9c @sixelSt |
        0x1b @sixelEscapeBegin |
        0x7f |
        digit @sixelParameterDigit |
        0x3f..0x7e @sixelPaintRepeated |
        (any - (0x18 | 0x1a | 0x1b | 0x30..0x39 | 0x3f..0x7f |
                0x90 | 0x96..0x98 | 0x9a..0x9f)) @sixelParamsAbandon
    )*;

    sixelColor := (
        cancel |
        stringC1 |
        0x9c @sixelColorSt |
        0x1b @sixelColorEscapeBegin |
        0x7f |
        digit @sixelParameterDigit |
        ';' @sixelParameterSeparator |
        (any - (0x18 | 0x1a | 0x1b | 0x30..0x39 | 0x3b | 0x7f |
                0x90 | 0x96..0x98 | 0x9a..0x9f)) @sixelColorDone
    )*;

    sixelRaster := (
        cancel |
        stringC1 |
        0x9c @sixelRasterSt |
        0x1b @sixelRasterEscapeBegin |
        0x7f |
        digit @sixelParameterDigit |
        ';' @sixelParameterSeparator |
        (any - (0x18 | 0x1a | 0x1b | 0x30..0x39 | 0x3b | 0x7f |
                0x90 | 0x96..0x98 | 0x9a..0x9f)) @sixelRasterDone
    )*;

    sixelEscape := (
        cancel |
        stringC1 |
        0x9c @sixelSt |
        '\\' @sixelSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    dcsCursorNumber =
        digit+ >dcsCursorNumberStart $dcsCursorDigit %dcsCursorNumberDone;

    dcsCursorCharset = (
        (['%"&'] $dcsCursorCharsetByte)
        (0x30..0x7e $dcsCursorCharsetByte) |
        ((0x30..0x7e - ['%"&']) $dcsCursorCharsetByte)
    ) >dcsCursorCharsetStart %dcsCursorCharsetDone;

    dcsCursorPayload = (
        dcsCursorNumber (';' @dcsCursorSeparator)
        dcsCursorNumber (';' @dcsCursorSeparator)
        dcsCursorNumber (';' @dcsCursorSeparator)
        (0x40..0x5f @dcsCursorByte) (';' @dcsCursorSeparator)
        (0x40..0x5f @dcsCursorByte) (';' @dcsCursorSeparator)
        (0x40..0x5f @dcsCursorByte) (';' @dcsCursorSeparator)
        dcsCursorNumber (';' @dcsCursorSeparator)
        dcsCursorNumber (';' @dcsCursorSeparator)
        (0x40..0x4f @dcsCursorByte) (';' @dcsCursorSeparator)
        dcsCursorCharset{4}
    );

    dcsCursor := (
        cancel |
        stringC1 |
        0x9c @dcsCursorSt |
        0x1b @dcsCursorDoneEscape |
        dcsCursorPayload
    )* $err(dcsCursorInvalid);

    dcsCursorDoneEscape := (
        cancel |
        stringC1 |
        '\\' @dcsCursorSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9b | 0x9d..0x9f)) @abortStringEscaped
    )*;

    dcsUpssId = (
        (['%"&'] $dcsUpssByte)
        (0x30..0x7e $dcsUpssByte) |
        ((0x30..0x7e - ['%"&']) $dcsUpssByte)
    ) >dcsUpssStart %dcsUpssComplete;

    dcsUpss := (
        cancel |
        stringC1 |
        0x9c @dcsUpssSt |
        0x1b @dcsUpssEscape |
        dcsUpssId
    )* $err(dcsUpssInvalid);

    dcsUpssEscape := (
        cancel |
        stringC1 |
        '\\' @dcsUpssSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9b | 0x9d..0x9f)) @abortStringEscaped
    )*;

    dcsIgnore := (
        cancel |
        stringC1 |
        0x9c @dcsIgnoreSt |
        0x1b @{ fgoto dcsIgnoreEscape; } |
        (0x00..0x17 | 0x19 | 0x1c..0x8f |
         0x91..0x95 | 0x99 | 0xa0..0xff) @dcsIgnoreData
    )*;

    dcsIgnoreEscape := (
        cancel |
        stringC1 |
        0x9c @dcsIgnoreSt |
        '\\' @dcsIgnoreSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\' | 0x90 | 0x96..0x98 |
                0x9a..0x9f)) @abortStringEscaped
    )*;

    oscCommand := (
        cancel |
        stringC1 |
        0x9c @oscCommandSt @oscDispatch @oscDone |
        0x07 @oscCommandBell @oscDispatch @oscDone |
        0x1b @{ fgoto oscCommandEscape; } |
        0x7f |
        sequenceC0 |
        digit @oscCommandDigit |
        ';' @oscCommandSeparator |
        (0x20..0x7e - (digit | ';')) @oscCommandInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscCommandInvalid
    )*;

    oscCommandEscape := (
        cancel |
        '\\' @oscCommandSt @oscDispatch @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscCwdEntry := (
        cancel |
        stringC1 |
        0x9c @oscCwdSt @oscCwdDispatch @oscDone |
        0x07 @oscCwdBell @oscCwdDispatch @oscDone |
        0x1b @{ fgoto oscCwdInvalidEscape; } |
        0x7f |
        sequenceC0 |
        '/' @oscCwdPathStart |
        'f' @oscCwdRaw @{ fgoto oscCwdFile; } |
        (0x20..0x7e - ('/' | 'f')) @oscCwdInvalidData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscCwdInvalidData
    )*;

    oscCwdPrefixGap = (
        cancel |
        stringC1 |
        0x9c @oscCwdSt @oscCwdDispatch @oscDone |
        0x07 @oscCwdBell @oscCwdDispatch @oscDone |
        0x1b @{ fgoto oscCwdInvalidEscape; } |
        0x7f |
        sequenceC0
    )*;

    oscCwdFile := (
        oscCwdPrefixGap
        'i' @oscCwdRaw
        oscCwdPrefixGap
        'l' @oscCwdRaw
        oscCwdPrefixGap
        'e' @oscCwdRaw
        oscCwdPrefixGap
        ':' @oscCwdRaw
        oscCwdPrefixGap
        '/' @oscCwdRaw
        oscCwdPrefixGap
        '/' @oscCwdRaw
        @{ fgoto oscCwdAuthority; }
    ) $err(oscCwdPrefixError);

    oscCwdAuthority := (
        cancel |
        stringC1 |
        0x9c @oscCwdSt @oscCwdDispatch @oscDone |
        0x07 @oscCwdBell @oscCwdDispatch @oscDone |
        0x1b @{ fgoto oscCwdAuthorityEscape; } |
        0x7f |
        sequenceC0 |
        '/' @oscCwdPathStart |
        (0x20..0x7e - '/') @oscCwdAuthorityData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscCwdAuthorityData
    )*;

    oscCwdPath := (
        cancel |
        stringC1 |
        0x9c @oscCwdSt @oscCwdDispatch @oscDone |
        0x07 @oscCwdBell @oscCwdDispatch @oscDone |
        0x1b @{ fgoto oscCwdPathEscape; } |
        0x7f |
        sequenceC0 |
        '%' @oscCwdPercentStart |
        (0x20..0x7e - '%') @oscCwdPathData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscCwdPathData
    )*;

    oscCwdPercentHigh := (
        cancel |
        stringC1 |
        0x9c @oscCwdSt @oscCwdDispatch @oscDone |
        0x07 @oscCwdBell @oscCwdDispatch @oscDone |
        0x1b @{ fgoto oscCwdInvalidEscape; } |
        0x7f |
        sequenceC0 |
        xdigit @oscCwdPercentHigh |
        (0x20..0x7e - xdigit) @oscCwdInvalidData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscCwdInvalidData
    )*;

    oscCwdPercentLow := (
        cancel |
        stringC1 |
        0x9c @oscCwdSt @oscCwdDispatch @oscDone |
        0x07 @oscCwdBell @oscCwdDispatch @oscDone |
        0x1b @{ fgoto oscCwdInvalidEscape; } |
        0x7f |
        sequenceC0 |
        xdigit @oscCwdPercentLow |
        (0x20..0x7e - xdigit) @oscCwdInvalidData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscCwdInvalidData
    )*;

    oscCwdInvalid := (
        cancel |
        stringC1 |
        0x9c @oscCwdSt @oscCwdDispatch @oscDone |
        0x07 @oscCwdBell @oscCwdDispatch @oscDone |
        0x1b @{ fgoto oscCwdInvalidEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscCwdInvalidEscape := (
        cancel |
        '\\' @oscCwdSt @oscCwdDispatch @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscCwdAuthorityEscape := (
        cancel |
        '\\' @oscCwdSt @oscCwdDispatch @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscCwdPathEscape := (
        cancel |
        '\\' @oscCwdSt @oscCwdDispatch @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscTitle0 := (
        cancel |
        stringC1 |
        0x9c @oscTitle0St |
        0x07 @oscTitle0St |
        0x1b @{ fgoto oscTitle0Escape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscTitleData
    )*;

    oscTitle1 := (
        cancel |
        stringC1 |
        0x9c @oscTitle1St |
        0x07 @oscTitle1St |
        0x1b @{ fgoto oscTitle1Escape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscTitleData
    )*;

    oscTitle2 := (
        cancel |
        stringC1 |
        0x9c @oscTitle2St |
        0x07 @oscTitle2St |
        0x1b @{ fgoto oscTitle2Escape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscTitleData
    )*;

    oscTitle0Escape := (
        cancel |
        '\\' @oscTitle0St |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscTitle1Escape := (
        cancel |
        '\\' @oscTitle1St |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscTitle2Escape := (
        cancel |
        '\\' @oscTitle2St |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscHyperlinkParamStart := (
        cancel |
        stringC1 |
        0x9c @oscHyperlinkMalformedSt |
        0x07 @oscHyperlinkMalformedSt |
        0x1b @{ fgoto oscHyperlinkParamEscape; } |
        0x7f |
        sequenceC0 |
        'i' @oscHyperlinkI |
        ':' @oscHyperlinkColon |
        ';' @oscHyperlinkUri |
        (0x20..0x7e - ('i' | ':' | ';')) @oscHyperlinkParamData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscHyperlinkParamData
    )*;

    oscHyperlinkParamI := (
        cancel |
        stringC1 |
        0x9c @oscHyperlinkMalformedSt |
        0x07 @oscHyperlinkMalformedSt |
        0x1b @{ fgoto oscHyperlinkParamEscape; } |
        0x7f |
        sequenceC0 |
        'd' @oscHyperlinkD |
        ':' @oscHyperlinkColon |
        ';' @oscHyperlinkUri |
        (0x20..0x7e - ('d' | ':' | ';')) @oscHyperlinkParamData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscHyperlinkParamData
    )*;

    oscHyperlinkParamId := (
        cancel |
        stringC1 |
        0x9c @oscHyperlinkMalformedSt |
        0x07 @oscHyperlinkMalformedSt |
        0x1b @{ fgoto oscHyperlinkParamEscape; } |
        0x7f |
        sequenceC0 |
        '=' @oscHyperlinkEqual |
        ':' @oscHyperlinkColon |
        ';' @oscHyperlinkUri |
        (0x20..0x7e - ('=' | ':' | ';')) @oscHyperlinkParamData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscHyperlinkParamData
    )*;

    oscHyperlinkIdValue := (
        cancel |
        stringC1 |
        0x9c @oscHyperlinkMalformedSt |
        0x07 @oscHyperlinkMalformedSt |
        0x1b @{ fgoto oscHyperlinkIdEscape; } |
        0x7f |
        ':' @oscHyperlinkIdColon |
        ';' @oscHyperlinkIdUri |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscData
    )*;

    oscHyperlinkParamSkip := (
        cancel |
        stringC1 |
        0x9c @oscHyperlinkMalformedSt |
        0x07 @oscHyperlinkMalformedSt |
        0x1b @{ fgoto oscHyperlinkParamEscape; } |
        0x7f |
        ':' @oscHyperlinkColon |
        ';' @oscHyperlinkUri |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscData
    )*;

    oscHyperlinkUri := (
        cancel |
        stringC1 |
        0x9c @oscHyperlinkSt |
        0x07 @oscHyperlinkSt |
        0x1b @{ fgoto oscHyperlinkUriEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscHyperlinkParamEscape := (
        cancel |
        '\\' @oscHyperlinkMalformedSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscHyperlinkIdEscape := (
        cancel |
        '\\' @oscHyperlinkMalformedSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscHyperlinkUriEscape := (
        cancel |
        '\\' @oscHyperlinkSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscProgressEntry := (
        cancel |
        stringC1 |
        0x9c @oscProgressNotifySt |
        0x07 @oscProgressNotifySt |
        0x1b @{ fgoto oscProgressEntryEscape; } |
        0x7f |
        sequenceC0 |
        '4' @oscProgressFour |
        (0x20..0x7e - '4') @oscProgressNotifyData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscProgressNotifyData
    )*;

    oscProgressFour := (
        cancel |
        stringC1 |
        0x9c @oscProgressNotifySt |
        0x07 @oscProgressNotifySt |
        0x1b @{ fgoto oscProgressFourEscape; } |
        0x7f |
        sequenceC0 |
        ';' @oscProgressBeginState |
        (0x20..0x7e - ';') @oscProgressNotifyData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscProgressNotifyData
    )*;

    oscProgressState := (
        cancel |
        stringC1 |
        0x9c @oscProgressSt |
        0x07 @oscProgressSt |
        0x1b @{ fgoto oscProgressPercentEscape; } |
        0x7f |
        sequenceC0 |
        digit @oscProgressStateDigit |
        ';' @oscProgressBeginPercent |
        (0x20..0x7e - (digit | ';')) @oscProgressDiscardData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscProgressDiscardData
    )*;

    oscProgressPercent := (
        cancel |
        stringC1 |
        0x9c @oscProgressSt |
        0x07 @oscProgressSt |
        0x1b @{ fgoto oscProgressPercentEscape; } |
        0x7f |
        sequenceC0 |
        digit @oscProgressPercentDigit |
        (0x20..0x7e - digit) @oscProgressDiscardData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscProgressDiscardData
    )*;

    oscProgressNotify := (
        cancel |
        stringC1 |
        0x9c @oscProgressNotifySt |
        0x07 @oscProgressNotifySt |
        0x1b @{ fgoto oscProgressNotifyEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscProgressDiscard := (
        cancel |
        stringC1 |
        0x9c @oscProgressDiscardSt |
        0x07 @oscProgressDiscardSt |
        0x1b @{ fgoto oscProgressDiscardEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscProgressEntryEscape := (
        cancel |
        '\\' @oscProgressNotifySt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscProgressFourEscape := (
        cancel |
        '\\' @oscProgressNotifySt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscProgressNotifyEscape := (
        cancel |
        '\\' @oscProgressNotifySt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscProgressDiscardEscape := (
        cancel |
        '\\' @oscProgressDiscardSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscProgressPercentEscape := (
        cancel |
        '\\' @oscProgressSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    osc52Selectors := (
        cancel |
        stringC1 |
        0x9c @osc52MalformedSt |
        0x07 @osc52MalformedBell |
        0x1b @{ fgoto osc52SelectorsEscape; } |
        0x7f |
        sequenceC0 |
        ';' @osc52BeginPayload |
        (0x20..0x7e - ';') @osc52Selector |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @osc52Selector
    )*;

    osc52Payload := (
        cancel |
        stringC1 |
        0x9c @osc52St @osc52Dispatch @oscDone |
        0x07 @osc52Bell @osc52Dispatch @oscDone |
        0x1b @{ fgoto osc52PayloadEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @osc52Data
    )*;

    osc52SelectorsEscape := (
        cancel |
        '\\' @osc52MalformedSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    osc52PayloadEscape := (
        cancel |
        '\\' @osc52St @osc52Dispatch @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscNotificationField := (
        cancel |
        stringC1 |
        0x9c @oscNotificationInvalidSt |
        0x07 @oscNotificationInvalidBell |
        0x1b @{ fgoto oscNotificationInvalidEscape; } |
        0x7f |
        sequenceC0 |
        [A-Za-z] @oscNotificationKey |
        ';' @oscNotificationBeginPayload |
        (0x20..0x7e - ([A-Za-z] | ';')) @oscNotificationInvalidData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscNotificationInvalidData
    )*;

    oscNotificationEqual := (
        cancel |
        stringC1 |
        0x9c @oscNotificationInvalidSt |
        0x07 @oscNotificationInvalidBell |
        0x1b @{ fgoto oscNotificationInvalidEscape; } |
        0x7f |
        sequenceC0 |
        '=' @oscNotificationBeginValue |
        (0x20..0x7e - '=') @oscNotificationInvalidData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscNotificationInvalidData
    )*;

    oscNotificationValue := (
        cancel |
        stringC1 |
        0x9c @oscNotificationInvalidSt |
        0x07 @oscNotificationInvalidBell |
        0x1b @{ fgoto oscNotificationInvalidEscape; } |
        0x7f |
        ':' @oscNotificationFinishField @oscNotificationNextField |
        ';' @oscNotificationFinishField @oscNotificationBeginPayload |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e -
         (':' | ';')) @oscNotificationValueData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscNotificationValueData
    )*;

    oscNotificationPayload := (
        cancel |
        stringC1 |
        0x9c @oscNotificationSt @oscNotificationDispatch @oscDone |
        0x07 @oscNotificationBell @oscNotificationDispatch @oscDone |
        0x1b @{ fgoto oscNotificationPayloadEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscNotificationPayloadData
    )*;

    oscNotificationInvalid := (
        cancel |
        stringC1 |
        0x9c @oscNotificationInvalidSt |
        0x07 @oscNotificationInvalidBell |
        0x1b @{ fgoto oscNotificationInvalidEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscNotificationPayloadEscape := (
        cancel |
        '\\' @oscNotificationSt @oscNotificationDispatch @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscNotificationInvalidEscape := (
        cancel |
        '\\' @oscNotificationInvalidSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscIndexedColorIndex := (
        cancel |
        stringC1 |
        0x9c @oscInvalidSt |
        0x07 @oscInvalidBell |
        0x1b @{ fgoto oscIndexedColorIndexEscape; } |
        0x7f |
        sequenceC0 |
        digit @oscIndexedColorIndexDigit |
        ';' @oscIndexedColorBegin |
        (0x20..0x7e - (digit | ';')) @oscIndexedColorIndexInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscIndexedColorIndexInvalid
    )*;

    oscIndexedColorIndexEscape := (
        cancel |
        '\\' @oscInvalidSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscIndexedColor := (
        oscColorValue (
            ';' @oscIndexedColorCommit @oscIndexedColorNext |
            0x9c @oscIndexedColorSt @oscIndexedColorCommit @oscDone |
            0x07 @oscIndexedColorBell @oscIndexedColorCommit @oscDone |
            0x1b @{ fgoto oscIndexedColorEscape; }
        )
    ) $err(oscIndexedColorInvalid);

    oscIndexedColorEscape := (
        cancel |
        '\\' @oscIndexedColorSt @oscIndexedColorCommit @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscIndexedColorDiscard := (
        cancel |
        stringC1 |
        0x9c @oscInvalidSt |
        0x07 @oscInvalidBell |
        0x1b @{ fgoto oscIndexedColorDiscardEscape; } |
        0x7f |
        sequenceC0 |
        ';' @oscIndexedColorDiscardNext |
        (0x20..0x7e - ';') @oscData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscData
    )*;

    oscIndexedColorDiscardEscape := (
        cancel |
        '\\' @oscInvalidSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscDynamicColor := (
        oscColorValue (
            ';' @oscDynamicColorCommit @oscDynamicColorNext |
            0x9c @oscDynamicColorSt @oscDynamicColorCommit @oscDone |
            0x07 @oscDynamicColorBell @oscDynamicColorCommit @oscDone |
            0x1b @{ fgoto oscDynamicColorEscape; }
        )
    ) $err(oscDynamicColorInvalid);

    oscDynamicColorEscape := (
        cancel |
        '\\' @oscDynamicColorSt @oscDynamicColorCommit @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscDynamicColorDiscard := (
        cancel |
        stringC1 |
        0x9c @oscInvalidSt |
        0x07 @oscInvalidBell |
        0x1b @{ fgoto oscDynamicColorDiscardEscape; } |
        0x7f |
        sequenceC0 |
        ';' @oscDynamicColorDiscardNext |
        (0x20..0x7e - ';') @oscData |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscData
    )*;

    oscDynamicColorDiscardEscape := (
        cancel |
        '\\' @oscInvalidSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscNumericFields := (
        cancel |
        stringC1 |
        0x9c @oscNumericSt @oscNumericFinalField @oscDone |
        0x07 @oscNumericFinalField @oscNumericBell @oscDone |
        0x1b @{ fgoto oscNumericEscape; } |
        0x7f |
        sequenceC0 |
        digit @oscNumericDigit |
        ';' @oscNumericField @oscNumericSeparator |
        (0x20..0x7e - (digit | ';')) @oscNumericInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff)
            @oscNumericInvalid
    )*;

    oscNumericEscape := (
        cancel |
        '\\' @oscNumericFinalField @oscNumericSt @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellEntry := (
        cancel |
        stringC1 |
        0x9c @oscShellUnknownSt |
        0x07 @oscShellUnknownSt |
        0x1b @{ fgoto oscShellUnknownEscape; } |
        0x7f |
        sequenceC0 |
        'A' @oscShellA |
        'B' @oscShellB |
        'C' @oscShellC |
        'D' @oscShellD |
        'I' @oscShellI |
        'L' @oscShellL |
        'N' @oscShellN |
        'P' @oscShellP |
        (0x20..0x7e - ('A' | 'B' | 'C' | 'D' | 'I' | 'L' | 'N' | 'P')) @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellAComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellASt |
        0x07 @oscShellASt |
        0x1b @{ fgoto oscShellACompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellATail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellBComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellBSt |
        0x07 @oscShellBSt |
        0x1b @{ fgoto oscShellBCompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellBTail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellCComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellCSt |
        0x07 @oscShellCSt |
        0x1b @{ fgoto oscShellCCompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellCTail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellDComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellDSt |
        0x07 @oscShellDSt |
        0x1b @{ fgoto oscShellDCompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellDTail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellIComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellISt |
        0x07 @oscShellISt |
        0x1b @{ fgoto oscShellICompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellITail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellLComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellLSt |
        0x07 @oscShellLSt |
        0x1b @{ fgoto oscShellLCompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellLTail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellNComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellNSt |
        0x07 @oscShellNSt |
        0x1b @{ fgoto oscShellNCompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellNTail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellPComplete := (
        cancel |
        stringC1 |
        0x9c @oscShellPSt |
        0x07 @oscShellPSt |
        0x1b @{ fgoto oscShellPCompleteEscape; } |
        0x7f |
        sequenceC0 |
        ';' @{ ragelAppendString(fc, parser.maxOscBytes); fgoto oscShellPTail; } |
        (0x20..0x7e - ';') @oscShellInvalid |
        (0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscShellInvalid
    )*;

    oscShellATail := (
        cancel |
        stringC1 |
        0x9c @oscShellASt |
        0x07 @oscShellASt |
        0x1b @{ fgoto oscShellATailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellBTail := (
        cancel |
        stringC1 |
        0x9c @oscShellBSt |
        0x07 @oscShellBSt |
        0x1b @{ fgoto oscShellBTailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellCTail := (
        cancel |
        stringC1 |
        0x9c @oscShellCSt |
        0x07 @oscShellCSt |
        0x1b @{ fgoto oscShellCTailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellDTail := (
        cancel |
        stringC1 |
        0x9c @oscShellDSt |
        0x07 @oscShellDSt |
        0x1b @{ fgoto oscShellDTailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellITail := (
        cancel |
        stringC1 |
        0x9c @oscShellISt |
        0x07 @oscShellISt |
        0x1b @{ fgoto oscShellITailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellLTail := (
        cancel |
        stringC1 |
        0x9c @oscShellLSt |
        0x07 @oscShellLSt |
        0x1b @{ fgoto oscShellLTailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellNTail := (
        cancel |
        stringC1 |
        0x9c @oscShellNSt |
        0x07 @oscShellNSt |
        0x1b @{ fgoto oscShellNTailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellPTail := (
        cancel |
        stringC1 |
        0x9c @oscShellPSt |
        0x07 @oscShellPSt |
        0x1b @{ fgoto oscShellPTailEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellUnknown := (
        cancel |
        stringC1 |
        0x9c @oscShellUnknownSt |
        0x07 @oscShellUnknownSt |
        0x1b @{ fgoto oscShellUnknownEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscBulkData
    )*;

    oscShellACompleteEscape := (
        cancel |
        '\\' @oscShellASt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellBCompleteEscape := (
        cancel |
        '\\' @oscShellBSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellCCompleteEscape := (
        cancel |
        '\\' @oscShellCSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellDCompleteEscape := (
        cancel |
        '\\' @oscShellDSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellICompleteEscape := (
        cancel |
        '\\' @oscShellISt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellLCompleteEscape := (
        cancel |
        '\\' @oscShellLSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellNCompleteEscape := (
        cancel |
        '\\' @oscShellNSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellPCompleteEscape := (
        cancel |
        '\\' @oscShellPSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellATailEscape := (
        cancel |
        '\\' @oscShellASt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellBTailEscape := (
        cancel |
        '\\' @oscShellBSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellCTailEscape := (
        cancel |
        '\\' @oscShellCSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellDTailEscape := (
        cancel |
        '\\' @oscShellDSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellITailEscape := (
        cancel |
        '\\' @oscShellISt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellLTailEscape := (
        cancel |
        '\\' @oscShellLSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellNTailEscape := (
        cancel |
        '\\' @oscShellNSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellPTailEscape := (
        cancel |
        '\\' @oscShellPSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscShellUnknownEscape := (
        cancel |
        '\\' @oscShellUnknownSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscPayload := (
        cancel |
        stringC1 |
        0x9c @oscSt @oscDispatch @oscDone |
        0x07 @oscBell @oscDispatch @oscDone |
        0x1b @oscEscape |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscRawData
    )*;

    oscEscape := (
        cancel |
        '\\' @oscSt @oscDispatch @oscDone |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    oscInvalid := (
        cancel |
        stringC1 |
        0x9c @oscInvalidSt |
        0x07 @oscInvalidBell |
        0x1b @{ fgoto oscInvalidEscape; } |
        0x7f |
        (0x00..0x06 | 0x08..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @oscRawData
    )*;

    oscInvalidEscape := (
        cancel |
        '\\' @oscInvalidSt |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

    string := (
        cancel |
        stringC1 |
        0x9c @ignoredSt |
        0x1b @ignoredEscape |
        0x7f |
        (0x00..0x17 | 0x19 | 0x1c..0x7e |
         0x80..0x8f | 0x91..0x95 | 0x99 | 0xa0..0xff) @ignoredData
    )*;

    stringEscape := (
        cancel |
        '\\' @{ parser.stringUtf8Remaining = 0; parser.stringLimit = 0; if constexpr (traced) { parserTrace->stringEnd(); } fnext main; fbreak; } |
        restartEscape |
        (any - (0x18 | 0x1a | 0x1b | '\\')) @abortStringEscaped
    )*;

}%%

#if defined(SHITTY_PARSER_DATA)
%% write data;
#elif defined(SHITTY_PARSER_INIT)
%% write init;
#elif defined(SHITTY_PARSER_EXEC)
%% write exec;
#endif
