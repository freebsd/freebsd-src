/*	$NetBSD: mount_msdos.c,v 1.18 1997/09/16 12:24:18 lukem Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/iconv.h>
#include <sys/linker.h>
#include <sys/module.h>

#include <fs/msdosfs/msdosfsmount.h>

#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
/* must be after stdio to declare fparseln */
#include <libutil.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

#define TRANSITION_PERIOD_HACK

/*
 * XXX - no way to specify "foo=<bar>"-type options; that's what we'd
 * want for "-u", "-g", "-m", "-M", "-L", "-D", and "-W".
 */
static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_SYNC,
	MOPT_UPDATE,
	{ "shortnames", 0, MSDOSFSMNT_SHORTNAME, 1 },
	{ "longnames", 0, MSDOSFSMNT_LONGNAME, 1 },
	{ "nowin95", 0, MSDOSFSMNT_NOWIN95, 1 },
	{ NULL }
};

static gid_t	a_gid(char *);
static uid_t	a_uid(char *);
static mode_t	a_mask(char *);
static void	usage(void) __dead2;
static int	set_charset(struct msdosfs_args *);

int
main(int argc, char **argv)
{
	struct msdosfs_args args;
	struct stat sb;
	int c, mntflags, set_gid, set_uid, set_mask, set_dirmask;
	char *dev, *dir, mntpath[MAXPATHLEN], *csp;

	mntflags = set_gid = set_uid = set_mask = set_dirmask = 0;
	(void)memset(&args, '\0', sizeof(args));
	args.magic = MSDOSFS_ARGSMAGIC;

	args.cs_win = NULL;
	args.cs_dos = NULL;
	args.cs_local = NULL;
#ifdef TRANSITION_PERIOD_HACK
	while ((c = getopt(argc, argv, "sl9u:g:m:M:o:L:D:W:")) != -1) {
#else
	while ((c = getopt(argc, argv, "sl9u:g:m:M:o:L:D:")) != -1) {
#endif
		switch (c) {
		case 's':
			args.flags |= MSDOSFSMNT_SHORTNAME;
			break;
		case 'l':
			args.flags |= MSDOSFSMNT_LONGNAME;
			break;
		case '9':
			args.flags |= MSDOSFSMNT_NOWIN95;
			break;
		case 'u':
			args.uid = a_uid(optarg);
			set_uid = 1;
			break;
		case 'g':
			args.gid = a_gid(optarg);
			set_gid = 1;
			break;
		case 'm':
			args.mask = a_mask(optarg);
			set_mask = 1;
			break;
		case 'M':
			args.dirmask = a_mask(optarg);
			set_dirmask = 1;
			break;
		case 'L':
			if (setlocale(LC_CTYPE, optarg) == NULL)
				err(EX_CONFIG, "%s", optarg);
			csp = strchr(optarg,'.');
			if (!csp)
				err(EX_CONFIG, "%s", optarg);
			args.cs_local = malloc(ICONV_CSNMAXLEN);
			if (args.cs_local == NULL)
				err(EX_OSERR, "malloc()");
			strncpy(args.cs_local,
			    kiconv_quirkcs(csp + 1, KICONV_VENDOR_MICSFT),
			    ICONV_CSNMAXLEN);
			break;
		case 'D':
			args.cs_dos = malloc(ICONV_CSNMAXLEN);
			if (args.cs_dos == NULL)
				err(EX_OSERR, "malloc()");
			strncpy(args.cs_dos, optarg, ICONV_CSNMAXLEN);
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &args.flags);
			break;
#ifdef TRANSITION_PERIOD_HACK
		case 'W':
			args.cs_local = malloc(ICONV_CSNMAXLEN);
			if (args.cs_local == NULL)
				err(EX_OSERR, "malloc()");
			args.cs_dos = malloc(ICONV_CSNMAXLEN);
			if (args.cs_dos == NULL)
				err(EX_OSERR, "malloc()");
			if (strcmp(optarg, "iso22dos") == 0) {
				strcpy(args.cs_local, "ISO8859-2");
				strcpy(args.cs_dos, "CP852");
			} else if (strcmp(optarg, "iso72dos") == 0) {
				strcpy(args.cs_local, "ISO8859-7");
				strcpy(args.cs_dos, "CP737");
			} else if (strcmp(optarg, "koi2dos") == 0) {
				strcpy(args.cs_local, "KOI8-R");
				strcpy(args.cs_dos, "CP866");
			} else if (strcmp(optarg, "koi8u2dos") == 0) {
				strcpy(args.cs_local, "KOI8-U");
				strcpy(args.cs_dos, "CP866");
			} else {
				err(EX_NOINPUT, "%s", optarg);
			}
			break;
#endif /* TRANSITION_PERIOD_HACK */
		case '?':
		default:
			usage();
			break;
		}
	}

	if (optind + 2 != argc)
		usage();

	if (set_mask && !set_dirmask) {
		args.dirmask = args.mask;
		set_dirmask = 1;
	}
	else if (set_dirmask && !set_mask) {
		args.mask = args.dirmask;
		set_mask = 1;
	}

	dev = argv[optind];
	dir = argv[optind + 1];

	if (args.cs_local) {
		if (set_charset(&args) == -1)
			err(EX_OSERR, "msdosfs_iconv");
		args.flags |= MSDOSFSMNT_KICONV;
	} else if (args.cs_dos) {
		if ((args.cs_local = malloc(ICONV_CSNMAXLEN)) == NULL)
			err(EX_OSERR, "malloc()");
		strcpy(args.cs_local, "ISO8859-1");
		if (set_charset(&args) == -1)
			err(EX_OSERR, "msdosfs_iconv");
		args.flags |= MSDOSFSMNT_KICONV;
	}

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary
	 * slashes from the devicename if there are any.
	 */
	(void)checkpath(dir, mntpath);
	(void)rmslashes(dev, dev);

	args.fspec = dev;
	args.export.ex_root = -2;	/* unchecked anyway on DOS fs */
	if (mntflags & MNT_RDONLY)
		args.export.ex_flags = MNT_EXRDONLY;
	else
		args.export.ex_flags = 0;
	if (!set_gid || !set_uid || !set_mask) {
		if (stat(mntpath, &sb) == -1)
			err(EX_OSERR, "stat %s", mntpath);

		if (!set_uid)
			args.uid = sb.st_uid;
		if (!set_gid)
			args.gid = sb.st_gid;
		if (!set_mask)
			args.mask = args.dirmask =
				sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}

	if (mount("msdosfs", mntpath, mntflags, &args) < 0)
		err(EX_OSERR, "%s", dev);

	exit (0);
}

