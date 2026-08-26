/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "color_spec.h"

#include <std/alg/minmax.h>

#include <math.h>

using namespace stl;

namespace {
    struct Triple {
        double x;
        double y;
        double z;
    };

    static constexpr double whiteX = 0.95047;
    static constexpr double whiteY = 1.0;
    static constexpr double whiteZ = 1.08883;
    static constexpr double chromaScale = 7.50725;
    static constexpr double bestRedU = 0.7127;
    static constexpr double bestRedV = 0.4931;
    static constexpr double pi = 3.14159265358979323846;

    static Triple whiteUvY() {
        const double divisor = whiteX + 15.0 * whiteY + 3.0 * whiteZ;
        return {4.0 * whiteX / divisor, 9.0 * whiteY / divisor, 1.0};
    }

    static Triple uvYToXyz(Triple value) {
        double divisor = 6.0 * value.x - 16.0 * value.y + 12.0;
        if (divisor == 0.0) {
            value = whiteUvY();
            divisor = 6.0 * value.x - 16.0 * value.y + 12.0;
        }
        const double x = 9.0 * value.x / divisor;
        const double y = 4.0 * value.y / divisor;
        const double z = 1.0 - x - y;
        if (y == 0.0) {
            return {x, value.z, z};
        }
        return {x * value.z / y, value.z, z * value.z / y};
    }

    static Triple xyzToUvY(Triple value) {
        const double divisor = value.x + 15.0 * value.y + 3.0 * value.z;
        if (divisor == 0.0) {
            Triple result = whiteUvY();
            result.z = value.y;
            return result;
        }
        return {4.0 * value.x / divisor, 9.0 * value.y / divisor, value.y};
    }

    static double valueToY(double value) {
        if (value < 7.99953624) {
            return value / 903.29;
        }
        const double scaled = (value + 16.0) / 116.0;
        return scaled * scaled * scaled;
    }

    static double yToValue(double value) {
        return value < 0.008856 ? value * 903.29 : cbrt(value) * 116.0 - 16.0;
    }

    static double hueOffset() {
        const Triple white = whiteUvY();
        return atan((bestRedV - white.y) / (bestRedU - white.x)) * 180.0 / pi;
    }

    static Triple tekHvcToXyz(Triple value) {
        if (value.y == 0.0 || value.y == 100.0) {
            Triple neutral = whiteUvY();
            neutral.z = value.y == 0.0 ? 0.0 : 1.0;
            return uvYToXyz(neutral);
        }
        const double hue = (value.x + hueOffset()) * pi / 180.0;
        Triple uvY = whiteUvY();
        uvY.x += cos(hue) * value.z / (value.y * chromaScale);
        uvY.y += sin(hue) * value.z / (value.y * chromaScale);
        uvY.z = valueToY(value.y);
        return uvYToXyz(uvY);
    }

    static Triple xyzToTekHvc(Triple value) {
        const Triple uvY = xyzToUvY(value);
        const Triple white = whiteUvY();
        const double u = uvY.x - white.x;
        const double v = uvY.y - white.y;
        const double lightness = yToValue(value.y);
        double hue = atan2(v, u) * 180.0 / pi - hueOffset();
        while (hue < 0.0) {
            hue += 360.0;
        }
        while (hue >= 360.0) {
            hue -= 360.0;
        }
        return {
            hue,
            lightness,
            lightness * chromaScale * sqrt(u * u + v * v),
        };
    }

    static Triple xyzToLinearRgb(Triple value) {
        return {
            3.2404542 * value.x - 1.5371385 * value.y - 0.4985314 * value.z,
            -0.9692660 * value.x + 1.8760108 * value.y + 0.0415560 * value.z,
            0.0556434 * value.x - 0.2040259 * value.y + 1.0572252 * value.z,
        };
    }

    static bool inGamut(Triple value) {
        constexpr double epsilon = 0.000001;
        return value.x >= -epsilon && value.x <= 1.0 + epsilon && value.y >= -epsilon && value.y <= 1.0 + epsilon && value.z >= -epsilon && value.z <= 1.0 + epsilon;
    }

