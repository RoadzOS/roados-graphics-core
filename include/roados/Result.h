#pragma once
/// Explicit, exception-free error handling for every fallible operation.

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace roados {

enum class ErrorCode {
    InvalidArgument,  ///< A caller-supplied value is out of range or malformed.
    Parse,            ///< Text input (colour, path data, SVG, JSON) could not be parsed.
    Io,               ///< A file could not be read or written.
    NotFound,         ///< A requested item (file, element id) does not exist.
    Unsupported,      ///< The input uses a feature this build does not support.
    Render,           ///< Skia failed to create a surface or encode output.
    Validation,       ///< Data loaded successfully but failed integrity/schema checks.
    Incompatible,     ///< Data was produced for an incompatible version.
};

const char* toString(ErrorCode code) noexcept;

struct Error {
    ErrorCode code;
    std::string message;
};

/// Holds either a value or an Error. Never throws.
template <class T>
class [[nodiscard]] Result {
public:
    Result(T value) : state_(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : state_(std::in_place_index<1>, std::move(error)) {}

    bool ok() const noexcept { return state_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    T& value() & { assert(ok()); return std::get<0>(state_); }
    const T& value() const& { assert(ok()); return std::get<0>(state_); }
    T&& value() && { assert(ok()); return std::get<0>(std::move(state_)); }

    const Error& error() const { assert(!ok()); return std::get<1>(state_); }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }

    T valueOr(T fallback) const& { return ok() ? std::get<0>(state_) : std::move(fallback); }

private:
    std::variant<T, Error> state_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)) {}

    bool ok() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
    const Error& error() const { assert(!ok()); return *error_; }

private:
    std::optional<Error> error_;
};

inline Error makeError(ErrorCode code, std::string message) { return Error{code, std::move(message)}; }

}  // namespace roados
