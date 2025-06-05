/*
 * String utilities for the TAP protocol.
 *
 * Additional string utilities that can't be included with C TAP Harness
 * because they rely on additional portability code from rra-c-util.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2011-2012 Russ Allbery <eagle@eyrie.org>
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
#include <portable/system.h>

#include <tests/tap/basic.h>
#include <tests/tap/string.h>


/*
 * vsprintf into a newly allocated string, reporting a fatal error with bail
 * on failure.
 */
void
bvasprintf(char **strp, const char *fmt, va_list args)
{
    int status;

    status = vasprintf(strp, fmt, args);
    if (status < 0)
        sysbail("failed to allocate memory for vasprintf");
}


/*
 * sprintf into a newly allocated string, reporting a fatal error with bail on
 * failure.
 */
void
basprintf(char **strp, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    bvasprintf(strp, fmt, args);
    va_end(args);
}
