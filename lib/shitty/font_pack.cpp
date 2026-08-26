/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "font_pack.h"
#include <lib/vterm/grapheme.h>

#include "composer.h"
#include "font_face.h"
#include "font_resolver.h"
#include <lib/vterm/unicode_map.h>
#include <lib/vterm/utf8.h>

#include <std/lib/buffer.h>
#include <std/sym/i_map.h>
#include <std/lib/vector.h>
#include <std/mem/obj_pool.h>
#include <std/str/builder.h>
#include <std/sys/throw.h>

#include <errno.h>
#include <math.h>
#include <string.h>

using namespace stl;

namespace {
    static constexpr u16 unresolvedFace = 0;
    static constexpr u16 uncoveredFace = UINT16_MAX;

    struct FontpackImpl final: public Fontpack {
        FontpackImpl(Composer& composer, ObjPool& pool, const StringView* names, size_t nameCount, u16 size);

        u16 getPx() const override;
        u16 getPy() const override;
        float boxDrawingStroke() const override;
        bool hasBold() const override;
        bool hasItalic() const override;
        bool hasBoldItalic() const override;

        // A pictogram behind a width-one codepoint can ink far past its
        Font* createOptional(Composer& composer, ObjPool& pool, StringView name, u16 size, FontStyle style, FontKind kind, FontMetrics metrics);
        Font* select(FontStyle style) const noexcept;
        Font* faceAt(u16 index) const noexcept;
        bool coversAll(Font* font, const u32* codepoints, size_t count) const;
        Font* resolveFace(const u32* codepoints, size_t count) override;
        void adoptFaceFor(const FontFaceMiss& miss) override;
        Font* styledFace(Font* face, FontStyle style) const override;
        void markUncovered(const u32* codepoints, size_t count);
        bool knownUncovered(const u32* codepoints, size_t count) const;
        float measureBoxDrawingStroke(Font* regular, const FontMetrics& actual);

        Composer* composer_ = nullptr;
        ObjPool* pool_ = nullptr;
        u16 size_ = 0;
        FontMetrics metrics_;
        float boxDrawingStroke_ = 0.0f;
        Font* regular_ = nullptr;
        Font* bold_ = nullptr;
        Font* italic_ = nullptr;
        Font* boldItalic_ = nullptr;
        Vector<Font*> fallbacks_;
        UnicodeMap<u16>* faceCache_ = nullptr;
        // Multi-codepoint clusters nothing serves, by content hash; the
        // single-codepoint verdicts live in faceCache_.
        IntMap<u64>* missedClusters_ = nullptr;
    };

    // Joiners and variation selectors modify a cluster but are absent from
    // most cmaps; they do not participate in coverage matching.
    static FontPlane clusterPlaneWish(const u32* codepoints, size_t count) {
        FontPlane wish = FontPlane::Any;
        for (size_t index = 0; index < count; ++index) {
            const u32 codepoint = codepoints[index];
            if (codepoint == 0xfe0f) {
                return FontPlane::Color;
            }
            if (codepoint == 0xfe0e) {
                return FontPlane::Mask;
            }
            if (emojiPresentation(codepoint)) {
                wish = FontPlane::Color;
            }
        }
        return wish;
    }

    static u64 clusterHash(const u32* codepoints, size_t count) {
        return StringView((const u8*)(codepoints), count * sizeof(u32)).hash64();
    }

    static bool significantCodepoint(u32 codepoint) {
        if (codepoint == 0x200d) {
            return false;
        }
        if (codepoint >= 0xfe00 && codepoint <= 0xfe0f) {
            return false;
        }
        if (codepoint >= 0xe0100 && codepoint <= 0xe01ef) {
            return false;
        }
        return true;
    }

    static float median(Vector<float>& values) {
        for (size_t index = 1; index < values.length(); ++index) {
            const float value = values[index];
            size_t position = index;
            while (position != 0 && values[position - 1] > value) {
                values.mut(position) = values[position - 1];
                --position;
            }
            values.mut(position) = value;
        }
        const size_t middle = values.length() / 2;
        return (values.length() & 1) != 0 ? values[middle] : (values[middle - 1] + values[middle]) * 0.5f;
    }

