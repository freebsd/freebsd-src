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

struct uwx_self_info;

extern struct uwx_self_info *uwx_self_init_info(struct uwx_env *env);

extern int uwx_self_free_info(struct uwx_self_info *info);

extern int uwx_self_init_context(struct uwx_env *env);

extern int uwx_self_init_from_sigcontext(
    struct uwx_env *env,
    struct uwx_self_info *info,
    ucontext_t *ucontext);

extern int uwx_self_do_context_frame(
    struct uwx_env *env,
    struct uwx_self_info *info);

extern int uwx_self_copyin(
    int request,
    char *loc,
    uint64_t rem,
    int len,
    intptr_t tok);

extern int uwx_self_lookupip(
    int request,
    uint64_t ip,
    intptr_t tok,
    uint64_t **resultp);

#define UWX_SELF_ERR_BADABICONTEXT  (-101)