gid_t
a_gid(s)
	char *s;
{
	struct group *gr;
	char *gname;
	gid_t gid;

	if ((gr = getgrnam(s)) != NULL)
		gid = gr->gr_gid;
	else {
		for (gname = s; *s && isdigit(*s); ++s);
		if (!*s)
			gid = atoi(gname);
		else
			errx(EX_NOUSER, "unknown group id: %s", gname);
	}
	return (gid);
}

uid_t
a_uid(s)
	char *s;
{
	struct passwd *pw;
	char *uname;
	uid_t uid;

	if ((pw = getpwnam(s)) != NULL)
		uid = pw->pw_uid;
	else {
		for (uname = s; *s && isdigit(*s); ++s);
		if (!*s)
			uid = atoi(uname);
		else
			errx(EX_NOUSER, "unknown user id: %s", uname);
	}
	return (uid);
}

mode_t
a_mask(s)
	char *s;
{
	int done, rv;
	char *ep;

	done = 0;
	rv = -1;
	if (*s >= '0' && *s <= '7') {
		done = 1;
		rv = strtol(optarg, &ep, 8);
	}
	if (!done || rv < 0 || *ep)
		errx(EX_USAGE, "invalid file mode: %s", s);
	return (rv);
}

void
usage()
{
#ifdef TRANSITION_PERIOD_HACK
	fprintf(stderr, "%s\n%s\n%s\n",
	"usage: mount_msdosfs [-9ls] [-D DOS_codepage] [-g gid] [-L locale]",
	"                     [-M mask] [-m mask] [-o options] [-u uid]",
	"		      [-W table] special node");
#else
	fprintf(stderr, "%s\n%s\n%s\n",
	"usage: mount_msdosfs [-9ls] [-D DOS_codepage] [-g gid] [-L locale]",
	"                     [-M mask] [-m mask] [-o options] [-u uid]",
	"		      special node");
#endif
	exit(EX_USAGE);
}

int
set_charset(struct msdosfs_args *args)
{
	int error;

	if (modfind("msdosfs_iconv") < 0)
		if (kldload("msdosfs_iconv") < 0 || modfind("msdosfs_iconv") < 0) {
			warnx("cannot find or load \"msdosfs_iconv\" kernel module");
			return (-1);
		}

	if ((args->cs_win = malloc(ICONV_CSNMAXLEN)) == NULL)
		return (-1);
	strncpy(args->cs_win, ENCODING_UNICODE, ICONV_CSNMAXLEN);
	error = kiconv_add_xlat16_cspairs(args->cs_win, args->cs_local);
	if (error)
		return (-1);
	if (args->cs_dos) {
		error = kiconv_add_xlat16_cspairs(args->cs_dos, args->cs_local);
		if (error)
			return (-1);
	} else {
		if ((args->cs_dos = malloc(ICONV_CSNMAXLEN)) == NULL)
			return (-1);
		strcpy(args->cs_dos, args->cs_local);
		error = kiconv_add_xlat16_cspair(args->cs_local, args->cs_local,
				KICONV_FROM_UPPER | KICONV_LOWER);
		if (error)
			return (-1);
	}

	return (0);
}