    // The effective perpendicular width of one long glyph stroke. Alpha
    // mass, rather than an ink bounding box, retains subpixel width. Fitting
    // x as a function of y also makes this usable for '/' when a face lacks
    // a useful pipe glyph.
    static float measureStroke(Font& font, const FontMetrics& metrics, u32 codepoint) {
        if (font.colored() || !font.covers(codepoint) || metrics.width == 0 || metrics.height == 0) {
            return 0.0f;
        }
        Buffer bitmap;
        bitmap.zero((size_t)(metrics.width) * metrics.height);
        font.render(&codepoint, 1, 1, bitmap.mutData());
        const auto* pixels = (const u8*)(bitmap.data());

        int first = metrics.height;
        int last = -1;
        for (int y = 0; y < metrics.height; ++y) {
            float mass = 0.0f;
            for (int x = 0; x < metrics.width; ++x) {
                mass += pixels[(size_t)(y) * metrics.width + x] / 255.0f;
            }
            if (mass > 0.125f) {
                first = first < y ? first : y;
                last = last > y ? last : y;
            }
        }
        if (last - first < 3) {
            return 0.0f;
        }
        const int trim = (last - first + 1) / 4;
        first += trim;
        last -= trim;

        float sumY = 0.0f;
        float sumX = 0.0f;
        float sumYY = 0.0f;
        float sumYX = 0.0f;
        int rows = 0;
        for (int y = first; y <= last; ++y) {
            float mass = 0.0f;
            float moment = 0.0f;
            for (int x = 0; x < metrics.width; ++x) {
                const float alpha = pixels[(size_t)(y) * metrics.width + x] / 255.0f;
                mass += alpha;
                moment += alpha * ((float)(x) + 0.5f);
            }
            if (mass <= 0.125f) {
                continue;
            }
            const float centroid = moment / mass;
            const float fy = (float)(y) + 0.5f;
            sumY += fy;
            sumX += centroid;
            sumYY += fy * fy;
            sumYX += fy * centroid;
            ++rows;
        }
        if (rows < 2) {
            return 0.0f;
        }
        const float denominator = rows * sumYY - sumY * sumY;
        const float slope = fabsf(denominator) > 1e-6f ? (rows * sumYX - sumY * sumX) / denominator : 0.0f;
        const float normalScale = sqrtf(1.0f + slope * slope);

        Vector<float> widths;
        for (int y = first; y <= last; ++y) {
            float mass = 0.0f;
            for (int x = 0; x < metrics.width; ++x) {
                mass += pixels[(size_t)(y) * metrics.width + x] / 255.0f;
            }
            if (mass > 0.125f) {
                widths.pushBack(mass / normalScale);
            }
        }
        return widths.empty() ? 0.0f : median(widths);
    }
}

FontpackImpl::FontpackImpl(Composer& composer, ObjPool& pool, const StringView* names, size_t nameCount, u16 size)
    : composer_(&composer)
    , pool_(&pool)
    , size_(size)
{
    const StringView primary = nameCount != 0 ? names[0] : StringView();
    regular_ = composer.loadFont(pool, {primary, size, FontStyle::Regular, FontKind::Primary}, metrics_);
    if (regular_ == nullptr) {
        Errno(EINVAL).raise(StringBuilder() << StringView(u8"no suitable font found for ") << primary);
    }
    boxDrawingStroke_ = measureBoxDrawingStroke(regular_, metrics_);

    bold_ = createOptional(composer, pool, primary, size, FontStyle::Bold, FontKind::Overlay, metrics_);
    italic_ = createOptional(composer, pool, primary, size, FontStyle::Italic, FontKind::Overlay, metrics_);
    boldItalic_ = createOptional(composer, pool, primary, size, FontStyle::BoldItalic, FontKind::Overlay, metrics_);
    if (bold_ == nullptr) {
        bold_ = regular_->synthesize(pool, FontStyle::Bold);
    }
    if (italic_ == nullptr) {
        italic_ = regular_->synthesize(pool, FontStyle::Italic);
    }
    if (boldItalic_ == nullptr) {
        boldItalic_ = regular_->synthesize(pool, FontStyle::BoldItalic);
    }

    for (size_t index = 1; index < nameCount; ++index) {
        Font* const fallback = createOptional(composer, pool, names[index], size, FontStyle::Regular, FontKind::Fallback, metrics_);
        if (fallback != nullptr) {
            fallbacks_.pushBack(fallback);
        }
    }

    faceCache_ = UnicodeMap<u16>::create(pool);
    missedClusters_ = pool.make<IntMap<u64>>(&pool);
}

