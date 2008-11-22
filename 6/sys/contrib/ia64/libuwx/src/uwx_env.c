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

#ifndef _KERNEL
#include <stdlib.h>
#endif

#include "uwx_env.h"
#include "uwx_scoreboard.h"
#include "uwx_str.h"
#include "uwx_trace.h"

#ifdef _KERNEL
static struct uwx_env uwx_env;
#define	free(p)		/* nullified */
#define	malloc(sz)	((sz == sizeof(uwx_env)) ? &uwx_env : NULL)
#endif

alloc_cb uwx_allocate_cb = 0;
free_cb uwx_free_cb = 0;

int uwx_register_alloc_cb(alloc_cb alloc, free_cb free)
{
    uwx_allocate_cb = alloc;
    uwx_free_cb = free;
    return UWX_OK;
}

int uwx_init_history(struct uwx_env *env)
{
    int i;

    if (env == 0)
	return UWX_ERR_NOENV;

    for (i = 0; i < NSPECIALREG; i++)
	env->history.special[i] = UWX_DISP_REG(i);;
    for (i = 0; i < NPRESERVEDGR; i++)
	env->history.gr[i] = UWX_DISP_REG(UWX_REG_GR(4+i));
    for (i = 0; i < NPRESERVEDBR; i++)
	env->history.br[i] = UWX_DISP_REG(UWX_REG_BR(1+i));
    for (i = 0; i < 4; i++)
	env->history.fr[i] = UWX_DISP_REG(UWX_REG_FR(2+i));
    for ( ; i < NPRESERVEDFR; i++)
	env->history.fr[i] = UWX_DISP_REG(UWX_REG_FR(12+i));

    return UWX_OK;
}

struct uwx_env *uwx_init()
{
    int i;
    struct uwx_env *env;

    if (uwx_allocate_cb == 0)
	env = (struct uwx_env *) malloc(sizeof(struct uwx_env));
    else
	env = (struct uwx_env *) (*uwx_allocate_cb)(sizeof(struct uwx_env));
    if (env != 0) {
	env->context.valid_regs = 0;
	env->context.valid_frs = 0;
	for (i = 0; i < NSPECIALREG; i++)
	    env->context.special[i] = 0;
	for (i = 0; i < NPRESERVEDGR; i++)
	    env->context.gr[i] = 0;
	for (i = 0; i < NPRESERVEDBR; i++)
	    env->context.br[i] = 0;
	for (i = 0; i < NPRESERVEDFR; i++) {
	    env->context.fr[i].part0 = 0;
	    env->context.fr[i].part1 = 0;
	}
	env->rstate = 0;
	env->remapped_ip = 0;
	env->function_offset = 0;
	(void)uwx_init_history(env);
	env->allocate_cb = uwx_allocate_cb;
	env->free_cb = uwx_free_cb;
	env->free_scoreboards = 0;
	env->used_scoreboards = 0;
	env->labeled_scoreboards = 0;
	(void)uwx_init_str_pool(env);
	env->module_name = 0;
	env->function_name = 0;
	env->cb_token = 0;
	env->copyin = 0;
	env->lookupip = 0;
	env->remote = 0;
	env->byte_swap = 0;
	env->abi_context = 0;
	env->nsbreg = NSBREG_NOFR;
	env->nscoreboards = 0;
	env->trace = 0;
	TRACE_INIT
	for (i = 0; i < NSCOREBOARDS; i++)
	    (void) uwx_alloc_scoreboard(env);
    }
    return env;
}

int uwx_set_remote(struct uwx_env *env, int is_big_endian_target)
{
    int is_big_endian_host;
    char *p;

    if (env == 0)
	return UWX_ERR_NOENV;

    env->remote = 1;

    is_big_endian_host = 1;
    p = (char *)&is_big_endian_host;
    *p = 0;
    if (is_big_endian_target == is_big_endian_host)
	env->byte_swap = 0;
    else
	env->byte_swap = 1;

    return UWX_OK;
}

int uwx_register_callbacks(
    struct uwx_env *env,
    intptr_t tok,
    copyin_cb copyin,
    lookupip_cb lookupip)
{
    if (env == 0)
	return UWX_ERR_NOENV;
    env->cb_token = tok;
    env->copyin = copyin;
    env->lookupip = lookupip;
    return UWX_OK;
}

int uwx_get_abi_context_code(struct uwx_env *env)
{
    if (env == 0)
	return UWX_ERR_NOENV;
    return env->abi_context;
}

int uwx_free(struct uwx_env *env)
{
    if (env != 0) {
	uwx_free_scoreboards(env);
	uwx_free_str_pool(env);
	if (env->free_cb == 0)
	    free((void *)env);
	else
	    (*env->free_cb)((void *)env);
    }
    return UWX_OK;
}
