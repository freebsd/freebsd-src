/*
Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
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
#include "uwx_trace.h"

#ifdef _KERNEL
static unsigned short uwx_allocated;
static struct uwx_scoreboard uwx_scoreboard[sizeof(uwx_allocated) << 3];

static void
free(struct uwx_scoreboard *p)
{
	int idx = p - uwx_scoreboard;
	uwx_allocated &= ~(1 << idx);
}

static struct uwx_scoreboard *
malloc(size_t sz)
{
	int idx;
	if (sz != sizeof(struct uwx_scoreboard))
		return (NULL);
	for (idx = 0; idx < (sizeof(uwx_allocated) << 3); idx++) {
		if ((uwx_allocated & (1 << idx)) == 0) {
			uwx_allocated |= 1 << idx;
			return (uwx_scoreboard + idx);
		}
	}
	return (NULL);
}
#endif


void uwx_prealloc_scoreboard(struct uwx_env *env, struct uwx_scoreboard *sb)
{
    sb->id = env->nscoreboards++;
    sb->nextused = env->used_scoreboards;
    sb->prealloc = 1;
    env->used_scoreboards = sb;
    TRACE_B_PREALLOC(sb->id)
}

struct uwx_scoreboard *uwx_alloc_scoreboard(struct uwx_env *env)
{
    struct uwx_scoreboard *sb;
    int i;

    if (env->free_scoreboards != 0) {
	sb = env->free_scoreboards;
	env->free_scoreboards = sb->nextfree;
	TRACE_B_REUSE(sb->id)
    }
    else {
	if (env->allocate_cb == 0)
	    sb = (struct uwx_scoreboard *)
			malloc(sizeof(struct uwx_scoreboard));
	else
	    sb = (struct uwx_scoreboard *)
			(*env->allocate_cb)(sizeof(struct uwx_scoreboard));
	if (sb == 0)
	    return 0;
	sb->id = env->nscoreboards++;
	sb->nextused = env->used_scoreboards;
	sb->prealloc = 0;
	env->used_scoreboards = sb;
	TRACE_B_ALLOC(sb->id)
    }

    sb->nextstack = 0;
    sb->nextlabel = 0;
    for (i = 0; i < env->nsbreg; i++)
	sb->rstate[i] = UWX_DISP_NONE;
    sb->rstate[SBREG_RP] = UWX_DISP_REG(UWX_REG_BR(0));
    sb->rstate[SBREG_PSP] = UWX_DISP_SPPLUS(0);
    sb->rstate[SBREG_PFS] = UWX_DISP_REG(UWX_REG_AR_PFS);
    sb->rstate[SBREG_PRIUNAT] = UWX_DISP_REG(UWX_REG_AR_UNAT);
    sb->label = 0;
    return sb;
}

static
void uwx_reclaim_scoreboards(struct uwx_env *env)
{
    struct uwx_scoreboard *sb;

    env->free_scoreboards = 0;
    for (sb = env->used_scoreboards; sb != 0; sb = sb->nextused) {
	sb->nextfree = env->free_scoreboards;
	env->free_scoreboards = sb;
    }
    env->labeled_scoreboards = 0;
}

struct uwx_scoreboard *uwx_init_scoreboards(struct uwx_env *env)
{
    struct uwx_scoreboard *sb;

    uwx_reclaim_scoreboards(env);
    sb = uwx_alloc_scoreboard(env);
    return sb;
}

struct uwx_scoreboard *uwx_new_scoreboard(
    struct uwx_env *env,
    struct uwx_scoreboard *prevsb)
{
    int i;
    struct uwx_scoreboard *sb;

    sb = uwx_alloc_scoreboard(env);
    if (sb == 0)
	return 0;
    sb->nextstack = prevsb;
    for (i = 0; i < env->nsbreg; i++)
	sb->rstate[i] = prevsb->rstate[i];
    return sb;
}

struct uwx_scoreboard *uwx_pop_scoreboards(
    struct uwx_env *env,
    struct uwx_scoreboard *sb,
    int ecount)
{
    struct uwx_scoreboard *next;

    while (ecount > 0) {
	next = sb->nextstack;
	TRACE_B_POP(sb->id)
	sb->nextstack = 0;
	sb->nextfree = env->free_scoreboards;
	env->free_scoreboards = sb;
	sb = next;
	if (sb == 0)
	    return 0;
	ecount--;
    }
    return sb;
}

int uwx_label_scoreboard(
    struct uwx_env *env,
    struct uwx_scoreboard *sb,
    int label)
{
    struct uwx_scoreboard *new;
    struct uwx_scoreboard *back;
    struct uwx_scoreboard *next;
    int i;

    TRACE_B_LABEL(label)

    /* Copy the current stack, storing reverse links */
    /* in the "nextstack" field. */

    back = 0;
    new = 0;
    while (sb != 0) {
	TRACE_B_LABEL_COPY(sb->id)
	new = uwx_alloc_scoreboard(env);
	if (new == 0)
	    return UWX_ERR_NOMEM;
	new->nextstack = back;
	for (i = 0; i < env->nsbreg; i++)
	    new->rstate[i] = sb->rstate[i];
	sb = sb->nextstack;
	back = new;
    }

    /* The "new" pointer now points to the bottom of the new stack, */
    /* and the "nextstack" links lead towards the top. */
    /* Now go back down the stack, reversing the stack links to their */
    /* proper direction. */

    back = 0;
    while (new != 0) {
	next = new->nextstack;
	new->nextstack = back;
	TRACE_B_LABEL_REVERSE(back, new)
	back = new;
	new = next;
    }

    /* The "back" pointer now points to the top of the stack. */

    back->label = label;
    back->nextlabel = env->labeled_scoreboards;
    env->labeled_scoreboards = back;
    return UWX_OK;
}

