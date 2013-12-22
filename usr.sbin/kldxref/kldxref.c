/*
 * Copyright (c) 2000, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <sys/stat.h>
#include <sys/module.h>
#define FREEBSD_ELF
#include <err.h>
#include <fts.h>
#include <string.h>
#include <machine/elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "ef.h"

#define	MAXRECSIZE	1024
#define check(val)	if ((error = (val)) != 0) break

static int dflag;	/* do not create a hint file, only write on stdout */
static int verbose;

static FILE *fxref;	/* current hints file */

static const char *xref_file = "linker.hints";

/*
 * A record is stored in the static buffer recbuf before going to disk.
 */
static char recbuf[MAXRECSIZE];
static int recpos;	/* current write position */
static int reccnt;	/* total record written to this file so far */

static void
intalign(void)
{
	recpos = (recpos + sizeof(int) - 1) & ~(sizeof(int) - 1);
}

static void
record_start(void)
{
	recpos = 0;
	memset(recbuf, 0, MAXRECSIZE);
}

static int
record_end(void)
{
	if (recpos == 0)
		return 0;
	reccnt++;
	intalign();
	fwrite(&recpos, sizeof(recpos), 1, fxref);
	return fwrite(recbuf, recpos, 1, fxref) != 1 ? errno : 0;
}

static int
record_buf(const void *buf, int size)
{
	if (MAXRECSIZE - recpos < size)
		errx(1, "record buffer overflow");
	memcpy(recbuf + recpos, buf, size);
	recpos += size;
	return 0;
}

/*
 * An int is stored in host order and aligned
 */
static int
record_int(int val)
{
	intalign();
	return record_buf(&val, sizeof(val));
}

/*
 * A string is stored as 1-byte length plus data, no padding
 */
static int
record_string(const char *str)
{
	int len, error;
	u_char val;
	
	if (dflag)
		return 0;
	val = len = strlen(str);
	if (len > 255)
		errx(1, "string %s too long", str);
	error = record_buf(&val, sizeof(val));
	if (error)
		return error;
	return record_buf(str, len);
}

static int
parse_entry(struct mod_metadata *md, const char *cval,
    struct elf_file *ef, const char *kldname)
{
	struct mod_depend mdp;
	struct mod_version mdv;
	Elf_Off data = (Elf_Off)md->md_data;
	int error = 0;

	record_start();
	switch (md->md_type) {
	case MDT_DEPEND:
		if (!dflag)
			break;
		check(EF_SEG_READ(ef, data, sizeof(mdp), &mdp));
		printf("  depends on %s.%d (%d,%d)\n", cval,
		    mdp.md_ver_preferred, mdp.md_ver_minimum, mdp.md_ver_maximum);
		break;
	case MDT_VERSION:
		check(EF_SEG_READ(ef, data, sizeof(mdv), &mdv));
		if (dflag) {
			printf("  interface %s.%d\n", cval, mdv.mv_version);
		} else {
			record_int(MDT_VERSION);
			record_string(cval);
			record_int(mdv.mv_version);
			record_string(kldname);
		}
		break;
	case MDT_MODULE:
		if (dflag) {
			printf("  module %s\n", cval);
		} else {
			record_int(MDT_MODULE);
			record_string(cval);
			record_string(kldname);
		}
		break;
	default:
		warnx("unknown metadata record %d in file %s", md->md_type, kldname);
	}
	if (!error)
		record_end();
	return error;
}

static int
read_kld(char *filename, char *kldname)
{
	struct mod_metadata md;
	struct elf_file ef;
	void **p, **orgp;
	int error, eftype, nmlen;
	long start, finish, entries;
	char kldmodname[MAXMODNAME + 1], cval[MAXMODNAME + 1], *cp;

	if (verbose || dflag)
		printf("%s\n", filename);
	error = ef_open(filename, &ef, verbose);
	if (error) {
		error = ef_obj_open(filename, &ef, verbose);
		if (error) {
			if (verbose)
				warnc(error, "elf_open(%s)", filename);
			return error;
		}
	}
	eftype = EF_GET_TYPE(&ef);
	if (eftype != EFT_KLD && eftype != EFT_KERNEL)  {
		EF_CLOSE(&ef);
		return 0;
	}
	if (!dflag) {
		cp = strrchr(kldname, '.');
		nmlen = (cp != NULL) ? cp - kldname : (int)strlen(kldname);
		if (nmlen > MAXMODNAME)
			nmlen = MAXMODNAME;
		strlcpy(kldmodname, kldname, nmlen);
/*		fprintf(fxref, "%s:%s:%d\n", kldmodname, kldname, 0);*/
	}
	do {
		check(EF_LOOKUP_SET(&ef, MDT_SETNAME, &start, &finish,
		    &entries));
		check(EF_SEG_READ_ENTRY_REL(&ef, start, sizeof(*p) * entries,
		    (void *)&p));
		orgp = p;
		while(entries--) {
			check(EF_SEG_READ_REL(&ef, (Elf_Off)*p, sizeof(md),
			    &md));
			p++;
			check(EF_SEG_READ(&ef, (Elf_Off)md.md_cval,
			    sizeof(cval), cval));
			cval[MAXMODNAME] = '\0';
			parse_entry(&md, cval, &ef, kldname);
		}
		if (error)
			warnc(error, "error while reading %s", filename);
		free(orgp);
	} while(0);
	EF_CLOSE(&ef);
	return error;
}

