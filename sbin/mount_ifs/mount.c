/*
 * Copyright (c) 1980, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1980, 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mount.c	8.19 (Berkeley) 4/19/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

int debug, verbose, skipvfs;

int	badvfsname __P((const char *, const char **));
int	badvfstype __P((int, const char **));
char   *catopt __P((char *, const char *));
struct statfs
       *getmntpt __P((const char *));
const char
      **makevfslist __P((char *));
void	mangle __P((char *, int *, const char **));
int	mountfs __P((const char *, const char *, const char *,
			int, const char *, const char *));
void	prmount __P((const char *, const char *, int));
void	usage __P((void));

/* From mount_ufs.c. */
int	mount_ufs __P((int, char * const *));

/* Map from mount otions to printable formats. */
static struct opt {
	int o_opt;
	const char *o_name;
} optnames[] = {
	{ MNT_ASYNC,		"asynchronous" },
	{ MNT_EXPORTED,		"NFS exported" },
	{ MNT_LOCAL,		"local" },
	{ MNT_NODEV,		"nodev" },
	{ MNT_NOEXEC,		"noexec" },
	{ MNT_NOSUID,		"nosuid" },
	{ MNT_QUOTA,		"with quotas" },
	{ MNT_RDONLY,		"read-only" },
	{ MNT_SYNCHRONOUS,	"synchronous" },
	{ MNT_UNION,		"union" },
	{ MNT_USER,		"user mount" },
	{ NULL }
};

