#pragma once
// Private bridge between roados value types and Skia. Never installed.

#include "include/core/SkBlendMode.h"
#include "include/core/SkColor.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"

#include "roados/Color.h"
#include "roados/Geometry.h"
#include "roados/Paint.h"
#include "roados/Path.h"
#include "roados/Transform.h"

namespace roados::sk {

inline SkColor4f color4f(Color c) {
    return SkColor4f::FromColor(SkColorSetARGB(c.a, c.r, c.g, c.b));
}

inline SkMatrix matrix(const Transform& t) {
    return SkMatrix::MakeAll(t.a(), t.c(), t.e(), t.b(), t.d(), t.f(), 0, 0, 1);
}

inline SkRect rect(const Rect& r) { return SkRect::MakeXYWH(r.x, r.y, r.width, r.height); }
inline Rect rect(const SkRect& r) { return Rect::fromLTRB(r.left(), r.top(), r.right(), r.bottom()); }

SkPath path(const Path& p);
Path fromSkPath(const SkPath& p);
SkBlendMode blend(BlendMode mode);

}  // namespace roados::sk
