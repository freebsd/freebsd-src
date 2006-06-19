/*-
 * Copyright (c) 2002, 2003 Greg Lehey
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
 * This software is provided by the author ``as is'' and any express
 * or implied warranties, including, but not limited to, the implied
 * warranties of merchantability and fitness for a particular purpose
 * are disclaimed.  In no event shall the author be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including
 * negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 */
/* $Id: asf.c,v 1.4 2003/05/04 02:55:20 grog Exp grog $ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asf.h"

struct kfile {
    char	       *name;
    caddr_t		addr;
    int			seen;
    STAILQ_ENTRY(kfile)	link;
};

static STAILQ_HEAD(,kfile) kfile_head = STAILQ_HEAD_INITIALIZER(kfile_head);

void
kfile_add(const char *name, caddr_t addr)
{
    struct kfile *kfp;

    if ((kfp = malloc(sizeof(*kfp))) == NULL ||
	(kfp->name = strdup(name)) == NULL)
	    errx(2, "out of memory");
    kfp->addr = addr;
    kfp->seen = 0;
    STAILQ_INSERT_TAIL(&kfile_head, kfp, link);
}

static struct kfile *
kfile_find(const char *name)
{
    struct kfile *kfp;

    STAILQ_FOREACH(kfp, &kfile_head, link)
	if (strcmp(kfp->name, name) == 0)
	    return (kfp);	/* found */

    return (NULL);		/* not found */
}

static int
kfile_allseen(void)
{
    struct kfile *kfp;

    STAILQ_FOREACH(kfp, &kfile_head, link)
	if (!kfp->seen)
	    return (0);	/* at least one unseen */

    return (1);		/* all seen */
}

static int
kfile_empty(void)
{
    return (STAILQ_EMPTY(&kfile_head));
}

/*
 * Take a blank separated list of tokens and turn it into a list of
 * individual nul-delimited strings.  Build a list of pointers at
 * token, which must have enough space for the tokens.  Return the
 * number of tokens, or -1 on error (typically a missing string
 * delimiter).
 */
int
tokenize(char *cptr, char *token[], int maxtoken)
{
    char delim;				/* delimiter to search for */
    int tokennr;			/* index of this token */

    for (tokennr = 0; tokennr < maxtoken;) {
	while (isspace(*cptr))
	    cptr++;			/* skip initial white space */
	if ((*cptr == '\0') || (*cptr == '\n')
	    || (*cptr == '#'))		/* end of line */
	    return tokennr;		/* return number of tokens found */
	delim = *cptr;
	token[tokennr] = cptr;		/* point to it */
	tokennr++;			/* one more */
	if (tokennr == maxtoken)	/* run off the end? */
	    return tokennr;
	if ((delim == '\'') || (delim == '"')) { /* delimitered */
	    for (;;) {
		cptr++;
		if ((*cptr == delim)
		    && (cptr[-1] != '\\')) { /* found the partner */
		    cptr++;		/* move on past */
		    if (!isspace(*cptr)) /* no space after closing quote */
			return -1;
		    *cptr++ = '\0';	/* delimit */
		} else if ((*cptr == '\0')
		    || (*cptr == '\n'))	/* end of line */
		    return -1;
	    }
	} else {			/* not quoted */
	    while ((*cptr != '\0') && (!isspace(*cptr)) && (*cptr != '\n'))
		cptr++;
	    if (*cptr != '\0')		/* not end of the line, */
		*cptr++ = '\0';		/* delimit and move to the next */
	}
    }
    return maxtoken;			/* can't get here */
}