int
main(argc, argv)
	int argc;
	char * const argv[];
{
	const char *mntonname, **vfslist, *vfstype;
	struct fstab *fs;
	struct statfs *mntbuf;
	FILE *mountdfp;
	pid_t pid;
	int all, ch, i, init_flags, mntsize, rval;
	char *options;

	all = init_flags = 0;
	options = NULL;
	vfslist = NULL;
	vfstype = "ufs";
	while ((ch = getopt(argc, argv, "adfo:rwt:uv")) != EOF)
		switch (ch) {
		case 'a':
			all = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			init_flags |= MNT_FORCE;
			break;
		case 'o':
			if (*optarg)
				options = catopt(options, optarg);
			break;
		case 'r':
			init_flags |= MNT_RDONLY;
			break;
		case 't':
			if (vfslist != NULL)
				errx(1, "only one -t option may be specified.");
			vfslist = makevfslist(optarg);
			vfstype = optarg;
			break;
		case 'u':
			init_flags |= MNT_UPDATE;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			init_flags &= ~MNT_RDONLY;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

#define	BADTYPE(type)							\
	(strcmp(type, FSTAB_RO) &&					\
	    strcmp(type, FSTAB_RW) && strcmp(type, FSTAB_RQ))

	rval = 0;
	switch (argc) {
	case 0:
		if (all)
			while ((fs = getfsent()) != NULL) {
				if (BADTYPE(fs->fs_type))
					continue;
				if (badvfsname(fs->fs_vfstype, vfslist))
					continue;
				if (strstr(fs->fs_mntops, "noauto"))
					continue;
				if (mountfs(fs->fs_vfstype, fs->fs_spec,
		    			fs->fs_file, init_flags, options,
			    		fs->fs_mntops))
						rval = 1;
			}
		else {
			if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
				err(1, "getmntinfo");
			for (i = 0; i < mntsize; i++) {
				if (badvfstype(mntbuf[i].f_type, vfslist))
					continue;
				prmount(mntbuf[i].f_mntfromname,
				    mntbuf[i].f_mntonname, mntbuf[i].f_flags);
			}
		}
		exit(rval);
	case 1:
		if (vfslist != NULL)
			usage();

		if (init_flags & MNT_UPDATE) {
			if ((mntbuf = getmntpt(*argv)) == NULL)
				errx(1,
				    "unknown special file or file system %s.",
				    *argv);
			if ((fs = getfsfile(mntbuf->f_mntonname)) == NULL)
				errx(1, "can't find fstab entry for %s.",
				    *argv);
			/* If it's an update, ignore the fstab file options. */
			fs->fs_mntops = NULL;
			mntonname = mntbuf->f_mntonname;
		} else {
			if ((fs = getfsfile(*argv)) == NULL &&
			    (fs = getfsspec(*argv)) == NULL)
				errx(1,
				    "%s: unknown special file or file system.",
				    *argv);
			if (BADTYPE(fs->fs_type))
				errx(1, "%s has unknown file system type.",
				    *argv);
			mntonname = fs->fs_file;
		}
		rval = mountfs(fs->fs_vfstype, fs->fs_spec,
		    mntonname, init_flags, options, fs->fs_mntops);
		break;
	case 2:
		/*
		 * If -t flag has not been specified, and spec contains either
		 * a ':' or a '@' then assume that an NFS filesystem is being
		 * specified ala Sun.
		 */
		if (vfslist == NULL && strpbrk(argv[0], ":@") != NULL)
			vfstype = "nfs";
		rval = mountfs(vfstype,
		    argv[0], argv[1], init_flags, options, NULL);
		break;
	default:
		usage();
		/* NOTREACHED */
	}

	/*
	 * If the mount was successfully, and done by root, tell mountd the
	 * good news.  Pid checks are probably unnecessary, but don't hurt.
	 */
	if (rval == 0 && getuid() == 0 &&
	    (mountdfp = fopen(_PATH_MOUNTDPID, "r")) != NULL) {
		if (fscanf(mountdfp, "%ld", &pid) == 1 &&
		     pid > 0 && kill(pid, SIGHUP) == -1 && errno != ESRCH)
			err(1, "signal mountd");
		(void)fclose(mountdfp);
	}

	exit(rval);
}

int
mountfs(vfstype, spec, name, flags, options, mntopts)
	const char *vfstype, *spec, *name, *options, *mntopts;
	int flags;
{
	struct stat sb;

	/* List of directories containing mount_xxx subcommands. */
	static const char *edirs[] = {
		_PATH_SBIN,
		_PATH_USRSBIN,
		NULL
	};
	const char *argv[100], **edir;
	struct statfs sf;
	pid_t pid;
	int argc, i, status;
	char *optbuf, execname[MAXPATHLEN + 1], mntpath[MAXPATHLEN];

	if (realpath(name, mntpath) != NULL && stat(mntpath, &sb) == 0) {
		if (!S_ISDIR(sb.st_mode)) {
			warnx("%s: Not a directory", mntpath);
			return (1);
		}
	} else {
		warn("%s", mntpath);
		return (1);
	}

	if (mntopts == NULL)
		mntopts = "";

	name = mntpath;

	if (options == NULL) {
		if (*mntopts == '\0')
			options = "rw";
		else
			options = mntopts;
		mntopts = "";
	}
	optbuf = catopt(strdup(mntopts), options);

	if (strcmp(name, "/") == 0)
		flags |= MNT_UPDATE;
	if (flags & MNT_FORCE)
		optbuf = catopt(optbuf, "force");
	if (flags & MNT_RDONLY)
		optbuf = catopt(optbuf, "ro");
	/*
	 * XXX
	 * The mount_mfs (newfs) command uses -o to select the
	 * optimisation mode.  We don't pass the default "-o rw"
	 * for that reason.
	 */
	if (flags & MNT_UPDATE)
		optbuf = catopt(optbuf, "update");

	argc = 0;
	argv[argc++] = vfstype;
	mangle(optbuf, &argc, argv);
	argv[argc++] = spec;
	argv[argc++] = name;
	argv[argc] = NULL;

	if (debug) {
		(void)printf("exec: mount_%s", vfstype);
		for (i = 1; i < argc; i++)
			(void)printf(" %s", argv[i]);
		(void)printf("\n");
		return (0);
	}

	switch (pid = vfork()) {
	case -1:				/* Error. */
		warn("vfork");
		free(optbuf);
		return (1);
	case 0:					/* Child. */
		if (strcmp(vfstype, "ufs") == 0)
			exit(mount_ufs(argc, (char * const *) argv));

		/* Go find an executable. */
		for (edir = edirs; *edir; edir++) {
			(void)snprintf(execname,
			    sizeof(execname), "%s/mount_%s", *edir, vfstype);
			execv(execname, (char * const *)argv);
		}
		if (errno == ENOENT) {
			int len = 0;
			char *cp;
			for (edir = edirs; *edir; edir++)
				len += strlen(*edir) + 2;	/* ", " */
			if ((cp = malloc(len)) == NULL) {
				warn(NULL);
				exit(1);
			}
			cp[0] = '\0';
			for (edir = edirs; *edir; edir++) {
				strcat(cp, *edir);
				if (edir[1] != NULL)
					strcat(cp, ", ");
			}
			warn("exec mount_%s not found in %s", vfstype, cp);
		}
		exit(1);
		/* NOTREACHED */
	default:				/* Parent. */
		free(optbuf);

		if (waitpid(pid, &status, 0) < 0) {
			warn("waitpid");
			return (1);
		}

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				return (WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			warnx("%s: %s", name, sys_siglist[WTERMSIG(status)]);
			return (1);
		}

		if (verbose) {
			if (statfs(name, &sf) < 0) {
				warn("%s", name);
				return (1);
			}
			prmount(sf.f_mntfromname, sf.f_mntonname, sf.f_flags);
		}
		break;
	}

	return (0);
}

void
prmount(spec, name, flags)
	const char *spec, *name;
	int flags;
{
	struct opt *o;
	int f;

	(void)printf("%s on %s", spec, name);

	flags &= MNT_VISFLAGMASK;
	for (f = 0, o = optnames; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			(void)printf("%s%s", !f++ ? " (" : ", ", o->o_name);
			flags &= ~o->o_opt;
		}
	(void)printf(f ? ")\n" : "\n");
}

struct statfs *
getmntpt(name)
	const char *name;
{
	struct statfs *mntbuf;
	int i, mntsize;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++)
		if (strcmp(mntbuf[i].f_mntfromname, name) == 0 ||
		    strcmp(mntbuf[i].f_mntonname, name) == 0)
			return (&mntbuf[i]);
	return (NULL);
}

