/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef AME_COMPAT_HPP
#define AME_COMPAT_HPP

#include "ame_compat.h"

#include <stdexcept>
#include <string>

namespace ame::compat {

class Error : public std::runtime_error {
public:
    explicit Error(ame_compat_status status)
        : std::runtime_error("AmeOS compatibility operation failed"), status_(status) {}

    ame_compat_status status() const noexcept { return status_; }

private:
    ame_compat_status status_;
};

inline ame_compat_version version() {
    ame_compat_version result{};
    if (ame_compat_get_version(&result) != AME_COMPAT_OK) {
        throw Error(AME_COMPAT_INVALID_ARGUMENT);
    }
    return result;
}

inline std::string getenv(const char *name) {
    size_t required = 0;
    ame_compat_status status = ame_compat_getenv(name, nullptr, 0, &required);
    if (status == AME_COMPAT_NOT_FOUND) {
        return {};
    }
    if (status != AME_COMPAT_BUFFER_TOO_SMALL && status != AME_COMPAT_OK) {
        throw Error(status);
    }
    std::string result(required, '\0');
    status = ame_compat_getenv(name, result.data(), result.size(), nullptr);
    if (status != AME_COMPAT_OK) {
        throw Error(status);
    }
    result.pop_back();
    return result;
}

inline void setenv(const char *name, const char *value, bool overwrite = true) {
    const auto status = ame_compat_setenv(name, value, overwrite ? 1 : 0);
    if (status != AME_COMPAT_OK) {
        throw Error(status);
    }
}

} // namespace ame::compat

#endif