    static Triple gamutMap(Triple xyz) {
        Triple rgb = xyzToLinearRgb(xyz);
        if (inGamut(rgb)) {
            return rgb;
        }
        Triple hvc = xyzToTekHvc(xyz);
        double lower = 0.0;
        double upper = hvc.z;
        for (unsigned i = 0; i < 32; ++i) {
            hvc.z = (lower + upper) * 0.5;
            const Triple candidate = xyzToLinearRgb(tekHvcToXyz(hvc));
            if (inGamut(candidate)) {
                lower = hvc.z;
                rgb = candidate;
            } else {
                upper = hvc.z;
            }
        }
        return rgb;
    }

    static u8 encodeSrgb(double value) {
        value = min(max(value, 0.0), 1.0);
        value = value <= 0.0031308 ? 12.92 * value : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
        return (u8)lround(value * 255.0);
    }

    static bool colorFromXyz(Triple xyz, Color& color) {
        if (!isfinite(xyz.x) || !isfinite(xyz.y) || !isfinite(xyz.z) || xyz.y < 0.0 || xyz.y > 1.0) {
            return false;
        }
        const Triple rgb = gamutMap(xyz);
        if (!isfinite(rgb.x) || !isfinite(rgb.y) || !isfinite(rgb.z)) {
            return false;
        }
        color = {encodeSrgb(rgb.x), encodeSrgb(rgb.y), encodeSrgb(rgb.z)};
        return true;
    }
}

bool colorFromRgbIntensity(double red, double green, double blue, Color& color) {
    if (red < 0.0 || red > 1.0 || green < 0.0 || green > 1.0 || blue < 0.0 || blue > 1.0) {
        return false;
    }
    color = {encodeSrgb(red), encodeSrgb(green), encodeSrgb(blue)};
    return true;
}

bool colorFromCieXyz(double x, double y, double z, Color& color) {
    if (y < 0.0 || y > 1.0) {
        return false;
    }
    return colorFromXyz({x, y, z}, color);
}

bool colorFromCieUvY(double u, double v, double y, Color& color) {
    if (y < 0.0 || y > 1.0) {
        return false;
    }
    return colorFromXyz(uvYToXyz({u, v, y}), color);
}

bool colorFromCieXyY(double x, double y, double luminance, Color& color) {
    if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0 || luminance < 0.0 || luminance > 1.0) {
        return false;
    }
    if (y == 0.0) {
        return colorFromXyz({}, color);
    }
    return colorFromXyz(
        {
            x * luminance / y,
            luminance,
            (1.0 - x - y) * luminance / y,
        },
        color
    );
}

bool colorFromCieLab(double lightness, double a, double b, Color& color) {
    if (lightness < 0.0 || lightness > 100.0) {
        return false;
    }
    double scaledLightness = (lightness + 16.0) / 116.0;
    Triple xyz{};
    xyz.y = scaledLightness * scaledLightness * scaledLightness;
    if (xyz.y < 0.008856) {
        scaledLightness = lightness / 9.03292;
        xyz = {
            whiteX * (a / 3893.5 + scaledLightness),
            scaledLightness,
            whiteZ * (scaledLightness - b / 1557.4),
        };
    } else {
        const double x = scaledLightness + a / 5.0;
        const double z = scaledLightness - b / 2.0;
        xyz.x = whiteX * x * x * x;
        xyz.z = whiteZ * z * z * z;
    }
    return colorFromXyz(xyz, color);
}

bool colorFromCieLuv(double lightness, double u, double v, Color& color) {
    if (lightness < 0.0 || lightness > 100.0) {
        return false;
    }
    Triple uvY = whiteUvY();
    uvY.z = valueToY(lightness);
    if (lightness != 0.0) {
        const double scale = 13.0 * (lightness / 100.0);
        uvY.x += u / scale;
        uvY.y += v / scale;
    }
    return colorFromXyz(uvYToXyz(uvY), color);
}

bool colorFromTekHvc(double hue, double value, double chroma, Color& color) {
    if (value < 0.0 || value > 100.0 || chroma < 0.0) {
        return false;
    }
    hue = fmod(hue, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    return colorFromXyz(tekHvcToXyz({hue, value, chroma}), color);
}

bool finishColorNumber(double mantissa, bool negative, u32 exponent, bool exponentNegative, double& value) {
    value = (negative ? -mantissa : mantissa) * pow(10.0, exponentNegative ? -(double)(exponent) : (double)(exponent));
    return isfinite(value);
}
