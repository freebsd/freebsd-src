/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
static char sccsid[] = "@(#)dirs.c	8.2 (Berkeley) 1/21/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <protocols/dumprestore.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <machine/endian.h>

#include "pathnames.h"
#include "restore.h"
#include "extern.h"

/*
 * Symbol table of directories read from tape.
 */
#define HASHSIZE	1000
#define INOHASH(val) (val % HASHSIZE)
struct inotab {
	struct	inotab *t_next;
	ino_t	t_ino;
	long	t_seekpt;
	long	t_size;
};
static struct inotab *inotab[HASHSIZE];

/*
 * Information retained about directories.
 */
struct modeinfo {
	ino_t ino;
	struct timeval timep[2];
	short mode;
	short uid;
	short gid;
};

/*
 * Definitions for library routines operating on directories.
 */
#undef DIRBLKSIZ
#define DIRBLKSIZ 1024
struct rstdirdesc {
	int	dd_fd;
	long	dd_loc;
	long	dd_size;
	char	dd_buf[DIRBLKSIZ];
};

/*
 * Global variables for this file.
 */
static long	seekpt;
static FILE	*df, *mf;
static RST_DIR	*dirp;
static char	dirfile[32] = "#";	/* No file */
static char	modefile[32] = "#";	/* No file */
static char	dot[2] = ".";		/* So it can be modified */

/*
 * Format of old style directories.
 */
#define ODIRSIZ 14
struct odirect {
	u_short	d_ino;
	char	d_name[ODIRSIZ];
};

static struct inotab	*allocinotab __P((ino_t, struct dinode *, long));
static void		 dcvt __P((struct odirect *, struct direct *));
static void		 flushent __P((void));
static struct inotab	*inotablookup __P((ino_t));
static RST_DIR		*opendirfile __P((const char *));
static void		 putdir __P((char *, long));
static void		 putent __P((struct direct *));
static void		 rst_seekdir __P((RST_DIR *, long, long));
static long		 rst_telldir __P((RST_DIR *));
static struct direct	*searchdir __P((ino_t, char *));

/*
 *	Extract directory contents, building up a directory structure
 *	on disk for extraction by name.
 *	If genmode is requested, save mode, owner, and times for all
 *	directories on the tape.
 */
void
extractdirs(genmode)
	int genmode;
{
	register int i;
	register struct dinode *ip;
	struct inotab *itp;
	struct direct nulldir;

	vprintf(stdout, "Extract directories from tape\n");
	(void) sprintf(dirfile, "%s/rstdir%d", _PATH_TMP, dumpdate);
	df = fopen(dirfile, "w");
	if (df == NULL) {
		fprintf(stderr,
		    "restore: %s - cannot create directory temporary\n",
		    dirfile);
		fprintf(stderr, "fopen: %s\n", strerror(errno));
		done(1);
	}
	if (genmode != 0) {
		(void) sprintf(modefile, "%s/rstmode%d", _PATH_TMP, dumpdate);
		mf = fopen(modefile, "w");
		if (mf == NULL) {
			fprintf(stderr,
			    "restore: %s - cannot create modefile \n",
			    modefile);
			fprintf(stderr, "fopen: %s\n", strerror(errno));
			done(1);
		}
	}
	nulldir.d_ino = 0;
	nulldir.d_type = DT_DIR;
	nulldir.d_namlen = 1;
	(void) strcpy(nulldir.d_name, "/");
	nulldir.d_reclen = DIRSIZ(0, &nulldir);
	for (;;) {
		curfile.name = "<directory file - name unknown>";
		curfile.action = USING;
		ip = curfile.dip;
		if (ip == NULL || (ip->di_mode & IFMT) != IFDIR) {
			(void) fclose(df);
			dirp = opendirfile(dirfile);
			if (dirp == NULL)
				fprintf(stderr, "opendirfile: %s\n",
				    strerror(errno));
			if (mf != NULL)
				(void) fclose(mf);
			i = dirlookup(dot);
			if (i == 0)
				panic("Root directory is not on tape\n");
			return;
		}
		itp = allocinotab(curfile.ino, ip, seekpt);
		getfile(putdir, xtrnull);
		putent(&nulldir);
		flushent();
		itp->t_size = seekpt - itp->t_seekpt;
	}
}

