/* SPDX-License-Identifier: BSD-2-Clause */
#include "ame_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <errno.h>
#include <process.h>
#else
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__x86_64__) && !defined(_WIN32)
extern void *ame_compat_memmove_asm(void *destination, const void *source, size_t length);
extern void *ame_compat_memset_asm(void *destination, int value, size_t length);
#endif

struct ame_compat_file {
    FILE *stream;
};

struct ame_compat_process {
#if defined(_WIN32)
    intptr_t pid;
#else
    pid_t pid;
#endif
};

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

void *ame_compat_memmove(void *destination, const void *source, size_t length) {
#if defined(__x86_64__) && !defined(_WIN32)
    return ame_compat_memmove_asm(destination, source, length);
#else
    return memmove(destination, source, length);
#endif
}

void *ame_compat_memset(void *destination, int value, size_t length) {
#if defined(__x86_64__) && !defined(_WIN32)
    return ame_compat_memset_asm(destination, value, length);
#else
    return memset(destination, value, length);
#endif
}

ame_compat_status ame_compat_file_open(
    const char *path,
    int write,
    ame_compat_file **out) {
    ame_compat_file *file;
    if (path == NULL || path[0] == '\0' || out == NULL) {
        return AME_COMPAT_INVALID_ARGUMENT;
    }
    file = (ame_compat_file *)calloc(1, sizeof(*file));
    if (file == NULL) {
        return AME_COMPAT_IO_ERROR;
    }
    file->stream = fopen(path, write != 0 ? "wb" : "rb");
    if (file->stream == NULL) {
        free(file);
        return AME_COMPAT_NOT_FOUND;
    }
    *out = file;
    return AME_COMPAT_OK;
}

ame_compat_status ame_compat_file_read(
    ame_compat_file *file,
    void *buffer,
    size_t buffer_len,
    size_t *bytes_read) {
    size_t count;
    if (file == NULL || file->stream == NULL || (buffer == NULL && buffer_len != 0)) {
        return AME_COMPAT_INVALID_HANDLE;
    }
    count = fread(buffer, 1, buffer_len, file->stream);
    if (bytes_read != NULL) {
        *bytes_read = count;
    }
    if (ferror(file->stream)) {
        clearerr(file->stream);
        return AME_COMPAT_IO_ERROR;
    }
    return AME_COMPAT_OK;
}

ame_compat_status ame_compat_file_write(
    ame_compat_file *file,
    const void *buffer,
    size_t buffer_len,
    size_t *bytes_written) {
    size_t count;
    if (file == NULL || file->stream == NULL || (buffer == NULL && buffer_len != 0)) {
        return AME_COMPAT_INVALID_HANDLE;
    }
    count = fwrite(buffer, 1, buffer_len, file->stream);
    if (bytes_written != NULL) {
        *bytes_written = count;
    }
    if (count != buffer_len || ferror(file->stream)) {
        clearerr(file->stream);
        return AME_COMPAT_IO_ERROR;
    }
    return AME_COMPAT_OK;
}

ame_compat_status ame_compat_file_close(ame_compat_file *file) {
    int result;
    if (file == NULL || file->stream == NULL) {
        return AME_COMPAT_INVALID_HANDLE;
    }
    result = fclose(file->stream);
    file->stream = NULL;
    free(file);
    return result == 0 ? AME_COMPAT_OK : AME_COMPAT_IO_ERROR;
}

static int valid_argv(const char *const *argv) {
    return argv != NULL && argv[0] != NULL && argv[0][0] != '\0';
}

ame_compat_status ame_compat_process_open(
    const char *path,
    const char *const *argv,
    ame_compat_process **out) {
    ame_compat_process *process;
    if (path == NULL || path[0] == '\0' || !valid_argv(argv) || out == NULL) {
        return AME_COMPAT_INVALID_ARGUMENT;
    }
    process = (ame_compat_process *)calloc(1, sizeof(*process));
    if (process == NULL) {
        return AME_COMPAT_IO_ERROR;
    }
#if defined(_WIN32)
    process->pid = _spawnvp(_P_NOWAIT, path, argv);
    if (process->pid == (intptr_t)-1) {
        free(process);
        return errno == ENOENT ? AME_COMPAT_NOT_FOUND : AME_COMPAT_IO_ERROR;
    }
#else
    process->pid = fork();
    if (process->pid < 0) {
        free(process);
        return AME_COMPAT_IO_ERROR;
    }
    if (process->pid == 0) {
        execv(path, (char *const *)argv);
        _exit(127);
    }
#endif
    *out = process;
    return AME_COMPAT_OK;
}

ame_compat_status ame_compat_process_wait(
    ame_compat_process *process,
    int32_t *exit_status) {
    if (process == NULL || exit_status == NULL) {
        return AME_COMPAT_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    {
        int status = 0;
        if (_cwait(&status, process->pid, 0) == -1) {
            return AME_COMPAT_IO_ERROR;
        }
        *exit_status = status;
        return AME_COMPAT_OK;
    }
#else
    {
        int status;
        if (waitpid(process->pid, &status, 0) < 0) {
            return AME_COMPAT_IO_ERROR;
        }
        *exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return AME_COMPAT_OK;
    }
#endif
}

ame_compat_status ame_compat_process_close(ame_compat_process *process) {
    if (process == NULL) {
        return AME_COMPAT_INVALID_HANDLE;
    }
    free(process);
    return AME_COMPAT_OK;
}
