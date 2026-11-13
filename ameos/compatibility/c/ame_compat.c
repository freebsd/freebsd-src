/* SPDX-License-Identifier: BSD-2-Clause */
#include "ame_compat.h"

#include <stdlib.h>
#include <string.h>

static int valid_name(const char *name) {
    return name != NULL && name[0] != '\0' && strchr(name, '=') == NULL;
}

ame_compat_status ame_compat_get_version(ame_compat_version *out) {
    if (out == NULL) {
        return AME_COMPAT_INVALID_ARGUMENT;
    }
    out->abi_version = AME_COMPAT_ABI_VERSION;
    out->major = 0;
    out->minor = 1;
    out->patch = 0;
    return AME_COMPAT_OK;
}

ame_compat_status ame_compat_getenv(
    const char *name,
    char *out,
    size_t out_len,
    size_t *required_len) {
    const char *value;
    size_t value_len;

    if (!valid_name(name) || (out == NULL && out_len != 0)) {
        return AME_COMPAT_INVALID_ARGUMENT;
    }
    value = getenv(name);
    if (value == NULL) {
        if (required_len != NULL) {
            *required_len = 0;
        }
        if (out != NULL && out_len != 0) {
            out[0] = '\0';
        }
        return AME_COMPAT_NOT_FOUND;
    }

    value_len = strlen(value);
    if (required_len != NULL) {
        *required_len = value_len + 1;
    }
    if (out_len < value_len + 1) {
        if (out != NULL && out_len != 0) {
            out[0] = '\0';
        }
        return AME_COMPAT_BUFFER_TOO_SMALL;
    }
    memcpy(out, value, value_len + 1);
    return AME_COMPAT_OK;
}

ame_compat_status ame_compat_setenv(
    const char *name,
    const char *value,
    int overwrite) {
    if (!valid_name(name) || value == NULL) {
        return AME_COMPAT_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    (void)overwrite;
    return _putenv_s(name, value) == 0 ? AME_COMPAT_OK : AME_COMPAT_INVALID_ARGUMENT;
#else
    return setenv(name, value, overwrite != 0) == 0
        ? AME_COMPAT_OK
        : AME_COMPAT_INVALID_ARGUMENT;
#endif
}
