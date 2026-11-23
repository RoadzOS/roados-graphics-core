#pragma once
/// A Canvas is a fixed-size drawing area that records a scene. Calling
/// render() rasterises it with Skia. Building a scene is not thread-safe.
/// Rendering a const Canvas may happen on several threads at once.

#include <string>

#include "roados/Image.h"
#include "roados/Renderer.h"
#include "roados/Scene.h"

namespace roados {

class Canvas {
public:
    Canvas(int width, int height) : width_(width), height_(height) {}

    int width() const { return width_; }
    int height() const { return height_; }

    Canvas& setBackground(Color c) { background_ = c; return *this; }
    Color background() const { return background_; }

    Canvas& draw(Node node) { root_.add(std::move(node)); return *this; }
    Canvas& circle(Circle s) { return draw(std::move(s)); }
    Canvas& ellipse(Ellipse s) { return draw(std::move(s)); }
    Canvas& rectangle(Rectangle s) { return draw(std::move(s)); }
    Canvas& roundedRect(RoundedRect s) { return draw(std::move(s)); }
    Canvas& square(Square s) { return draw(std::move(s)); }
    Canvas& triangle(Triangle s) { return draw(std::move(s)); }
    Canvas& polygon(Polygon s) { return draw(std::move(s)); }
    Canvas& line(Line s) { return draw(std::move(s)); }
    Canvas& polyline(Polyline s) { return draw(std::move(s)); }
    Canvas& path(PathShape s) { return draw(std::move(s)); }
    Canvas& text(Text s) { return draw(std::move(s)); }
    Canvas& image(ImageShape s) { return draw(std::move(s)); }
    Canvas& svg(SvgShape s) { return draw(std::move(s)); }
    Canvas& group(Group g) { return draw(std::move(g)); }

    Group& root() { return root_; }
    const Group& root() const { return root_; }
    void clear() { root_.clear(); }

    /// Rasterises with the shared default renderer.
    Result<Image> render(const RenderOptions& options = {}) const;
    /// Serialises to SVG with the shared default renderer.
    Result<std::string> toSvg() const;

private:
    int width_, height_;
    Color background_ = Color::Transparent();
    Group root_;
};

}  // namespace roados