static void
doobj(const char *path, caddr_t addr, FILE *out)
{
    uintmax_t	base = (uintptr_t)addr;
    uintmax_t	textaddr = 0;
    uintmax_t	dataaddr = 0;
    uintmax_t	bssaddr = 0;
    uintmax_t  *up;
    int		octokens;
    char       *octoken[MAXTOKEN];
    char	ocbuf[LINE_MAX + PATH_MAX];
    FILE       *objcopy;

    snprintf(ocbuf, sizeof(ocbuf),
	     "/usr/bin/objdump --section-headers %s", path);
    if ((objcopy = popen(ocbuf, "r")) == NULL)
	err(2, "can't start %s", ocbuf);
    while (fgets(ocbuf, sizeof(ocbuf), objcopy)) {
	octokens = tokenize(ocbuf, octoken, MAXTOKEN);
	if (octokens <= 1)
	    continue;
	up = NULL;
	if (strcmp(octoken[1], ".text") == 0)
	    up = &textaddr;
	else if (strcmp(octoken[1], ".data") == 0)
	    up = &dataaddr;
	else if (strcmp(octoken[1], ".bss") == 0)
	    up = &bssaddr;
	if (up == NULL)
	    continue;
	*up = strtoumax(octoken[3], NULL, 16) + base;
    }
    if (textaddr) {	/* we must have a text address */
	fprintf(out, "add-symbol-file %s 0x%jx", path, textaddr);
	if (dataaddr)
	    fprintf(out, " -s .data 0x%jx", dataaddr);
	if (bssaddr)
	    fprintf(out, " -s .bss 0x%jx", bssaddr);
	fprintf(out, "\n");
    }
}