/*
 * skip over all the directories on the tape
 */
void
skipdirs()
{

	while (curfile.dip && (curfile.dip->di_mode & IFMT) == IFDIR) {
		skipfile();
	}
}

/*
 *	Recursively find names and inumbers of all files in subtree
 *	pname and pass them off to be processed.
 */
void
treescan(pname, ino, todo)
	char *pname;
	ino_t ino;
	long (*todo) __P((char *, ino_t, int));
{
	register struct inotab *itp;
	register struct direct *dp;
	int namelen;
	long bpt;
	char locname[MAXPATHLEN + 1];

	itp = inotablookup(ino);
	if (itp == NULL) {
		/*
		 * Pname is name of a simple file or an unchanged directory.
		 */
		(void) (*todo)(pname, ino, LEAF);
		return;
	}
	/*
	 * Pname is a dumped directory name.
	 */
	if ((*todo)(pname, ino, NODE) == FAIL)
		return;
	/*
	 * begin search through the directory
	 * skipping over "." and ".."
	 */
	(void) strncpy(locname, pname, MAXPATHLEN);
	(void) strncat(locname, "/", MAXPATHLEN);
	namelen = strlen(locname);
	rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
	dp = rst_readdir(dirp); /* "." */
	if (dp != NULL && strcmp(dp->d_name, ".") == 0)
		dp = rst_readdir(dirp); /* ".." */
	else
		fprintf(stderr, "Warning: `.' missing from directory %s\n",
			pname);
	if (dp != NULL && strcmp(dp->d_name, "..") == 0)
		dp = rst_readdir(dirp); /* first real entry */
	else
		fprintf(stderr, "Warning: `..' missing from directory %s\n",
			pname);
	bpt = rst_telldir(dirp);
	/*
	 * a zero inode signals end of directory
	 */
	while (dp != NULL && dp->d_ino != 0) {
		locname[namelen] = '\0';
		if (namelen + dp->d_namlen >= MAXPATHLEN) {
			fprintf(stderr, "%s%s: name exceeds %d char\n",
				locname, dp->d_name, MAXPATHLEN);
		} else {
			(void) strncat(locname, dp->d_name, (int)dp->d_namlen);
			treescan(locname, dp->d_ino, todo);
			rst_seekdir(dirp, bpt, itp->t_seekpt);
		}
		dp = rst_readdir(dirp);
		bpt = rst_telldir(dirp);
	}
	if (dp == NULL)
		fprintf(stderr, "corrupted directory: %s.\n", locname);
}

/*
 * Lookup a pathname which is always assumed to start from the ROOTINO.
 */
struct direct *
pathsearch(pathname)
	const char *pathname;
{
	ino_t ino;
	struct direct *dp;
	char *path, *name, buffer[MAXPATHLEN];

	strcpy(buffer, pathname);
	path = buffer;
	ino = ROOTINO;
	while (*path == '/')
		path++;
	dp = NULL;
	while ((name = strsep(&path, "/")) != NULL && *name != NULL) {
		if ((dp = searchdir(ino, name)) == NULL)
			return (NULL);
		ino = dp->d_ino;
	}
	return (dp);
}

/*
 * Lookup the requested name in directory inum.
 * Return its inode number if found, zero if it does not exist.
 */
static struct direct *
searchdir(inum, name)
	ino_t	inum;
	char	*name;
{
	register struct direct *dp;
	register struct inotab *itp;
	int len;

	itp = inotablookup(inum);
	if (itp == NULL)
		return (NULL);
	rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
	len = strlen(name);
	do {
		dp = rst_readdir(dirp);
		if (dp == NULL || dp->d_ino == 0)
			return (NULL);
	} while (dp->d_namlen != len || strncmp(dp->d_name, name, len) != 0);
	return (dp);
}

/*
 * Put the directory entries in the directory file
 */