/*
 * Create a temp file in directory root, make sure we don't
 * overflow the buffer for the destination name
 */
static FILE *
maketempfile(char *dest, const char *root)
{
	char *p;
	int n, fd;

	p = strrchr(root, '/');
	n = p != NULL ? p - root + 1 : 0;
	if (snprintf(dest, MAXPATHLEN, "%.*slhint.XXXXXX", n, root) >=
	    MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	fd = mkstemp(dest);
	if (fd < 0)
		return NULL;
	fchmod(fd, 0644);	/* nothing secret in the file */
	return fdopen(fd, "w+");
}

static char xrefname[MAXPATHLEN], tempname[MAXPATHLEN];

static void
usage(void)
{

	fprintf(stderr, "%s\n",
	    "usage: kldxref [-Rdv] [-f hintsfile] path ..."
	);
	exit(1);
}

static int
compare(const FTSENT *const *a, const FTSENT *const *b)
{
	if ((*a)->fts_info == FTS_D && (*b)->fts_info != FTS_D)
		return 1;
	if ((*a)->fts_info != FTS_D && (*b)->fts_info == FTS_D)
		return -1;
	return strcmp((*a)->fts_name, (*b)->fts_name);
}

int
main(int argc, char *argv[])
{
	FTS *ftsp;
	FTSENT *p;
	int opt, fts_options, ival;
	struct stat sb;

	fts_options = FTS_PHYSICAL;

	while ((opt = getopt(argc, argv, "Rdf:v")) != -1) {
		switch (opt) {
		case 'd':	/* no hint file, only print on stdout */
			dflag = 1;
			break;
		case 'f':	/* use this name instead of linker.hints */
			xref_file = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'R':	/* recurse on directories */
			fts_options |= FTS_COMFOLLOW;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	if (argc - optind < 1)
		usage();
	argc -= optind;
	argv += optind;

	if (stat(argv[0], &sb) != 0)
		err(1, "%s", argv[0]);
	if ((sb.st_mode & S_IFDIR) == 0) {
		errno = ENOTDIR;
		err(1, "%s", argv[0]);
	}

	ftsp = fts_open(argv, fts_options, compare);
	if (ftsp == NULL)
		exit(1);

	for (;;) {
		p = fts_read(ftsp);
		if ((p == NULL || p->fts_info == FTS_D) && fxref) {
			/* close and rename the current hint file */
			fclose(fxref);
			fxref = NULL;
			if (reccnt) {
				rename(tempname, xrefname);
			} else {
				/* didn't find any entry, ignore this file */
				unlink(tempname);
				unlink(xrefname);
			}
		}
		if (p == NULL)
			break;
		if (p->fts_info == FTS_D && !dflag) {
			/* visiting a new directory, create a new hint file */
			snprintf(xrefname, sizeof(xrefname), "%s/%s",
			    ftsp->fts_path, xref_file);
			fxref = maketempfile(tempname, ftsp->fts_path);
			if (fxref == NULL)
				err(1, "can't create %s", tempname);
			ival = 1;
			fwrite(&ival, sizeof(ival), 1, fxref);
			reccnt = 0;
		}
		/* skip non-files or .symbols entries */
		if (p->fts_info != FTS_F)
			continue;
		if (p->fts_namelen >= 8 &&
		    strcmp(p->fts_name + p->fts_namelen - 8, ".symbols") == 0)
			continue;
		read_kld(p->fts_path, p->fts_name);
	}
	fts_close(ftsp);
	return 0;
}
