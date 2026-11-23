#pragma once
// Number and string formatting with JavaScript semantics, so the C++ port of
// the Studio's composeIcon() emits byte-identical markup.

#include <string>
#include <string_view>

namespace roados::rvhl::detail {

/// `${x}` / String(x): shortest round-trip representation ("20", "46.5", "-8").
std::string jsNumber(double x);

/// Number.prototype.toFixed(digits): exact decimal value, ties rounded away
/// from zero (C printf rounds ties to even, which differs for e.g. 1.03125).
std::string jsToFixed(double x, int digits);

/// Math.round(x): nearest integer, ties toward +∞.
double jsRound(double x);

/// The Studio's escapeXml(): & < > " (not ').
std::string escapeXml(std::string_view s);

}  // namespace roados::rvhl::detail