static void
putdir(buf, size)
	char *buf;
	long size;
{
	struct direct cvtbuf;
	register struct odirect *odp;
	struct odirect *eodp;
	register struct direct *dp;
	long loc, i;

	if (cvtflag) {
		eodp = (struct odirect *)&buf[size];
		for (odp = (struct odirect *)buf; odp < eodp; odp++)
			if (odp->d_ino != 0) {
				dcvt(odp, &cvtbuf);
				putent(&cvtbuf);
			}
	} else {
		for (loc = 0; loc < size; ) {
			dp = (struct direct *)(buf + loc);
			if (Bcvt)
				swabst((u_char *)"ls", (u_char *) dp);
			if (oldinofmt && dp->d_ino != 0) {
#if BYTE_ORDER == BIG_ENDIAN
				if (Bcvt)
#else
				if (!Bcvt)
#endif
					dp->d_namlen = dp->d_type;
				dp->d_type = DT_UNKNOWN;
			}
			i = DIRBLKSIZ - (loc & (DIRBLKSIZ - 1));
			if ((dp->d_reclen & 0x3) != 0 ||
			    dp->d_reclen > i ||
			    dp->d_reclen < DIRSIZ(0, dp) ||
			    dp->d_namlen > NAME_MAX) {
				vprintf(stdout, "Mangled directory: ");
				if ((dp->d_reclen & 0x3) != 0)
					vprintf(stdout,
					   "reclen not multiple of 4 ");
				if (dp->d_reclen < DIRSIZ(0, dp))
					vprintf(stdout,
					   "reclen less than DIRSIZ (%d < %d) ",
					   dp->d_reclen, DIRSIZ(0, dp));
				if (dp->d_namlen > NAME_MAX)
					vprintf(stdout,
					   "reclen name too big (%d > %d) ",
					   dp->d_namlen, NAME_MAX);
				vprintf(stdout, "\n");
				loc += i;
				continue;
			}
			loc += dp->d_reclen;
			if (dp->d_ino != 0) {
				putent(dp);
			}
		}
	}
}

/*
 * These variables are "local" to the following two functions.
 */
char dirbuf[DIRBLKSIZ];
long dirloc = 0;
long prev = 0;

/*
 * add a new directory entry to a file.
 */
static void
putent(dp)
	struct direct *dp;
{
	dp->d_reclen = DIRSIZ(0, dp);
	if (dirloc + dp->d_reclen > DIRBLKSIZ) {
		((struct direct *)(dirbuf + prev))->d_reclen =
		    DIRBLKSIZ - prev;
		(void) fwrite(dirbuf, 1, DIRBLKSIZ, df);
		dirloc = 0;
	}
	bcopy((char *)dp, dirbuf + dirloc, (long)dp->d_reclen);
	prev = dirloc;
	dirloc += dp->d_reclen;
}

/*
 * flush out a directory that is finished.
 */
static void
flushent()
{
	((struct direct *)(dirbuf + prev))->d_reclen = DIRBLKSIZ - prev;
	(void) fwrite(dirbuf, (int)dirloc, 1, df);
	seekpt = ftell(df);
	dirloc = 0;
}

static void
dcvt(odp, ndp)
	register struct odirect *odp;
	register struct direct *ndp;
{

	bzero((char *)ndp, (long)(sizeof *ndp));
	ndp->d_ino =  odp->d_ino;
	ndp->d_type = DT_UNKNOWN;
	(void) strncpy(ndp->d_name, odp->d_name, ODIRSIZ);
	ndp->d_namlen = strlen(ndp->d_name);
	ndp->d_reclen = DIRSIZ(0, ndp);
}

/*
 * Seek to an entry in a directory.
 * Only values returned by rst_telldir should be passed to rst_seekdir.
 * This routine handles many directories in a single file.
 * It takes the base of the directory in the file, plus
 * the desired seek offset into it.
 */
static void
rst_seekdir(dirp, loc, base)
	register RST_DIR *dirp;
	long loc, base;
{

	if (loc == rst_telldir(dirp))
		return;
	loc -= base;
	if (loc < 0)
		fprintf(stderr, "bad seek pointer to rst_seekdir %d\n", loc);
	(void) lseek(dirp->dd_fd, base + (loc & ~(DIRBLKSIZ - 1)), SEEK_SET);
	dirp->dd_loc = loc & (DIRBLKSIZ - 1);
	if (dirp->dd_loc != 0)
		dirp->dd_size = read(dirp->dd_fd, dirp->dd_buf, DIRBLKSIZ);
}