float FontpackImpl::measureBoxDrawingStroke(Font* regular, const FontMetrics& actual) {
    static constexpr u16 measurementPixels = 128;
    FontFace* const face = regular->face();
    if (face == nullptr) {
        return 0.0f;
    }
    try {
        // Hinting noise dominates at ordinary terminal sizes. Probe the
        // regular primary face once at a large size, then scale the alpha-
        // mass result back by the actual cell height. A pipe is ideal; the
        // slash estimator removes its measured slope (the aspect-ratio
        // correction) and is the fallback for faces without a usable pipe.
        auto measurementPool = ObjPool::fromMemory();
        FontMetrics measurementMetrics;
        Font* measurement = composer_->renderFace(*measurementPool, face, measurementPixels, FontKind::Primary, measurementMetrics);
        if (measurement == nullptr || measurementMetrics.height == 0) {
            return 0.0f;
        }
        const float shortSide = (float)(measurementMetrics.width < measurementMetrics.height ? measurementMetrics.width : measurementMetrics.height);
        float width = measureStroke(*measurement, measurementMetrics, '|');
        if (width <= 0.0f || width > shortSide * 0.4f) {
            width = measureStroke(*measurement, measurementMetrics, '/');
        }
        if (width <= 0.0f || width > shortSide * 0.4f) {
            return 0.0f;
        }
        return width * actual.height / measurementMetrics.height;
    } catch (Exception&) {
        // Stem matching is cosmetic and must never make a usable font fail
        // to load; Composer supplies a deterministic geometric fallback.
        return 0.0f;
    }
}

Font* FontpackImpl::createOptional(Composer& composer, ObjPool& pool, StringView name, u16 size, FontStyle style, FontKind kind, FontMetrics metrics) {
    try {
        return composer.loadFont(pool, {name, size, style, kind}, metrics);
    } catch (Exception&) {
        return nullptr;
    }
}

u16 FontpackImpl::getPx() const {
    return metrics_.width;
}

u16 FontpackImpl::getPy() const {
    return metrics_.height;
}

float FontpackImpl::boxDrawingStroke() const {
    return boxDrawingStroke_;
}

bool FontpackImpl::hasBold() const {
    return bold_ != nullptr;
}

bool FontpackImpl::hasItalic() const {
    return italic_ != nullptr;
}

bool FontpackImpl::hasBoldItalic() const {
    return boldItalic_ != nullptr;
}

Font* FontpackImpl::styledFace(Font* face, FontStyle style) const {
    return face == regular_ ? select(style) : face;
}

Font* FontpackImpl::select(FontStyle style) const noexcept {
    switch (style) {
        case FontStyle::Bold:
            return bold_ != nullptr ? bold_ : regular_;
        case FontStyle::Italic:
            return italic_ != nullptr ? italic_ : regular_;
        case FontStyle::BoldItalic:
            if (boldItalic_ != nullptr) {
                return boldItalic_;
            }
            if (italic_ != nullptr) {
                return italic_;
            }
            return bold_ != nullptr ? bold_ : regular_;
        case FontStyle::Regular:
            return regular_;
    }
    return regular_;
}

Font* FontpackImpl::faceAt(u16 index) const noexcept {
    return index == 0 ? regular_ : fallbacks_[index - 1u];
}

bool FontpackImpl::coversAll(Font* font, const u32* codepoints, size_t count) const {
    for (size_t index = 0; index < count; ++index) {
        if (significantCodepoint(codepoints[index]) && !font->covers(codepoints[index])) {
            return false;
        }
    }
    return true;
}

