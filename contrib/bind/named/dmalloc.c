/* dmalloc - debugging layer on top of malloc
 * vix 25mar92 [fixed bug in round-up calcs in alloc()]
 * vix 24mar92 [added size calcs, improved printout]
 * vix 22mar92 [original work]
 *
 * $Id: dmalloc.c,v 8.3 1996/05/17 09:10:46 vixie Exp $
 */

/*
 * ++Copyright++ 1993
 * -
 * Copyright (c) 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <stdio.h>
#include <signal.h>
#include "../conf/portability.h"
#include "../conf/options.h"

#ifdef DMALLOC

#define TRUE 1
#define FALSE 0
typedef	unsigned bool;

#define	MAX_MEMORY	65536	/* must fit in typeof(datum.size) */
#define	MAX_CALLERS	256	/* must be **2 */

typedef struct caller {
	struct caller	*next;
	struct filenam	*file;
	struct calltab	*frees;
	unsigned	line;
	unsigned	calls;
	unsigned	blocks;
	unsigned	bytes;
} caller;

typedef	struct filenam {
	struct filenam	*next;
	char		*name;
} filenam;

typedef	struct calltab {
	struct caller	*callers[MAX_CALLERS];
} calltab;

typedef struct datum {
	unsigned	size;		/* size of malloc'd item */
	unsigned	caller;		/* offset into memory[] */
	/* user data follows */
} datum;

static	char	memory[MAX_MEMORY];
static	char	*nextmem = memory;
static	char	*alloc(size) unsigned size; {
			char *thismem = nextmem;
			int oddness = (size % sizeof(char*));
			if (oddness)
				size += (sizeof(char*) - oddness);
			nextmem += size;
			if (nextmem >= &memory[MAX_MEMORY]) {
				fprintf(stderr, "dmalloc.alloc: out of mem\n");
				kill(0, SIGBUS);
			}
			return thismem;
		}

static	filenam	*Files;
static	calltab	Callers;

/*--------------------------------------------------- imports
 */

#undef	malloc
#undef	calloc
#undef	realloc
#undef	free

char	*malloc(), *calloc(), *realloc();

#if	defined(sun)
int	free();
#else
void	free();
#endif

/*--------------------------------------------------- private
 */

#define	STR_EQ(l,r)	(((l)[0] == (r)[0]) && !strcmp(l, r))

static filenam *
findFile(file, addflag)
	char *file;
	bool addflag;
{
	filenam	*f;

	for (f = Files;  f;  f = f->next)
		if (STR_EQ(file, f->name))
			return f;
	if (!addflag)
		return NULL;
	f = (filenam*) alloc(sizeof(filenam));
	f->next = Files;
	Files = f;
	f->name = alloc(strlen(file) + 1);
	strcpy(f->name, file);
	return f;
}

static caller *
findCaller(ctab, file, line, addflag)
	calltab *ctab;
	char *file;
	unsigned line;
	bool addflag;
{
	unsigned hash = line & (MAX_CALLERS - 1);
	caller *c;

	for (c = ctab->callers[hash];  c;  c = c->next)
		if ((c->line == line) && STR_EQ(c->file->name, file))
			return c;
	if (!addflag)
		return NULL;
	c = (caller*) alloc(sizeof(caller));
	c->next = ctab->callers[hash];
	c->file = findFile(file, TRUE);
	c->line = line;
	c->calls = 0;
	c->frees = (calltab *) alloc(sizeof(calltab));
	ctab->callers[hash] = c;
	return c;
}

/*--------------------------------------------------- public
 */

char *
dmalloc(file, line, size)
	char *file;
	unsigned line;
	unsigned size;
{
	caller *c;
	datum *d;

	c = findCaller(&Callers, file, line, TRUE);
	d = (datum *) malloc(sizeof(datum) + size);
	if (!d)
		return (NULL);
	d->size = size;
	d->caller = ((char *)c) - memory;
	c->calls++;
	c->blocks++;
	c->bytes += size;
	return (char *) (d+1);
}

void
dfree(file, line, ptr)
	char *file;
	unsigned line;
	char *ptr;
{
	caller *c, *a;
	datum *d;

	d = (datum *) ptr;  d--;
	a = (caller *) (memory + d->caller);
	a->bytes -= d->size;
	a->blocks--;
	c = findCaller(a->frees, file, line, TRUE);
	c->calls++;
	free((char*) d);
}

char *
dcalloc(file, line, nelems, elsize)
	char *file;
	unsigned line;
	unsigned nelems, elsize;
{
	unsigned size = (nelems * elsize);
	char *ptr;

	ptr = dmalloc(file, line, size);
	if (ptr)
		bzero(ptr, size);
	return ptr;
}

char *
drealloc(file, line, ptr, size)
	char *file;
	unsigned line;
	char *ptr;
	unsigned size;
{
	caller *c, *a;
	datum *d;

	d = (datum *) ptr;  d--;
	/* fix up stats from allocation */
	a = (caller *) (memory + d->caller);
	a->bytes -= d->size;
	a->blocks--;
	/* we are a "freer" of this allocation */
	c = findCaller(a->frees, file, line, TRUE);
	c->calls++;
	/* get new allocation and stat it */
	c = findCaller(&Callers, file, line, TRUE);
	d = (datum *) realloc((char *) d, sizeof(datum) + size);
	d->size = size;
	d->caller = ((char *)c) - memory;
	c->calls++;
	c->blocks++;
	c->bytes += size;
	return (char *) (d+1);
}

static void
dmalloccallers(outf, prefix, ctab)
	FILE *outf;
	char *prefix;
	calltab *ctab;
{
	/* this bizarre logic is to print all of a file's entries together */
	filenam	*f;

	for (f = Files;  f;  f = f->next) {
		int	i;

		for (i = MAX_CALLERS-1;  i >= 0;  i--) {
			caller *c;

			for (c = ctab->callers[i];  c;  c = c->next) {
				if (f != c->file)
					continue;
				fprintf(outf, "%s\"%s\":%u calls=%u",
					prefix, c->file->name, c->line,
					c->calls);
				if (c->blocks || c->bytes)
					fprintf(outf, " blocks=%u bytes=%u",
						c->blocks, c->bytes);
				fputc('\n', outf);
				if (c->frees)
					dmalloccallers(outf,
						       "\t\t", c->frees);
			}
		}
	}
}

void
dmallocstats(outf)
	FILE *outf;
{
	fprintf(outf, "dallocstats [ private mem used=%u, avail=%u ]\n",
		nextmem - memory, &memory[MAX_MEMORY] - nextmem);
	dmalloccallers(outf, "\t", &Callers);
}

#endif /*DMALLOC*/
