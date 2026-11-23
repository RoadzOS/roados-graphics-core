#pragma once
// Minimal SHA-256 (FIPS 180-4) for verifying RVHL bundle files against the
// manifest. Not used for anything security-sensitive beyond integrity checks.

#include <string>
#include <string_view>

namespace roados::rvhl::detail {

/// Lower-case hex SHA-256 digest of `data`.
std::string sha256Hex(std::string_view data);

}  // namespace roados::rvhl::detail
