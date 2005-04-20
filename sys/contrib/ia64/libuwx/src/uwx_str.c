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

#include "uwx_env.h"
#include "uwx_str.h"

#ifdef _KERNEL
static struct uwx_str_pool	uwx_str_pool;
#define	free(p)		/* nullified */
#define	malloc(sz)	((sz == sizeof(uwx_str_pool)) ? &uwx_str_pool : NULL)
#endif

/*
 *  uwx_str.c
 *
 *  This file contains the routines for maintaining a string
 *  pool for the unwind environment. We preallocate enough
 *  space for most purposes so that no memory allocation is
 *  necessary during a normal unwind. If we do need more,
 *  we use the allocate callback, if one is provided.
 *
 *  The string pool is reused with each call to step(),
 *  and is completely freed when the unwind environment is
 *  freed.
 */


int uwx_init_str_pool(struct uwx_env *env)
{
    if (env->allocate_cb == 0)
	env->string_pool = (struct uwx_str_pool *)
		malloc(sizeof(struct uwx_str_pool));
    else
	env->string_pool = (struct uwx_str_pool *)
		(*env->allocate_cb)(sizeof(struct uwx_str_pool));

    if (env->string_pool == 0)
	return UWX_ERR_NOMEM;

    env->string_pool->next = 0;
    env->string_pool->size = STRPOOLSIZE;
    env->string_pool->used = 0;

    return UWX_OK;
}

void uwx_free_str_pool(struct uwx_env *env)
{
    struct uwx_str_pool *pool;
    struct uwx_str_pool *next;

    for (pool = env->string_pool; pool != 0; pool = next) {
	next = pool->next;
	if (env->free_cb == 0)
	    free(pool);
	else
	    (*env->free_cb)(pool);
    }
}

char *uwx_alloc_str(struct uwx_env *env, char *str)
{
    int len;
    int size;
    struct uwx_str_pool *pool;
    struct uwx_str_pool *prev;
    char *p;

    len = strlen(str) + 1;
    prev = 0;
    for (pool = env->string_pool; pool != 0; pool = pool->next) {
	prev = pool;
	if (pool->size - pool->used >= len)
	    break;
    }
    if (pool == 0) {
	size = STRPOOLSIZE;
	if (len > size)
	    size = len;
	size += sizeof(struct uwx_str_pool) - STRPOOLSIZE;
	if (env->allocate_cb == 0)
	    pool = (struct uwx_str_pool *) malloc(size);
	else
	    pool = (struct uwx_str_pool *) (*env->allocate_cb)(size);
	if (env->string_pool == 0)
	    return 0;
	pool->next = 0;
	pool->size = size;
	pool->used = 0;
	prev->next = pool;
    }
    p = pool->pool + pool->used;
    strcpy(p, str);
    pool->used += len;
    return p;
}

void uwx_reset_str_pool(struct uwx_env *env)
{
    struct uwx_str_pool *pool;

    for (pool = env->string_pool; pool != 0; pool = pool->next)
	pool->used = 0;
}