static void
findmodules(const char *modules_path, const char *sfx[], FILE *out)
{
    char	       *path_argv[2];
    char	       *p;
    FTS		       *fts;
    FTSENT	       *ftsent;
    struct kfile       *kfp;
    int			i;
    int			sl;

    /* Have to copy modules_path here because it's const */
    if ((path_argv[0] = strdup(modules_path)) == NULL)
	errx(2, "out of memory");
    path_argv[1] = NULL;

    /* Have to fts once per suffix to find preferred suffixes first */
    do {
	sl = *sfx ? strlen(*sfx) : 0;	/* current suffix length */
	fts = fts_open(path_argv, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
	if (fts == NULL)
	    err(2, "can't begin traversing path %s", modules_path);
	while ((ftsent = fts_read(fts)) != NULL) {
	    if (ftsent->fts_info == FTS_DNR ||
		ftsent->fts_info == FTS_ERR ||
		ftsent->fts_info == FTS_NS) {
		    errno = ftsent->fts_errno;
		    err(2, "error while traversing path %s", ftsent->fts_path);
	    }
	    if (ftsent->fts_info != FTS_F)
		continue;			/* not a plain file */

	    if (sl > 0) {
		/* non-blank suffix; see if file name has it */
		i = ftsent->fts_namelen - sl;
		if (i <= 0 || strcmp(ftsent->fts_name + i, *sfx) != 0)
		    continue;		/* no such suffix */
		if ((p = strdup(ftsent->fts_name)) == NULL)
		    errx(2, "out of memory");
		p[i] = '\0';		/* remove suffix in the copy */
		kfp = kfile_find(p);
		free(p);
	    } else
		kfp = kfile_find(ftsent->fts_name);

	    if (kfp && !kfp->seen) {
		doobj(ftsent->fts_path, kfp->addr, out);
		kfp->seen = 1;
		/* Optimization: stop fts as soon as seen all loaded modules */
		if (kfile_allseen()) {
		    fts_close(fts);
		    goto done;
		}
	    }
	}
	if (ftsent == NULL && errno != 0)
	    err(2, "couldn't complete traversing path %s", modules_path);
	fts_close(fts);
    } while (*sfx++);
done:
    free(path_argv[0]);
}

static void
usage(const char *myname)
{
    fprintf(stderr,
	"Usage:\n"
	"%s [-afKksVx] [-M core] [-N system ] [-o outfile] [-X suffix]\n"
	"%*s [modules-path [outfile]]\n\n"
	"\t-a\tappend to outfile\n"
	"\t-f\tfind the module in any subdirectory of modules-path\n"
	"\t-K\tuse kld(2) to get the list of modules\n"
	"\t-k\ttake input from kldstat(8)\n"
	"\t-M\tspecify core name for kvm(3)\n"
	"\t-N\tspecify system name for kvm(3)\n"
	"\t-o\tuse outfile instead of \".asf\"\n"
	"\t-s\tdon't prepend subdir for module path\n"
	"\t-V\tuse kvm(3) to get the list of modules\n"
	"\t-X\tappend suffix to list of possible module file name suffixes\n"
	"\t-x\tclear list of possible module file name suffixes\n",
	myname, (int)strlen(myname), "");
    exit(2);
}

#define	MAXSUFFIXES	15

/* KLD file names end in this */
static int	   nsuffixes = 2;
static const char *suffixes[MAXSUFFIXES + 1] = {
    ".debug",
    ".symbols",
    NULL
};

int
main(int argc, char *argv[])
{
    char basename[PATH_MAX];
    char path[PATH_MAX];
    const char *filemode = "w";		/* mode for outfile */
    const char *modules_path = "modules"; /* path to kernel build directory */
    const char *outfile = ".asf";	/* and where to write the output */
    const char *corefile = NULL;	/* for kvm(3) */
    const char *sysfile = NULL;		/* for kvm(3) */
    const char **sfx;
    struct kfile *kfp;
    struct stat st;
    FILE *out;				/* output file */
    int dofind = 0;
    int dokld = 0;
    int dokvm = 0;
    int nosubdir = 0;
    int runprog = 0;
    int i;
    const int sl = strlen(KLDSUFFIX);

    while ((i = getopt(argc, argv, "afKkM:N:o:sVX:x")) != -1)
	switch (i) {
	case 'a':
	    filemode = "a";	/* append to outfile */
	    break;
	case 'f':
	    dofind = 1;		/* find .ko (recursively) */
	    break;
	case 'K':
	    dokld = 1;		/* use kld(2) interface */
	    break;
	case 'k':
	    runprog = 1;	/* get input from kldstat(8) */
	    break;
	case 'M':
	    corefile = optarg;	/* core file for kvm(3) */
	    break;
	case 'N':
	    sysfile = optarg;	/* system file (kernel) for kvm(3) */
	    break;
	case 'o':
	    outfile = optarg;	/* output file name */
	    break;
	case 's':
	    nosubdir = 1;	/* don't descend into subdirs */
	    break;
	case 'V':
	    dokvm = 1;		/* use kvm(3) interface */
	    break;
	case 'X':
	    if (nsuffixes >= MAXSUFFIXES)
		errx(2, "only %d suffixes can be specified", MAXSUFFIXES);
	    suffixes[nsuffixes++] = optarg;
	    suffixes[nsuffixes] = NULL;
	    break;
	case 'x':
	    nsuffixes = 0;
	    suffixes[0] = NULL;
	    break;
	default:
	    usage(argv[0]);
	}

    argc -= optind;
    argv += optind;

    if (argc > 0) {
	modules_path = argv[0];
	argc--, argv++;
    }
    if (argc > 0) {
	outfile = argv[0];
	argc--, argv++;
    }
    if (argc > 0)
	usage(argv[0]);

    if (strcmp(outfile, "-") == 0)
	out = stdout;
    else
	if ((out = fopen(outfile, filemode)) == NULL)
	    err(2, "can't open output file %s", outfile);

    if (dokvm || corefile || sysfile) {
	if (dokld || runprog)
	    warnx("using kvm(3) instead");
	asf_kvm(sysfile, corefile);
    } else if (dokld) {
	if (runprog)
	    warnx("using kld(2) instead");
	asf_kld();
    } else
	asf_prog(runprog);

    /* Avoid long operations like module tree traversal when nothing to do */
    if (kfile_empty()) {
	warnx("no kernel modules loaded");
	return (0);
    }

    if (!dofind)
	STAILQ_FOREACH(kfp, &kfile_head, link) {
	    if (!nosubdir) {
		/* prepare basename of KLD, w/o suffix */
		strlcpy(basename, kfp->name, sizeof(basename) - 1);
		i = strlen(basename);
		if (i > sl && strcmp(basename + i - sl, KLDSUFFIX) == 0)
		    i -= sl;
		basename[i] = '/';
		basename[i + 1] = '\0';
	    }
	    for (sfx = suffixes;; sfx++) {
		snprintf(path, sizeof(path),
			 "%s/%s%s%s",
			 modules_path,
			 nosubdir ? "" : basename,
			 kfp->name,
			 *sfx ? *sfx : "");
		if (*sfx == NULL || stat(path, &st) == 0) {
		    doobj(path, kfp->addr, out);
		    break;
		}
	    }
	}
    else
    	findmodules(modules_path, suffixes, out);

    return (0);
}
