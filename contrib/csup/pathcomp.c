/*-
 * Copyright (c) 2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "pathcomp.h"

struct pathcomp {
	char *target;
	size_t targetlen;
	char *trashed;
	char *prev;
	size_t prevlen;
	size_t goal;
	size_t curlen;
};

struct pathcomp	*
pathcomp_new(void)
{
	struct pathcomp *pc;

	pc = xmalloc(sizeof(struct pathcomp));
	pc->curlen = 0;
	pc->target = NULL;
	pc->targetlen = 0;
	pc->trashed = NULL;
	pc->prev = NULL;
	pc->prevlen = 0;
	return (pc);
}

int
pathcomp_put(struct pathcomp *pc, int type, char *path)
{
	char *cp;

	assert(pc->target == NULL);
	if (*path == '/')
		return (-1);

	switch (type) {
	case PC_DIRDOWN:
		pc->target = path;
		pc->targetlen = strlen(path);
		break;
	case PC_FILE:
	case PC_DIRUP:
		cp = strrchr(path, '/');
		pc->target = path;
		if (cp != NULL)
			pc->targetlen = cp - path;
		else
			pc->targetlen = 0;
		break;
	}
	if (pc->prev != NULL)
		pc->goal = commonpathlength(pc->prev, pc->prevlen, pc->target,
		    pc->targetlen);
	else
		pc->goal = 0;
	if (pc->curlen == pc->goal)	/* No need to go up. */
		pc->goal = pc->targetlen;
	return (0);
}

int
pathcomp_get(struct pathcomp *pc, int *type, char **name)
{
	char *cp;
	size_t slashpos, start;

	if (pc->curlen > pc->goal) {		/* Going up. */
		assert(pc->prev != NULL);
		pc->prev[pc->curlen] = '\0';
		cp = pc->prev + pc->curlen - 1;
		while (cp >= pc->prev) {
			if (*cp == '/')
				break;
			cp--;
		}
		if (cp >= pc->prev)
			slashpos = cp - pc->prev;
		else
			slashpos = 0;
		pc->curlen = slashpos;
		if (pc->curlen <= pc->goal) {	/* Done going up. */
			assert(pc->curlen == pc->goal);
			pc->goal = pc->targetlen;
		}
		*type = PC_DIRUP;
		*name = pc->prev;
		return (1);
	} else if (pc->curlen < pc->goal) {	/* Going down. */
		/* Restore the previously overwritten '/' character. */
		if (pc->trashed != NULL) {
			*pc->trashed = '/';
			pc->trashed = NULL;
		}
		if (pc->curlen == 0)
			start = pc->curlen;
		else
			start = pc->curlen + 1;
		slashpos = start;
		while (slashpos < pc->goal) {
			if (pc->target[slashpos] == '/')
				break;
			slashpos++;
		}
		if (pc->target[slashpos] != '\0') {
			assert(pc->target[slashpos] == '/');
			pc->trashed = pc->target + slashpos;
			pc->target[slashpos] = '\0';
		}
		pc->curlen = slashpos;
		*type = PC_DIRDOWN;
		*name = pc->target;
		return (1);
	} else {	/* Done. */
		if (pc->target != NULL) {
			if (pc->trashed != NULL) {
				*pc->trashed = '/';
				pc->trashed = NULL;
			}
			if (pc->prev != NULL)
				free(pc->prev);
			pc->prev = xmalloc(pc->targetlen + 1);
			memcpy(pc->prev, pc->target, pc->targetlen);
			pc->prev[pc->targetlen] = '\0';
			pc->prevlen = pc->targetlen;
			pc->target = NULL;
			pc->targetlen = 0;
		}
		return (0);
	}
}

void
pathcomp_finish(struct pathcomp *pc)
{

	pc->target = NULL;
	pc->targetlen = 0;
	pc->goal = 0;
}

void
pathcomp_free(struct pathcomp *pc)
{

	if (pc->prev != NULL)
		free(pc->prev);
	free(pc);
}