/*
 * get next entry in a directory.
 */
struct direct *
rst_readdir(dirp)
	register RST_DIR *dirp;
{
	register struct direct *dp;

	for (;;) {
		if (dirp->dd_loc == 0) {
			dirp->dd_size = read(dirp->dd_fd, dirp->dd_buf,
			    DIRBLKSIZ);
			if (dirp->dd_size <= 0) {
				dprintf(stderr, "error reading directory\n");
				return (NULL);
			}
		}
		if (dirp->dd_loc >= dirp->dd_size) {
			dirp->dd_loc = 0;
			continue;
		}
		dp = (struct direct *)(dirp->dd_buf + dirp->dd_loc);
		if (dp->d_reclen == 0 ||
		    dp->d_reclen > DIRBLKSIZ + 1 - dirp->dd_loc) {
			dprintf(stderr, "corrupted directory: bad reclen %d\n",
				dp->d_reclen);
			return (NULL);
		}
		dirp->dd_loc += dp->d_reclen;
		if (dp->d_ino == 0 && strcmp(dp->d_name, "/") != 0)
			continue;
		if (dp->d_ino >= maxino) {
			dprintf(stderr, "corrupted directory: bad inum %d\n",
				dp->d_ino);
			continue;
		}
		return (dp);
	}
}

/*
 * Simulate the opening of a directory
 */
RST_DIR *
rst_opendir(name)
	const char *name;
{
	struct inotab *itp;
	RST_DIR *dirp;
	ino_t ino;

	if ((ino = dirlookup(name)) > 0 &&
	    (itp = inotablookup(ino)) != NULL) {
		dirp = opendirfile(dirfile);
		rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
		return (dirp);
	}
	return (NULL);
}

/*
 * In our case, there is nothing to do when closing a directory.
 */
void
rst_closedir(dirp)
	RST_DIR *dirp;
{

	(void)close(dirp->dd_fd);
	free(dirp);
	return;
}

/*
 * Simulate finding the current offset in the directory.
 */
static long
rst_telldir(dirp)
	RST_DIR *dirp;
{
	return ((long)lseek(dirp->dd_fd,
	    (off_t)0, SEEK_CUR) - dirp->dd_size + dirp->dd_loc);
}

/*
 * Open a directory file.
 */
static RST_DIR *
opendirfile(name)
	const char *name;
{
	register RST_DIR *dirp;
	register int fd;

	if ((fd = open(name, O_RDONLY)) == -1)
		return (NULL);
	if ((dirp = malloc(sizeof(RST_DIR))) == NULL) {
		(void)close(fd);
		return (NULL);
	}
	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	return (dirp);
}

/*
 * Set the mode, owner, and times for all new or changed directories
 */
void
setdirmodes(flags)
	int flags;
{
	FILE *mf;
	struct modeinfo node;
	struct entry *ep;
	char *cp;

	vprintf(stdout, "Set directory mode, owner, and times.\n");
	(void) sprintf(modefile, "%s/rstmode%d", _PATH_TMP, dumpdate);
	mf = fopen(modefile, "r");
	if (mf == NULL) {
		fprintf(stderr, "fopen: %s\n", strerror(errno));
		fprintf(stderr, "cannot open mode file %s\n", modefile);
		fprintf(stderr, "directory mode, owner, and times not set\n");
		return;
	}
	clearerr(mf);
	for (;;) {
		(void) fread((char *)&node, 1, sizeof(struct modeinfo), mf);
		if (feof(mf))
			break;
		ep = lookupino(node.ino);
		if (command == 'i' || command == 'x') {
			if (ep == NULL)
				continue;
			if ((flags & FORCE) == 0 && ep->e_flags & EXISTED) {
				ep->e_flags &= ~NEW;
				continue;
			}
			if (node.ino == ROOTINO &&
		   	    reply("set owner/mode for '.'") == FAIL)
				continue;
		}
		if (ep == NULL) {
			panic("cannot find directory inode %d\n", node.ino);
		} else {
			cp = myname(ep);
			(void) chown(cp, node.uid, node.gid);
			(void) chmod(cp, node.mode);
			utimes(cp, node.timep);
			ep->e_flags &= ~NEW;
		}
	}
	if (ferror(mf))
		panic("error setting directory modes\n");
	(void) fclose(mf);
}

