/*
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 1999 Semen Ustimenko (semenu@FreeBSD.org)
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fs/hpfs/hpfsmount.h>
#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

static gid_t	a_gid __P((char *));
static uid_t	a_uid __P((char *));
static mode_t	a_mask __P((char *));
static void	usage __P((void)) __dead2;
static void	load_u2wtable __P((struct hpfs_args *, char *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct hpfs_args args;
	struct stat sb;
	int c, mntflags, set_gid, set_uid, set_mask,error;
	int forcerw = 0;
	char *dev, *dir, ndir[MAXPATHLEN+1];
#if __FreeBSD_version >= 300000
	struct vfsconf vfc;
#else
	struct vfsconf *vfc;
#endif

	mntflags = set_gid = set_uid = set_mask = 0;
	(void)memset(&args, '\0', sizeof(args));

	while ((c = getopt(argc, argv, "u:g:m:o:c:W:F")) !=  -1) {
		switch (c) {
		case 'F':
			forcerw=1;
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
			args.mode = a_mask(optarg);
			set_mask = 1;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case 'W':
			load_u2wtable(&args, optarg);
			args.flags |= HPFSMNT_TABLES;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}

	if (optind + 2 != argc)
		usage();

	if (!(mntflags & MNT_RDONLY) && !forcerw) {
		warnx("Write support is BETA, you need -F flag to enable RW mount!");
		exit (111);
	}

	dev = argv[optind];
	dir = argv[optind + 1];
	if (dir[0] != '/') {
		warnx("\"%s\" is a relative path", dir);
		if (getcwd(ndir, sizeof(ndir)) == NULL)
			err(EX_OSERR, "getcwd");
		strncat(ndir, "/", sizeof(ndir) - strlen(ndir) - 1);
		strncat(ndir, dir, sizeof(ndir) - strlen(ndir) - 1);
		dir = ndir;
		warnx("using \"%s\" instead", dir);
	}

	args.fspec = dev;
	args.export.ex_root = 65534;	/* unchecked anyway on DOS fs */
	if (mntflags & MNT_RDONLY)
		args.export.ex_flags = MNT_EXRDONLY;
	else
		args.export.ex_flags = 0;

	if (!set_gid || !set_uid || !set_mask) {
		if (stat(dir, &sb) == -1)
			err(EX_OSERR, "stat %s", dir);

		if (!set_uid)
			args.uid = sb.st_uid;
		if (!set_gid)
			args.gid = sb.st_gid;
		if (!set_mask)
			args.mode = sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}

#if __FreeBSD_version >= 300000
	error = getvfsbyname("hpfs", &vfc);
	if(error && vfsisloadable("hpfs")) {
		if(vfsload("hpfs"))
#else
	vfc = getvfsbyname("hpfs");
	if(!vfc && vfsisloadable("hpfs")) {
		if(vfsload("hpfs"))
#endif
			err(EX_OSERR, "vfsload(hpfs)");
		endvfsent();	/* clear cache */
#if __FreeBSD_version >= 300000
		error = getvfsbyname("hpfs", &vfc);
#else
		vfc = getvfsbyname("hpfs");
#endif
	}
#if __FreeBSD_version >= 300000
	if (error)
#else
	if (!vfc)
#endif
		errx(EX_OSERR, "hpfs filesystem is not available");

#if __FreeBSD_version >= 300000
	if (mount(vfc.vfc_name, dir, mntflags, &args) < 0)
#else
	if (mount(vfc->vfc_index, dir, mntflags, &args) < 0)
#endif
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
	fprintf(stderr, "usage: mount_hpfs [-u user] [-g group] [-m mask] bdev dir\n");
	exit(EX_USAGE);
}

void
load_u2wtable (pargs, name)
	struct hpfs_args *pargs;
	char *name;
{
	FILE *f;
	int i, code;
	char buf[128];
	char *fn;

	if (*name == '/')
		fn = name;
	else {
		snprintf(buf, sizeof(buf), "/usr/libdata/msdosfs/%s", name);
		buf[127] = '\0';
		fn = buf;
	}
	if ((f = fopen(fn, "r")) == NULL)
		err(EX_NOINPUT, "%s", fn);
	for (i = 0; i < 128; i++) {
		if (fscanf(f, "%i", &code) != 1)
			errx(EX_DATAERR, "u2w: missing item number %d", i);
		/* pargs->u2w[i] = code; */
	}
	for (i = 0; i < 128; i++) {
		if (fscanf(f, "%i", &code) != 1)
			errx(EX_DATAERR, "d2u: missing item number %d", i);
		pargs->d2u[i] = code;
	}
	for (i = 0; i < 128; i++) {
		if (fscanf(f, "%i", &code) != 1)
			errx(EX_DATAERR, "u2d: missing item number %d", i);
		pargs->u2d[i] = code;
	}
	fclose(f);
}
