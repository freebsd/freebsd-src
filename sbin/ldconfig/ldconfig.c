/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: ldconfig.c,v 1.2 1993/11/09 04:19:22 paul Exp $
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#include <dirent.h>

#include "ld.h"

#undef major
#undef minor

char				*progname;
static int			verbose;
static int			nostd;
static int			justread;

struct shlib_list {
	/* Internal list of shared libraries found */
	char			*name;
	char			*path;
	int			dewey[MAXDEWEY];
	int			ndewey;
#define major dewey[0]
#define minor dewey[1]
	struct shlib_list	*next;
};

static struct shlib_list	*shlib_head = NULL, **shlib_tail = &shlib_head;

static void	enter __P((char *, char *, char *, int *, int));
static int	dodir __P((char *));
static int	build_hints __P((void));

int
main(argc, argv)
int	argc;
char	*argv[];
{
	int		i, c;
	int		rval = 0;
	extern int	optind;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	while ((c = getopt(argc, argv, "rsv")) != EOF) {
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 's':
			nostd = 1;
			break;
		case 'r':
			justread = 1;
			break;
		default:
			fprintf(stderr, "Usage: %s [-v] [dir ...]\n", progname);
			exit(1);
			break;
		}
	}

	if (justread)
		return listhints();

	if (!nostd)
		std_search_dirs(NULL);

	for (i = 0; i < n_search_dirs; i++)
		rval |= dodir(search_dirs[i]);

	for (i = optind; i < argc; i++)
		rval |= dodir(argv[i]);

	rval |= build_hints();

	return rval;
}

int
dodir(dir)
char	*dir;
{
	DIR		*dd;
	struct dirent	*dp;
	char		name[MAXPATHLEN], rest[MAXPATHLEN];
	int		dewey[MAXDEWEY], ndewey;

	if ((dd = opendir(dir)) == NULL) {
		perror(dir);
		return -1;
	}

	while ((dp = readdir(dd)) != NULL) {
		int	n;

		name[0] = rest[0] = '\0';

		n = sscanf(dp->d_name, "lib%[^.].so.%s",
					name, rest);

		if (n < 2 || rest[0] == '\0')
			continue;

		ndewey = getdewey(dewey, rest);
		enter(dir, dp->d_name, name, dewey, ndewey);
	}

	return 0;
}

static void
enter(dir, file, name, dewey, ndewey)
char	*dir, *file, *name;
int	dewey[], ndewey;
{
	struct shlib_list	*shp;

	for (shp = shlib_head; shp; shp = shp->next) {
		if (strcmp(name, shp->name) != 0 || major != shp->major)
			continue;

		/* Name matches existing entry */
		if (cmpndewey(dewey, ndewey, shp->dewey, shp->ndewey) > 0) {

			/* Update this entry with higher versioned lib */
			if (verbose)
				printf("Updating lib%s.%d.%d to %s/%s\n",
					shp->name, shp->major, shp->minor,
					dir, file);

			free(shp->name);
			shp->name = strdup(name);
			free(shp->path);
			shp->path = concat(dir, "/", file);
			bcopy(dewey, shp->dewey, sizeof(shp->dewey));
			shp->ndewey = ndewey;
		}
		break;
	}

	if (shp)
		/* Name exists: older version or just updated */
		return;

	/* Allocate new list element */
	if (verbose)
		printf("Adding %s/%s\n", dir, file);

	shp = (struct shlib_list *)xmalloc(sizeof *shp);
	shp->name = strdup(name);
	shp->path = concat(dir, "/", file);
	bcopy(dewey, shp->dewey, MAXDEWEY);
	shp->ndewey = ndewey;
	shp->next = NULL;

	*shlib_tail = shp;
	shlib_tail = &shp->next;
}


#if DEBUG
/* test */
#undef _PATH_LD_HINTS
#define _PATH_LD_HINTS		"./ld.so.hints"
#endif

int
hinthash(cp, vmajor, vminor)
char	*cp;
int	vmajor, vminor;
{
	int	k = 0;

	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	k = (((k << 1) + (k >> 14)) ^ (vmajor*257)) & 0x3fff;
	k = (((k << 1) + (k >> 14)) ^ (vminor*167)) & 0x3fff;

	return k;
}

