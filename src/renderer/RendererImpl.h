#pragma once
#include <map>
#include <mutex>
#include <string>

#include "include/core/SkFontMgr.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTypeface.h"
#include "roados/Renderer.h"

namespace roados {

struct ResolvedTypeface {
    sk_sp<SkTypeface> typeface;
    std::string family;
};

struct Renderer::Impl {
    Impl();
    /// Resolves a CSS-style family list to a typeface (cached, thread-safe).
    ResolvedTypeface typeface(const Font& font) const;

    sk_sp<SkFontMgr> fontMgr;
    mutable std::mutex mutex;
    mutable std::map<std::string, ResolvedTypeface> cache;
};

}  // namespace roados