int uwx_copy_scoreboard(
    struct uwx_env *env,
    struct uwx_scoreboard *sb,
    int label)
{
    struct uwx_scoreboard *next;
    struct uwx_scoreboard *next2;
    struct uwx_scoreboard *lsb;
    struct uwx_scoreboard *new;
    struct uwx_scoreboard *back;
    int i;

    TRACE_B_COPY(label, sb->id)

    /* Free the existing stack. */

    next = sb->nextstack;
    while (next != 0) {
	TRACE_B_COPY_FREE(next->id)
	next2 = next->nextstack;
	next->nextstack = 0;
	next->nextfree = env->free_scoreboards;
	env->free_scoreboards = next;
	next = next2;
    }

    /* Find the scoreboard with the requested label. */

    for (lsb = env->labeled_scoreboards; lsb != 0; lsb = lsb->nextlabel) {
	if (lsb->label == label)
	    break;
    }

    if (lsb == 0)
	return UWX_ERR_UNDEFLABEL;

    TRACE_B_COPY_FOUND(lsb->id)

    /* Copy the labeled scoreboard. */

    sb->nextstack = 0;
    sb->nextlabel = 0;
    for (i = 0; i < env->nsbreg; i++)
	sb->rstate[i] = lsb->rstate[i];
    sb->label = 0;

    /* Now copy its stack, storing reverse links in the nextstack field. */

    back = sb;
    new = 0;
    for (next = lsb->nextstack; next != 0; next = next->nextstack) {
	TRACE_B_COPY_COPY(next->id)
	new = uwx_alloc_scoreboard(env);
	if (new == 0)
	    return UWX_ERR_NOMEM;
	new->nextstack = back;
	for (i = 0; i < env->nsbreg; i++)
	    new->rstate[i] = next->rstate[i];
	back = new;
    }

    /* The "new" pointer now points to the bottom of the new stack, */
    /* and the "nextstack" links lead towards the top. */
    /* Now go back down the stack, reversing the nextstack links to their */
    /* proper direction. */

    back = 0;
    while (new != 0) {
	next = new->nextstack;
	new->nextstack = back;
	TRACE_B_COPY_REVERSE(back, new)
	back = new;
	new = next;
    }

    return UWX_OK;
}

void uwx_free_scoreboards(struct uwx_env *env)
{
    struct uwx_scoreboard *sb;
    struct uwx_scoreboard *next;

    for (sb = env->used_scoreboards; sb != 0; sb = next) {
	TRACE_B_FREE(sb->id)
	next = sb->nextused;
	if (!sb->prealloc) {
	    if (env->free_cb == 0)
		free((void *)sb);
	    else
		(*env->free_cb)((void *)sb);
	}
    }
    env->free_scoreboards = 0;
    env->used_scoreboards = 0;
    env->labeled_scoreboards = 0;
}