int
badvfsname(vfsname, vfslist)
	const char *vfsname;
	const char **vfslist;
{

	if (vfslist == NULL)
		return (0);
	while (*vfslist != NULL) {
		if (strcmp(vfsname, *vfslist) == 0)
			return (skipvfs);
		++vfslist;
	}
	return (!skipvfs);
}

int
badvfstype(vfstype, vfslist)
	int vfstype;
	const char **vfslist;
{
	struct vfsconf *vfc;
	vfc = getvfsbytype(vfstype);

	if ( ! vfc )
		return (0);

	return (badvfsname(vfc->vfc_name, vfslist));
}

const char **
makevfslist(fslist)
	char *fslist;
{
	const char **av;
	int i;
	char *nextcp;

	if (fslist == NULL)
		return (NULL);
	if (fslist[0] == 'n' && fslist[1] == 'o') {
		fslist += 2;
		skipvfs = 1;
	}
	for (i = 0, nextcp = fslist; *nextcp; nextcp++)
		if (*nextcp == ',')
			i++;
	if ((av = malloc((size_t)(i + 2) * sizeof(char *))) == NULL) {
		warn(NULL);
		return (NULL);
	}
	nextcp = fslist;
	i = 0;
	av[i++] = nextcp;
	while ((nextcp = strchr(nextcp, ',')) != NULL) {
		*nextcp++ = '\0';
		av[i++] = nextcp;
	}
	av[i++] = NULL;
	return (av);
}

char *
catopt(s0, s1)
	char *s0;
	const char *s1;
{
	size_t i;
	char *cp;

	if (s0 && *s0) {
		i = strlen(s0) + strlen(s1) + 1 + 1;
		if ((cp = malloc(i)) == NULL)
			err(1, NULL);
		(void)snprintf(cp, i, "%s,%s", s0, s1);
	} else
		cp = strdup(s1);

	if (s0)
		free(s0);
	return (cp);
}

void
mangle(options, argcp, argv)
	char *options;
	int *argcp;
	const char **argv;
{
	char *p, *s;
	int argc;

	argc = *argcp;
	for (s = options; (p = strsep(&s, ",")) != NULL;)
		if (*p != '\0')
			if (*p == '-') {
				argv[argc++] = p;
				p = strchr(p, '=');
				if (p) {
					*p = '\0';
					argv[argc++] = p+1;
				}
			} else if (strcmp(p, "rw") != 0) {
				argv[argc++] = "-o";
				argv[argc++] = p;
			}

	*argcp = argc;
}

void
usage()
{

	(void)fprintf(stderr,
		"usage: mount %s %s\n       mount %s\n       mount %s\n",
		"[-dfruvw] [-o options] [-t ufs | external_type]",
			"special node",
		"[-adfruvw] [-t ufs | external_type]",
		"[-dfruvw] special | node");
	exit(1);
}
