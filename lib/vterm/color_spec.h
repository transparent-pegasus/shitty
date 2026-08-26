/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include "color.h"

namespace stl {
    class StringView;
}

bool colorFromRgbIntensity(double red, double green, double blue, Color& color);
bool colorFromCieXyz(double x, double y, double z, Color& color);
bool colorFromCieUvY(double u, double v, double y, Color& color);
bool colorFromCieXyY(double x, double y, double luminance, Color& color);
bool colorFromCieLab(double lightness, double a, double b, Color& color);
bool colorFromCieLuv(double lightness, double u, double v, Color& color);
bool colorFromTekHvc(double hue, double value, double chroma, Color& color);
bool finishColorNumber(double mantissa, bool negative, u32 exponent, bool exponentNegative, double& value);