Font* FontpackImpl::resolveFace(const u32* codepoints, size_t count) {
    u16* cached = nullptr;
    if (count == 1) {
        cached = &(*faceCache_)[codepoints[0]];
        if (*cached == uncoveredFace) {
            return nullptr;
        }
        if (*cached != unresolvedFace) {
            return faceAt((u16)(*cached - 1u));
        }
    }

    // An emoji-presentation cluster looks for a color face across the
    // whole chain first, so a monochrome face cannot shadow a color
    // emoji font behind it; an explicit VS15 asks for the opposite.
    const FontPlane wish = clusterPlaneWish(codepoints, count);
    if (wish != FontPlane::Any) {
        const bool wantColor = wish == FontPlane::Color;
        if (regular_->colored() == wantColor && coversAll(regular_, codepoints, count)) {
            if (cached != nullptr) {
                *cached = 1;
            }
            return regular_;
        }
        for (size_t index = 0; index < fallbacks_.length(); ++index) {
            Font* const fallback = fallbacks_[index];
            if (fallback->colored() == wantColor && coversAll(fallback, codepoints, count)) {
                if (cached != nullptr) {
                    *cached = (u16)(index + 2u);
                }
                return fallback;
            }
        }
    }

    if (coversAll(regular_, codepoints, count)) {
        if (cached != nullptr) {
            *cached = 1;
        }
        return regular_;
    }
    for (size_t index = 0; index < fallbacks_.length(); ++index) {
        Font* const fallback = fallbacks_[index];
        if (coversAll(fallback, codepoints, count)) {
            if (cached != nullptr) {
                *cached = (u16)(index + 2u);
            }
            return fallback;
        }
    }
    if (count > 1 && knownUncovered(codepoints, count)) {
        return nullptr;
    }
    // No verdict yet: unwind the frame like a lost surface. The renderer
    // catches at the top, adoptFaceFor() settles the verdict, and the
    // frame re-runs.
    FontFaceMiss miss;
    miss.count = count < FontFaceMiss::limit ? count : FontFaceMiss::limit;
    memcpy(miss.codepoints, codepoints, miss.count * sizeof(u32));
    throw miss;
}

void FontpackImpl::markUncovered(const u32* codepoints, size_t count) {
    if (count == 1) {
        (*faceCache_)[codepoints[0]] = uncoveredFace;
        return;
    }
    const u64 key = clusterHash(codepoints, count);
    *missedClusters_->insert(key) = key;
}

bool FontpackImpl::knownUncovered(const u32* codepoints, size_t count) const {
    return missedClusters_->find(clusterHash(codepoints, count)) != nullptr;
}

void FontpackImpl::adoptFaceFor(const FontFaceMiss& miss) {
    u32 significant[FontFaceMiss::limit];
    size_t filtered = 0;
    for (size_t index = 0; index < miss.count; ++index) {
        if (significantCodepoint(miss.codepoints[index])) {
            significant[filtered++] = miss.codepoints[index];
        }
    }
    if (filtered != 0) {
        const FontPlane plane = clusterPlaneWish(miss.codepoints, miss.count);
        for (IntrusiveNode* node = composer_->fontResolvers.mutFront(); node != composer_->fontResolvers.mutEnd(); node = node->next) {
            FontResolver* const resolver = static_cast<FontResolver*>(node);
            FontFace* const face = resolver->resolveCluster(significant, filtered, plane);
            if (face == nullptr) {
                continue;
            }
            FontMetrics metrics = metrics_;
            Font* const font = composer_->renderFace(*pool_, face, size_, FontKind::Fallback, metrics);
            // The system can answer with a face that does not actually
            // cover the cluster (fontconfig matches unconditionally);
            // adopting it would not settle anything.
            if (font == nullptr || !coversAll(font, miss.codepoints, miss.count)) {
                continue;
            }
            // A cluster with an explicit plane wish only adopts a face of
            // that plane: a host answer on the wrong plane would shadow
            // the right face further down the chain.
            if (plane != FontPlane::Any && font->colored() != (plane == FontPlane::Color)) {
                continue;
            }
            fallbacks_.pushBack(font);
            return;
        }
    }
    markUncovered(miss.codepoints, miss.count);
}

Fontpack* Fontpack::create(Composer& composer, ObjPool& pool, const StringView* names, size_t nameCount, u16 size) {
    return pool.make<FontpackImpl>(composer, pool, names, nameCount, size);
}
