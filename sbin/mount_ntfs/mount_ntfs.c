/*
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 1999 Semen Ustimenko
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
 *
 * $FreeBSD$
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#define NTFS
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/module.h>
#include <sys/iconv.h>
#include <sys/linker.h>
#include <fs/ntfs/ntfsmount.h>
#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <libutil.h>

#include "mntopts.h"

#define TRANSITION_PERIOD_HACK

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

static gid_t	a_gid(char *);
static uid_t	a_uid(char *);
static mode_t	a_mask(char *);
static void	usage(void) __dead2;

static int	set_charset(struct ntfs_args *);

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct ntfs_args args;
	struct stat sb;
	int c, mntflags, set_gid, set_uid, set_mask;
	char *dev, *dir, mntpath[MAXPATHLEN];

	mntflags = set_gid = set_uid = set_mask = 0;
	(void)memset(&args, '\0', sizeof(args));
	args.cs_ntfs = NULL;
	args.cs_local = NULL;

#ifdef TRANSITION_PERIOD_HACK
	while ((c = getopt(argc, argv, "aiu:g:m:o:C:W:")) !=  -1) {
#else
	while ((c = getopt(argc, argv, "aiu:g:m:o:C:")) !=  -1) {
#endif
		switch (c) {
		case 'u':
			args.uid = a_uid(optarg);
			set_uid = 1;
			break;
		case 'g':
			args.gid = a_gid(optarg);
			set_gid = 1;
			break;
		case 'm':
			args.mode = a_mask(optarg);
			set_mask = 1;
			break;
		case 'i':
			args.flag |= NTFS_MFLAG_CASEINS;
			break;
		case 'a':
			args.flag |= NTFS_MFLAG_ALLNAMES;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case 'C':
			args.cs_local = malloc(ICONV_CSNMAXLEN);
			if (args.cs_local == NULL)
				err(EX_OSERR, "malloc()");
			strncpy(args.cs_local,
			    kiconv_quirkcs(optarg, KICONV_VENDOR_MICSFT),
			    ICONV_CSNMAXLEN);
			break;
#ifdef TRANSITION_PERIOD_HACK
		case 'W':
			args.cs_local = malloc(ICONV_CSNMAXLEN);
			if (args.cs_local == NULL)
				err(EX_OSERR, "malloc()");
			if (strcmp(optarg, "iso22dos") == 0) {
				strcpy(args.cs_local, "ISO8859-2");
			} else if (strcmp(optarg, "iso72dos") == 0) {
				strcpy(args.cs_local, "ISO8859-7");
			} else if (strcmp(optarg, "koi2dos") == 0) {
				strcpy(args.cs_local, "KOI8-R");
			} else if (strcmp(optarg, "koi8u2dos") == 0) {
				strcpy(args.cs_local, "KOI8-U");
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

	dev = argv[optind];
	dir = argv[optind + 1];

	if (args.cs_local) {
		if (set_charset(&args) == -1)
			err(EX_OSERR, "ntfs_iconv");
		args.flag |= NTFS_MFLAG_KICONV;
		/*
		 * XXX
		 * Force to be MNT_RDONLY,
		 * since only reading is supported right now,
		 */
		mntflags |= MNT_RDONLY;
	}

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary 
	 * slashes from the devicename if there are any.
	 */
	(void)checkpath(dir, mntpath);
	(void)rmslashes(dev, dev);

	args.fspec = dev;
	args.export.ex_root = 65534;	/* unchecked anyway on DOS fs */
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
			args.mode = sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}

	if (mount("ntfs", mntpath, mntflags, &args) < 0)
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
	int done, rv=0;
	char *ep;

	done = 0;
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
	fprintf(stderr, "%s\n%s\n",
	"usage: mount_ntfs [-a] [-i] [-u user] [-g group] [-m mask]",
	"                  [-C charset] [-W u2wtable] bdev dir");
#else
	fprintf(stderr, "usage: mount_ntfs [-a] [-i] [-u user] [-g group] [-m mask] [-C charset] bdev dir\n");
#endif
	exit(EX_USAGE);
}

int
set_charset(struct ntfs_args *pargs)
{
	int error;

	if (modfind("ntfs_iconv") < 0)
		if (kldload("ntfs_iconv") < 0 || modfind("ntfs_iconv") < 0) {
			warnx( "cannot find or load \"ntfs_iconv\" kernel module");
			return (-1);
		}

	if ((pargs->cs_ntfs = malloc(ICONV_CSNMAXLEN)) == NULL)
		return (-1);
	strncpy(pargs->cs_ntfs, ENCODING_UNICODE, ICONV_CSNMAXLEN);
	error = kiconv_add_xlat16_cspairs(pargs->cs_ntfs, pargs->cs_local);
	if (error)
		return (-1);

	return (0);
}
