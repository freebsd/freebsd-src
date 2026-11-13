/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef AME_COMPAT_HPP
#define AME_COMPAT_HPP

#include "ame_compat.h"

#include <stdexcept>
#include <string>
#include <vector>

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

inline void *memmove(void *destination, const void *source, size_t length) {
    return ame_compat_memmove(destination, source, length);
}

inline void *memset(void *destination, int value, size_t length) {
    return ame_compat_memset(destination, value, length);
}

class Process {
public:
    Process(const char *path, const std::vector<const char *> &arguments = {}) {
        argv_.reserve(arguments.size() + 2);
        argv_.push_back(path);
        argv_.insert(argv_.end(), arguments.begin(), arguments.end());
        argv_.push_back(nullptr);
        const auto status = ame_compat_process_open(path, argv_.data(), &raw_);
        if (status != AME_COMPAT_OK) {
            throw Error(status);
        }
    }

    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;

    ~Process() {
        if (raw_ != nullptr) {
            ame_compat_process_close(raw_);
        }
    }

    int32_t wait() {
        int32_t status_code = 0;
        const auto status = ame_compat_process_wait(raw_, &status_code);
        if (status != AME_COMPAT_OK) {
            throw Error(status);
        }
        return status_code;
    }

private:
    ame_compat_process *raw_ = nullptr;
    std::vector<const char *> argv_;
};

class File {
public:
    File(const char *path, bool write) {
        const auto status = ame_compat_file_open(path, write ? 1 : 0, &raw_);
        if (status != AME_COMPAT_OK) {
            throw Error(status);
        }
    }

    File(const File &) = delete;
    File &operator=(const File &) = delete;

    ~File() {
        if (raw_ != nullptr) {
            ame_compat_file_close(raw_);
        }
    }

    std::vector<unsigned char> read(size_t capacity) {
        std::vector<unsigned char> result(capacity);
        size_t count = 0;
        const auto status = ame_compat_file_read(raw_, result.data(), result.size(), &count);
        if (status != AME_COMPAT_OK) {
            throw Error(status);
        }
        result.resize(count);
        return result;
    }

    size_t write(const void *data, size_t length) {
        size_t count = 0;
        const auto status = ame_compat_file_write(raw_, data, length, &count);
        if (status != AME_COMPAT_OK) {
            throw Error(status);
        }
        return count;
    }

private:
    ame_compat_file *raw_ = nullptr;
};

} // namespace ame::compat

#endif
