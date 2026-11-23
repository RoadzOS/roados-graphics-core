#pragma once
/// Scene graph: a Group (layer) holds an ordered list of child nodes. Children
/// are drawn in list order, so later children appear on top.

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "roados/Shape.h"
#include "roados/Svg.h"
#include "roados/Text.h"

namespace roados {

class Node;

/// A group of nodes sharing a transform, opacity and blend mode. With
/// opacity < 1 or a non-SrcOver blend mode, the group is composited as an
/// isolated layer (SVG <g opacity> semantics).
class Group {
public:
    Group() = default;
    explicit Group(std::string name);  // defined below, once Node is complete

    Group& add(Node node);
    Group& name(std::string n) { name_ = std::move(n); return *this; }
    Group& visible(bool v) { visible_ = v; return *this; }
    Group& opacity(float o) { opacity_ = o; return *this; }
    Group& blend(BlendMode m) { blend_ = m; return *this; }
    Group& transform(const Transform& t) { transform_ = t; return *this; }
    Group& clip(const Rect& r) { clip_ = r; return *this; }
    Group& noClip() { clip_.reset(); return *this; }

    const std::string& name() const { return name_; }
    bool visible() const { return visible_; }
    float opacity() const { return opacity_; }
    BlendMode blend() const { return blend_; }
    const Transform& transform() const { return transform_; }
    const std::optional<Rect>& clip() const { return clip_; }

    std::vector<Node>& children() { return children_; }
    const std::vector<Node>& children() const { return children_; }
    std::size_t size() const;
    void clear();

    /// Direct child group with the given name, or nullptr.
    Group* findGroup(std::string_view n);
    const Group* findGroup(std::string_view n) const;
    /// Moves the child at `from` to index `to` (both must be < size()).
    bool move(std::size_t from, std::size_t to);
    bool remove(std::size_t index);

private:
    std::vector<Node> children_;
    std::string name_;
    bool visible_ = true;
    float opacity_ = 1;
    BlendMode blend_ = BlendMode::SrcOver;
    Transform transform_;
    std::optional<Rect> clip_;
};

using NodeVariant = std::variant<Circle, Ellipse, Rectangle, RoundedRect, Square, Triangle, Polygon, Line,
                                 Polyline, PathShape, Text, ImageShape, SvgShape, Group>;

class Node {
public:
    template <class T>
        requires std::is_constructible_v<NodeVariant, T&&> && (!std::is_same_v<std::remove_cvref_t<T>, Node>)
    Node(T&& value) : value_(std::forward<T>(value)) {}

    const NodeVariant& value() const { return value_; }
    NodeVariant& value() { return value_; }

    template <class T> bool is() const { return std::holds_alternative<T>(value_); }
    template <class T> T* as() { return std::get_if<T>(&value_); }
    template <class T> const T* as() const { return std::get_if<T>(&value_); }

private:
    NodeVariant value_;
};

inline Group::Group(std::string name) : name_(std::move(name)) {}
inline Group& Group::add(Node node) {
    children_.push_back(std::move(node));
    return *this;
}
inline std::size_t Group::size() const { return children_.size(); }
inline void Group::clear() { children_.clear(); }

}  // namespace roados
