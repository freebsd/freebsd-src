/*
 * Copyright (c) 1980 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#ifndef lint
char const copyright[] =
"@(#) Copyright (c) 1980 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)symorder.c	5.8 (Berkeley) 4/1/91";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

/*
 * symorder - reorder symbol table
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <a.out.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SPACE		500

#define	OKEXIT		0
#define	NOTFOUNDEXIT	1
#define	ERREXIT		2

char	*exclude[SPACE];
struct	nlist order[SPACE];

struct	exec exec;
struct	stat stb;
struct	nlist *newtab, *symtab;
off_t	sa;
int	nexclude, nsym, strtabsize, symfound, symkept, small, missing, clean;
char	*kfile, *newstrings, *strings, asym[BUFSIZ];

void badfmt __P((char *));
int excluded __P((struct nlist *));
int inlist __P((struct nlist *));
void reorder __P((struct nlist *, struct nlist *, int));
int savesymb __P((struct nlist *));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	register struct nlist *p, *symp;
	register FILE *f, *xfile;
	register int i;
	register char *start, *t, *xfilename;
	int ch, n, o;

	xfilename = NULL;
	while ((ch = getopt(argc, argv, "cmtx:")) != EOF)
		switch(ch) {
		case 'c':
			clean = 1;
			break;
		case 'm':
			missing = 1;
			break;
		case 't':
			small = 1;
			break;
		case 'x':
			if (xfilename != NULL)
				usage();
			xfilename = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	if ((f = fopen(argv[0], "r")) == NULL)
		err(ERREXIT, "%s", argv[0]);

	for (p = order; fgets(asym, sizeof(asym), f) != NULL;) {
		for (t = asym; isspace(*t); ++t);
		if (!*(start = t))
			continue;
		while (*++t);
		if (*--t == '\n')
			*t = '\0';
		p->n_un.n_name = strdup(start);
		++p;
		if (++nsym >= sizeof order / sizeof order[0])
			break;
	}
	(void)fclose(f);

	if (xfilename != NULL) {
		if ((xfile = fopen(xfilename, "r")) == NULL)
			err(ERREXIT, "%s", xfilename);
		for (; fgets(asym, sizeof(asym), xfile) != NULL;) {
			for (t = asym; isspace(*t); ++t);
			if (!*(start = t))
				continue;
			while (*++t);
			if (*--t == '\n')
				*t = '\0';
			exclude[nexclude] = strdup(start);
			if (++nexclude >= sizeof exclude / sizeof exclude[0])
				break;
		}
		(void)fclose(xfile);
	}

	kfile = argv[1];
	if ((f = fopen(kfile, "r")) == NULL)
		err(ERREXIT, "%s", kfile);
	if ((o = open(kfile, O_WRONLY)) < 0)
		err(ERREXIT, "%s", kfile);

	/* read exec header */
	if ((fread(&exec, sizeof(exec), 1, f)) != 1)
		badfmt("no exec header");
	if (N_BADMAG(exec))
		badfmt("bad magic number");
	if (exec.a_syms == 0)
		badfmt("stripped");
	(void)fstat(fileno(f), &stb);
	if (stb.st_size < N_STROFF(exec) + sizeof(off_t))
		badfmt("no string table");

	/* seek to and read the symbol table */
	sa = N_SYMOFF(exec);
	(void)fseek(f, sa, SEEK_SET);
	n = exec.a_syms;
	if (!(symtab = (struct nlist *)malloc(n)))
		err(ERREXIT, NULL);
	if (fread((void *)symtab, 1, n, f) != n)
		badfmt("corrupted symbol table");

	/* read string table size and string table */
	if (fread((void *)&strtabsize, sizeof(int), 1, f) != 1 ||
	    strtabsize <= 0)
		badfmt("corrupted string table");
	strings = malloc(strtabsize);
	if (strings == NULL)
		err(ERREXIT, NULL);
	/*
	 * Subtract four from strtabsize since strtabsize includes itself,
	 * and we've already read it.
	 */
	if (fread(strings, 1, strtabsize - sizeof(int), f) !=
	    strtabsize - sizeof(int))
		badfmt("corrupted string table");

	i = n / sizeof(struct nlist);
	if (!clean) {
		newtab = (struct nlist *)malloc(n);
		if (newtab == (struct nlist *)NULL)
			err(ERREXIT, NULL);
		memset(newtab, 0, n);

		reorder(symtab, newtab, i);
		free((void *)symtab);
		symtab = newtab;
	} else {
		symkept = i;
	}

	newstrings = malloc(strtabsize);
	if (newstrings == NULL)
		err(ERREXIT, NULL);
	t = newstrings;
	for (symp = symtab; --i >= 0; symp++) {
		if (symp->n_un.n_strx == 0)
			continue;
		if (inlist(symp) < 0) {
			if (small)
				continue;
			if (clean && !savesymb(symp))
				symp->n_type &= ~N_EXT;
		} else if (clean)
			symfound++;
		symp->n_un.n_strx -= sizeof(int);
		(void)strcpy(t, &strings[symp->n_un.n_strx]);
		symp->n_un.n_strx = (t - newstrings) + sizeof(int);
		t += strlen(t) + 1;
	}

	/* update shrunk sizes */
	strtabsize = t - newstrings + sizeof(int);
	n = symkept * sizeof(struct nlist);

	/* fix exec sym size */
	(void)lseek(o, (off_t)0, SEEK_SET);
	exec.a_syms = n;
	if (write(o, (void *)&exec, sizeof(exec)) != sizeof(exec))
		err(ERREXIT, "%s", kfile);

	(void)lseek(o, sa, SEEK_SET);
	if (write(o, (void *)symtab, n) != n)
		err(ERREXIT, "%s", kfile);
	if (write(o, (void *)&strtabsize, sizeof(int)) != sizeof(int))
		err(ERREXIT, "%s", kfile);
	if (write(o, newstrings, strtabsize - sizeof(int)) !=
	    strtabsize - sizeof(int))
		err(ERREXIT, "%s", kfile);

	ftruncate(o, lseek(o, (off_t)0, SEEK_CUR));

	if ((i = nsym - symfound) > 0) {
		(void)printf("symorder: %d symbol%s not found:\n",
		    i, i == 1 ? "" : "s");
		for (i = 0; i < nsym; i++)
			if (order[i].n_value == 0)
				printf("%s\n", order[i].n_un.n_name);
		if (!missing)
			exit(NOTFOUNDEXIT);
	}
	exit(OKEXIT);
}

