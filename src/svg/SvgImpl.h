#pragma once
#include <string>

#include "include/core/SkRefCnt.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "roados/Svg.h"

namespace roados {

struct SvgDocument::Impl {
    sk_sp<SkSVGDOM> dom;
    Size size;
    std::optional<Rect> viewBox;
    SvgReport report;
    std::string source;
};

/// Text normalisation pass (see Svg.h). Exposed for testing.
std::string normalizeSvgText(std::string_view svg, const Renderer& renderer, SvgReport& report);

}  // namespace roados
