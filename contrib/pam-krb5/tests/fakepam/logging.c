/*
 * Logging functions for the fake PAM library, used for testing.
 *
 * This file contains the implementation of pam_syslog and pam_vsyslog, which
 * log to an internal buffer rather than to syslog, and the testing function
 * used to recover that buffer.  It also includes the pam_strerror
 * implementation.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <tests/fakepam/internal.h>
#include <tests/fakepam/pam.h>
#include <tests/tap/basic.h>
#include <tests/tap/string.h>

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))

/* The struct used to accumulate log messages. */
static struct output *messages = NULL;


/*
 * Allocate a new, empty output struct and call bail if memory allocation
 * fails.
 */
struct output *
output_new(void)
{
    struct output *output;

    output = bmalloc(sizeof(struct output));
    output->count = 0;
    output->allocated = 1;
    output->lines = bmalloc(sizeof(output->lines[0]));
    output->lines[0].line = NULL;
    return output;
}


/*
 * Add a new output line to the output struct, resizing the array as
 * necessary.  Calls bail if memory allocation fails.
 */
void
output_add(struct output *output, int priority, const char *string)
{
    size_t next = output->count;
    size_t size, n;

    if (output->count == output->allocated) {
        n = output->allocated + 1;
        size = sizeof(output->lines[0]);
        output->lines = breallocarray(output->lines, n, size);
        output->allocated = n;
    }
    output->lines[next].priority = priority;
    output->lines[next].line = bstrdup(string);
    output->count++;
}


/*
 * Return the error string associated with the PAM error code.  We do this as
 * a giant case statement so that we don't assume anything about the error
 * codes used by the system PAM library.
 */
const char *
pam_strerror(PAM_STRERROR_CONST pam_handle_t *pamh UNUSED, int code)
{
    /* clang-format off */
    switch (code) {
    case PAM_SUCCESS:     return "No error";
    case PAM_OPEN_ERR:    return "Failure loading service module";
    case PAM_SYMBOL_ERR:  return "Symbol not found";
    case PAM_SERVICE_ERR: return "Error in service module";
    case PAM_SYSTEM_ERR:  return "System error";
    case PAM_BUF_ERR:     return "Memory buffer error";
    default:              return "Unknown error";
    }
    /* clang-format on */
}


/*
 * Log a message using variadic arguments.  Just a wrapper around
 * pam_vsyslog.
 */
void
pam_syslog(const pam_handle_t *pamh, int priority, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    pam_vsyslog(pamh, priority, format, args);
    va_end(args);
}


/*
 * Log a PAM error message with a given priority.  Just appends the priority,
 * a space, and the error message, followed by a newline, to the internal
 * buffer, allocating new space if needed.  Ignore memory allocation failures;
 * we have no way of reporting them, but the tests will fail due to missing
 * output.
 */
void
pam_vsyslog(const pam_handle_t *pamh UNUSED, int priority, const char *format,
            va_list args)
{
    char *message = NULL;

    bvasprintf(&message, format, args);
    if (messages == NULL)
        messages = output_new();
    output_add(messages, priority, message);
    free(message);
}


/*
 * Used by test code.  Returns the accumulated messages in an output struct
 * and starts a new one.  Caller is responsible for freeing with
 * pam_output_free.
 */
struct output *
pam_output(void)
{
    struct output *output;

    output = messages;
    messages = NULL;
    return output;
}


/*
 * Free an output struct.
 */
void
pam_output_free(struct output *output)
{
    size_t i;

    if (output == NULL)
        return;
    for (i = 0; i < output->count; i++)
        if (output->lines[i].line != NULL)
            free(output->lines[i].line);
    free(output->lines);
    free(output);
}