int
savesymb(s)
	register struct nlist *s;
{
	if ((s->n_type & N_EXT) != N_EXT)
		return 0;
	switch (s->n_type & N_TYPE) {
		case N_TEXT:
		case N_DATA:	
			return 0;
		default:	
			return 1;
	}
}

void
reorder(st1, st2, entries)
	register struct nlist *st1, *st2;
	int entries;
{
	register struct nlist *p;
	register int i, n;

	for (p = st1, n = entries; --n >= 0; ++p)
		if (inlist(p) != -1)
			++symfound;
	for (p = st2 + symfound, n = entries; --n >= 0; ++st1) {
		if (excluded(st1))
			continue;
		i = inlist(st1);
		if (i == -1)
			*p++ = *st1;
		else
			st2[i] = *st1;
		++symkept;
	}
}

int
inlist(p)
	register struct nlist *p;
{
	register char *nam;
	register struct nlist *op;

	if (p->n_type & N_STAB || p->n_un.n_strx == 0)
		return (-1);
	if (p->n_un.n_strx < sizeof(int) || p->n_un.n_strx >= strtabsize)
		badfmt("corrupted symbol table");
	nam = &strings[p->n_un.n_strx - sizeof(int)];
	for (op = &order[nsym]; --op >= order; ) {
		if (strcmp(op->n_un.n_name, nam) != 0)
			continue;
		op->n_value = 1;
		return (op - order);
	}
	return (-1);
}

int
excluded(p)
	register struct nlist *p;
{
	register char *nam;
	register int x;

	if (p->n_type & N_STAB || p->n_un.n_strx == 0)
		return (0);
	if (p->n_un.n_strx < sizeof(int) || p->n_un.n_strx >= strtabsize)
		badfmt("corrupted symbol table");
	nam = &strings[p->n_un.n_strx - sizeof(int)];
	for (x = nexclude; --x >= 0; )
		if (strcmp(nam, exclude[x]) == 0)
			return (1);
	return (0);
}

void
badfmt(why)
	char *why;
{
	errx(ERREXIT, "%s: %s: %s", kfile, why, strerror(EFTYPE));
}

static void
usage()
{
	(void)fprintf(stderr,
	    "usage: symorder [-c] [-m] [-t] [-x excludelist] symlist file\n");
	exit(ERREXIT);
}