int
build_hints()
{
	struct hints_header	hdr;
	struct hints_bucket	*blist;
	struct shlib_list	*shp;
	char			*strtab;
	int			i, n, str_index = 0;
	int			strtab_sz = 0;	/* Total length of strings */
	int			nhints = 0;	/* Total number of hints */
	int			fd;
	char			*tmpfile;

	for (shp = shlib_head; shp; shp = shp->next) {
		strtab_sz += 1 + strlen(shp->name);
		strtab_sz += 1 + strlen(shp->path);
		nhints++;
	}

	/* Fill hints file header */
	hdr.hh_magic = HH_MAGIC;
	hdr.hh_version = LD_HINTS_VERSION_1;
	hdr.hh_nbucket = 1 * nhints;
	n = hdr.hh_nbucket * sizeof(struct hints_bucket);
	hdr.hh_hashtab = sizeof(struct hints_header);
	hdr.hh_strtab = hdr.hh_hashtab + n;
	hdr.hh_strtab_sz = strtab_sz;
	hdr.hh_ehints = hdr.hh_strtab + hdr.hh_strtab_sz;

	if (verbose)
		printf("Totals: entries %d, buckets %d, string size %d\n",
					nhints, hdr.hh_nbucket, strtab_sz);

	/* Allocate buckets and string table */
	blist = (struct hints_bucket *)xmalloc(n);
	bzero((char *)blist, n);
	for (i = 0; i < hdr.hh_nbucket; i++)
		/* Empty all buckets */
		blist[i].hi_next = -1;

	strtab = (char *)xmalloc(strtab_sz);

	/* Enter all */
	for (shp = shlib_head; shp; shp = shp->next) {
		struct hints_bucket	*bp;

		bp = blist +
		  (hinthash(shp->name, shp->major, shp->minor) % hdr.hh_nbucket);

		if (bp->hi_pathx) {
			int	i;

			for (i = 0; i < hdr.hh_nbucket; i++) {
				if (blist[i].hi_pathx == 0)
					break;
			}
			if (i == hdr.hh_nbucket) {
				fprintf(stderr, "Bummer!\n");
				return -1;
			}
			while (bp->hi_next != -1)
				bp = &blist[bp->hi_next];
			bp->hi_next = i;
			bp = blist + i;
		}

		/* Insert strings in string table */
		bp->hi_namex = str_index;
		strcpy(strtab + str_index, shp->name);
		str_index += 1 + strlen(shp->name);

		bp->hi_pathx = str_index;
		strcpy(strtab + str_index, shp->path);
		str_index += 1 + strlen(shp->path);

		/* Copy versions */
		bcopy(shp->dewey, bp->hi_dewey, sizeof(bp->hi_dewey));
		bp->hi_ndewey = shp->ndewey;
	}

	tmpfile = concat(_PATH_LD_HINTS, "+", "");
	if ((fd = open(tmpfile, O_RDWR|O_CREAT|O_TRUNC, 0444)) == -1) {
		perror(_PATH_LD_HINTS);
		return -1;
	}

	mywrite(&hdr, 1, sizeof(struct hints_header), fd);
	mywrite(blist, hdr.hh_nbucket, sizeof(struct hints_bucket), fd);
	mywrite(strtab, strtab_sz, 1, fd);

	if (close(fd) != 0) {
		perror(_PATH_LD_HINTS);
		return -1;
	}

	/* Now, install real file */
	if (unlink(_PATH_LD_HINTS) != 0 && errno != ENOENT) {
		perror(_PATH_LD_HINTS);
		return -1;
	}

	if (rename(tmpfile, _PATH_LD_HINTS) != 0) {
		perror(_PATH_LD_HINTS);
		return -1;
	}

	return 0;
}

int
listhints()
{
	int			fd;
	caddr_t			addr;
	long			msize;
	struct hints_header	*hdr;
	struct hints_bucket	*blist;
	char			*strtab;
	int			i;

	if ((fd = open(_PATH_LD_HINTS, O_RDONLY, 0)) == -1) {
		perror(_PATH_LD_HINTS);
		return -1;
	}

	msize = PAGSIZ;
	addr = mmap(0, msize, PROT_READ, MAP_FILE|MAP_COPY, fd, 0);

	if (addr == (caddr_t)-1) {
		perror(_PATH_LD_HINTS);
		return -1;
	}

	hdr = (struct hints_header *)addr;
	if (HH_BADMAG(*hdr)) {
		fprintf(stderr, "%s: Bad magic: %d\n");
		return -1;
	}

	if (hdr->hh_version != LD_HINTS_VERSION_1) {
		fprintf(stderr, "Unsupported version: %d\n", hdr->hh_version);
		return -1;
	}

	if (hdr->hh_ehints > msize) {
		if (mmap(addr+msize, hdr->hh_ehints - msize,
				PROT_READ, MAP_FILE|MAP_COPY|MAP_FIXED,
				fd, msize) != (caddr_t)(addr+msize)) {

			perror(_PATH_LD_HINTS);
			return -1;
		}
	}
	close(fd);

	blist = (struct hints_bucket *)(addr + hdr->hh_hashtab);
	strtab = (char *)(addr + hdr->hh_strtab);

	printf("%s:\n", _PATH_LD_HINTS);
	for (i = 0; i < hdr->hh_nbucket; i++) {
		struct hints_bucket	*bp = &blist[i];

		/* Sanity check */
		if (bp->hi_namex >= hdr->hh_strtab_sz) {
			fprintf(stderr, "Bad name index: %#x\n", bp->hi_namex);
			return -1;
		}
		if (bp->hi_pathx >= hdr->hh_strtab_sz) {
			fprintf(stderr, "Bad path index: %#x\n", bp->hi_pathx);
			return -1;
		}

		printf("\t%d:-l%s.%d.%d => %s (%d -> %d)\n",
			i,
			strtab + bp->hi_namex, bp->hi_major, bp->hi_minor,
			strtab + bp->hi_pathx,
			hinthash(strtab+bp->hi_namex, bp->hi_major, bp->hi_minor)
					% hdr->hh_nbucket,
			bp->hi_next);
	}

	return 0;
}

