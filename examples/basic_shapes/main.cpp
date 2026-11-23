// Minimal command-line example: draws basic shapes and writes a PNG and an SVG.
//   ./basic_shapes [output.png]
#include <cstdio>
#include <fstream>
#include <string>

#include <roados/Graphics.h>

using namespace roados;

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : "basic_shapes.png";

    Canvas canvas(512, 512);
    canvas.setBackground(Color::Transparent());

    canvas.circle(Circle().center(120, 120).radius(50).fill(Color("#2196F3")));

    canvas.roundedRect(RoundedRect()
                           .position(200, 100)
                           .size(180, 120)
                           .radius(20)
                           .fill(Color("#FFFFFF"))
                           .stroke(Color("#222222"), 3));

    canvas.line(Line().from(30, 300).to(450, 300).stroke(Color("#00BCD4"), 5).lineCap(LineCap::Round));

    canvas.polygon(Polygon::regular(6, 120, 410, 60).fill(LinearGradient{
        {60, 350}, {180, 470}, {{0, Color("#FF9800")}, {1, Color("#E91E63")}}}));

    canvas.path(PathShape(Path().moveTo(240, 460).cubicTo(280, 340, 380, 520, 440, 360))
                    .stroke(Color("#8BC34A"), 6)
                    .dash({14, 8})
                    .lineCap(LineCap::Round)
                    .noFill());

    canvas.text(Text("RoadOS").position(290, 160).size(28).weight(800).anchor(TextAnchor::Middle)
                    .baseline(TextBaseline::Central).fill(Color("#222222")));

    auto image = canvas.render();
    if (!image) {
        std::fprintf(stderr, "render failed: %s\n", image.error().message.c_str());
        return 1;
    }
    if (auto saved = image->savePNG(out); !saved) {
        std::fprintf(stderr, "save failed: %s\n", saved.error().message.c_str());
        return 1;
    }

    auto svg = canvas.toSvg();
    if (svg) std::ofstream(out + ".svg") << *svg;

    const Version v = libraryVersion();
    std::printf("RoadOS Graphics Core %d.%d.%d (Skia %s): wrote %s (%dx%d)%s\n", v.major, v.minor, v.patch,
                skiaRevision(), out.c_str(), image->width(), image->height(), svg ? " and .svg" : "");
    return 0;
}
