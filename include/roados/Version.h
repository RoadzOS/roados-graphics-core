#pragma once

namespace roados {

struct Version {
    int major, minor, patch;
};

/// Version of the roados_graphics library.
Version libraryVersion() noexcept;
/// Short git revision of the Skia build this library was linked against.
const char* skiaRevision() noexcept;

}  // namespace roados