/*
 * Generate a literal copy of a directory.
 */
int
genliteraldir(name, ino)
	char *name;
	ino_t ino;
{
	register struct inotab *itp;
	int ofile, dp, i, size;
	char buf[BUFSIZ];

	itp = inotablookup(ino);
	if (itp == NULL)
		panic("Cannot find directory inode %d named %s\n", ino, name);
	if ((ofile = creat(name, 0666)) < 0) {
		fprintf(stderr, "%s: ", name);
		(void) fflush(stderr);
		fprintf(stderr, "cannot create file: %s\n", strerror(errno));
		return (FAIL);
	}
	rst_seekdir(dirp, itp->t_seekpt, itp->t_seekpt);
	dp = dup(dirp->dd_fd);
	for (i = itp->t_size; i > 0; i -= BUFSIZ) {
		size = i < BUFSIZ ? i : BUFSIZ;
		if (read(dp, buf, (int) size) == -1) {
			fprintf(stderr,
				"write error extracting inode %d, name %s\n",
				curfile.ino, curfile.name);
			fprintf(stderr, "read: %s\n", strerror(errno));
			done(1);
		}
		if (!Nflag && write(ofile, buf, (int) size) == -1) {
			fprintf(stderr,
				"write error extracting inode %d, name %s\n",
				curfile.ino, curfile.name);
			fprintf(stderr, "write: %s\n", strerror(errno));
			done(1);
		}
	}
	(void) close(dp);
	(void) close(ofile);
	return (GOOD);
}

/*
 * Determine the type of an inode
 */
int
inodetype(ino)
	ino_t ino;
{
	struct inotab *itp;

	itp = inotablookup(ino);
	if (itp == NULL)
		return (LEAF);
	return (NODE);
}

/*
 * Allocate and initialize a directory inode entry.
 * If requested, save its pertinent mode, owner, and time info.
 */
static struct inotab *
allocinotab(ino, dip, seekpt)
	ino_t ino;
	struct dinode *dip;
	long seekpt;
{
	register struct inotab	*itp;
	struct modeinfo node;

	itp = calloc(1, sizeof(struct inotab));
	if (itp == NULL)
		panic("no memory directory table\n");
	itp->t_next = inotab[INOHASH(ino)];
	inotab[INOHASH(ino)] = itp;
	itp->t_ino = ino;
	itp->t_seekpt = seekpt;
	if (mf == NULL)
		return (itp);
	node.ino = ino;
	node.timep[0].tv_sec = dip->di_atime.ts_sec;
	node.timep[0].tv_usec = dip->di_atime.ts_nsec / 1000;
	node.timep[1].tv_sec = dip->di_mtime.ts_sec;
	node.timep[1].tv_usec = dip->di_mtime.ts_nsec / 1000;
	node.mode = dip->di_mode;
	node.uid = dip->di_uid;
	node.gid = dip->di_gid;
	(void) fwrite((char *)&node, 1, sizeof(struct modeinfo), mf);
	return (itp);
}

/*
 * Look up an inode in the table of directories
 */
static struct inotab *
inotablookup(ino)
	ino_t	ino;
{
	register struct inotab *itp;

	for (itp = inotab[INOHASH(ino)]; itp != NULL; itp = itp->t_next)
		if (itp->t_ino == ino)
			return (itp);
	return (NULL);
}

/*
 * Clean up and exit
 */
void
done(exitcode)
	int exitcode;
{

	closemt();
	if (modefile[0] != '#')
		(void) unlink(modefile);
	if (dirfile[0] != '#')
		(void) unlink(dirfile);
	exit(exitcode);
}
