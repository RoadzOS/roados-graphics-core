// Renders any SVG file to PNG through Skia's SVG module.
//   ./svg_rendering <input.svg> <output.png> [size_px] [--raw]
// --raw disables RoadOS text normalisation (to compare against plain Skia).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <roados/Graphics.h>

using namespace roados;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <input.svg> <output.png> [size_px] [--raw]\n", argv[0]);
        return 2;
    }
    const int size = argc > 3 && argv[3][0] != '-' ? std::atoi(argv[3]) : 256;
    const bool raw = argc > 3 && std::strcmp(argv[argc - 1], "--raw") == 0;

    auto doc = SvgDocument::load(argv[1], SvgOptions{.normalizeText = !raw});
    if (!doc) {
        std::fprintf(stderr, "error: %s\n", doc.error().message.c_str());
        return 1;
    }
    const auto& rep = doc->report();
    std::printf("%s: %gx%g, text=%d baselines=%d families=%d\n", argv[1], doc->intrinsicSize().width,
                doc->intrinsicSize().height, rep.textElements, rep.baselinesAdjusted, rep.fontFamiliesResolved);
    for (const auto& w : rep.warnings) std::printf("  warning: %s\n", w.c_str());

    Canvas canvas(size, size);
    canvas.svg(SvgShape(*doc).size(float(size), float(size)));
    auto img = canvas.render();
    if (!img) {
        std::fprintf(stderr, "render error: %s\n", img.error().message.c_str());
        return 1;
    }
    if (auto r = img->savePNG(argv[2]); !r) {
        std::fprintf(stderr, "save error: %s\n", r.error().message.c_str());
        return 1;
    }
    return 0;
}
