/*
 * Copyright (c) 2002,2003 Hewlett-Packard Company
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _KERNEL
#include <signal.h>
#endif

struct uwx_ttrace_info;

extern struct uwx_ttrace_info *uwx_ttrace_init_info(
    struct uwx_env *env,
    pid_t pid,
    lwpid_t lwpid,
    uint64_t load_map);

extern int uwx_ttrace_free_info(struct uwx_ttrace_info *info);

extern int uwx_ttrace_init_context(
    struct uwx_env *env,
    struct uwx_ttrace_info *info);

extern int uwx_ttrace_init_from_sigcontext(
    struct uwx_env *env,
    struct uwx_ttrace_info *info,
    uint64_t ucontext);

extern int uwx_ttrace_do_context_frame(
    struct uwx_env *env,
    struct uwx_ttrace_info *info);

extern int uwx_ttrace_copyin(
    int request,
    char *loc,
    uint64_t rem,
    int len,
    intptr_t tok);

extern int uwx_ttrace_lookupip(
    int request,
    uint64_t ip,
    intptr_t tok,
    uint64_t **resultp);

#define UWX_TT_ERR_BADABICONTEXT	(-101)
#define UWX_TT_ERR_TTRACE		(-102)
