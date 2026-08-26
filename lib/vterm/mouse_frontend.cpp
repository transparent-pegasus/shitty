/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "mouse_frontend.h"

#include <std/alg/minmax.h>

#include <math.h>
#include <limits.h>

using namespace stl;

int mouseFramebufferCoordinate(double logical, double scale) {
    if (!isfinite(logical) || !isfinite(scale)) {
        return 0;
    }
    const double pixel = logical * max(1.0, scale);
    return (int)(min(max(round(pixel), (double)(INT_MIN)), (double)(INT_MAX)));
}

MouseProtocolPoint mouseProtocolPoint(MouseTrackingEnc encoding, int pixelX, int pixelY, const MouseGeometry& geometry) {
    const int contentWidth = max(1, geometry.framebufferWidth - 2 * geometry.border);
    const int contentHeight = max(1, geometry.framebufferHeight - 2 * geometry.border);
    if (encoding == MouseTrackingEnc::SGRPixels) {
        return {
            min(max(pixelX - geometry.border + 1, 1), contentWidth),
            min(max(pixelY - geometry.border + 1, 1), contentHeight),
        };
    }
    const int columns = max(1, contentWidth / max(1, geometry.glyphWidth));
    const int rows = max(1, contentHeight / max(1, geometry.glyphHeight));
    return {
        min(max((pixelX - geometry.border) / max(1, geometry.glyphWidth) + 1, 1), columns),
        min(max((pixelY - geometry.border) / max(1, geometry.glyphHeight) + 1, 1), rows),
    };
}

unsigned mouseProtocolModifiers(unsigned modifiers, bool reportAlt) {
    unsigned result = 0;
    if (modifiers & FrontendShift) {
        result |= MouseShift;
    }
    if (reportAlt && (modifiers & FrontendAlt)) {
        result |= MouseAlt;
    }
    if (modifiers & FrontendControl) {
        result |= MouseControl;
    }
    return result;
}

int mouseTerminalButton(int button) {
    switch (button) {
        case 0:
            return 1;
        case 2:
            return 2;
        case 1:
            return 3;
        default:
            return button >= 3 ? button - 3 + 8 : 0;
    }
}

bool mouseButtonReportAllowed(MouseTrackingMode mode, MouseEventType type, int button) {
    return button >= 1 && button <= 11 && !(type == MouseEventType::Release && button > 3) && mode != MouseTrackingMode::Disabled && !(type == MouseEventType::Release && mode == MouseTrackingMode::X10_Compat);
}

int MouseWheelAccumulator::consumeAxis(double delta, double& remainder) {
    if (!isfinite(delta)) {
        remainder = 0.0;
        return 0;
    }
    const double total = remainder + min(max(delta, -100.0), 100.0);
    const int steps = (int)(trunc(total));
    remainder = total - steps;
    return steps;
}

MouseWheelSteps MouseWheelAccumulator::consume(double x, double y, bool reporting) {
    if (reporting != reporting_) {
        reporting_ = reporting;
        remainderX_ = 0.0;
        remainderY_ = 0.0;
    }

    MouseWheelSteps steps;
    steps.y = consumeAxis(y, remainderY_);
    if (reporting) {
        steps.x = consumeAxis(x, remainderX_);
    } else {
        remainderX_ = 0.0;
    }
    return steps;
}

void MouseWheelAccumulator::reset() {
    reporting_ = false;
    remainderX_ = 0.0;
    remainderY_ = 0.0;
}

bool MouseFrontendState::protocolActive(unsigned modifiers, MouseTrackingMode mode) const {
    return !selectionOngoing_ && !(modifiers & FrontendShift) && mode != MouseTrackingMode::Disabled;
}

void MouseFrontendState::updateButton(int button, bool pressed) {
    if (button < 0 || button >= (int)(sizeof(buttons_) * CHAR_BIT)) {
        return;
    }
    const unsigned mask = 1u << button;
    if (pressed) {
        buttons_ |= mask;
    } else {
        buttons_ &= ~mask;
    }
}

int MouseFrontendState::motionButton() const {
    if (buttons_ & (1u << 0)) {
        return 1;
    }
    if (buttons_ & (1u << 2)) {
        return 2;
    }
    if (buttons_ & (1u << 1)) {
        return 3;
    }
    return 0;
}

bool MouseFrontendState::primaryButtonPressed() const {
    return buttons_ & ((1u << 0) | (1u << 1) | (1u << 2));
}

int MouseFrontendState::registerClick(int button, double x, double y, double time) {
    const double elapsed = time - lastClickTime_;
    const bool repeated = button == lastButton_ && elapsed >= 0.0 && elapsed <= 0.5 && fabs(x - lastClickX_) <= 4.0 && fabs(y - lastClickY_) <= 4.0;
    clickCount_ = repeated ? clickCount_ + 1 : 1;
    lastButton_ = button;
    lastClickTime_ = time;
    lastClickX_ = x;
    lastClickY_ = y;
    return clickCount_;
}

bool MouseFrontendState::reportMotion(int column, int row, MouseTrackingMode mode, MouseTrackingEnc encoding, u32 generation) {
    if (!hasMotionContext_ || mode != lastMotionMode_ || encoding != lastMotionEncoding_ || generation != lastMotionGeneration_) {
        hasMotionContext_ = true;
        lastMotionMode_ = mode;
        lastMotionEncoding_ = encoding;
        lastMotionGeneration_ = generation;
        hasReportedMotion_ = false;
    }
    if (hasReportedMotion_ && column == lastReportColumn_ && row == lastReportRow_) {
        return false;
    }
    hasReportedMotion_ = true;
    lastReportColumn_ = column;
    lastReportRow_ = row;
    return true;
}

void MouseFrontendState::resetMotion() {
    hasReportedMotion_ = false;
    hasMotionContext_ = false;
}
