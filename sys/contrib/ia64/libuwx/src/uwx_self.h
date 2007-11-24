/*
Copyright (c) 2003 Hewlett-Packard Development Company, L.P.
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __UWX_SELF_INCLUDED
#define __UWX_SELF_INCLUDED 1

#include <signal.h>

#ifndef __UWX_INCLUDED
#include "uwx.h"
#endif /* __UWX_INCLUDED */

#if defined(__cplusplus)
#define __EXTERN_C extern "C"
#else
#define __EXTERN_C extern
#endif

struct uwx_self_info;

__EXTERN_C struct uwx_self_info *uwx_self_init_info(struct uwx_env *env);

__EXTERN_C int uwx_self_free_info(struct uwx_self_info *info);

__EXTERN_C int uwx_self_init_context(struct uwx_env *env);

__EXTERN_C int uwx_self_init_from_sigcontext(
    struct uwx_env *env,
    struct uwx_self_info *info,
    ucontext_t *ucontext);

__EXTERN_C int uwx_self_do_context_frame(
    struct uwx_env *env,
    struct uwx_self_info *info);

__EXTERN_C int uwx_self_copyin(
    int request,
    char *loc,
    uint64_t rem,
    int len,
    intptr_t tok);

__EXTERN_C int uwx_self_lookupip(
    int request,
    uint64_t ip,
    intptr_t tok,
    uint64_t **resultp);

#define UWX_SELF_ERR_BADABICONTEXT  (-101)

#undef __EXTERN_C

#if defined(__cplusplus)

class UnwindExpressSelf : public UnwindExpress {

public:

    UnwindExpressSelf() {
	info = uwx_self_init_info(env);
	(void)uwx_register_callbacks(env, (intptr_t)info,
				uwx_self_copyin, uwx_self_lookupip);
    }

    ~UnwindExpressSelf() {
	if (info != 0)
	    uwx_self_free_info(info);
	info = 0;
    }

    int init_context() {
	return uwx_self_init_context(env);
    }

    int init_context(ucontext_t *ucontext) {
	return uwx_self_init_from_sigcontext(env, info, ucontext);
    }

    int do_context_frame() {
	return uwx_self_do_context_frame(env, info);
    }

protected:

    struct uwx_self_info *info;

};

#endif /* __cplusplus */

#endif /* __UWX_SELF_INCLUDED */
