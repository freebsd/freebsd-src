/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)safefile.c	8.40 (Berkeley) 6/5/98";
#endif /* not lint */

# include "sendmail.h"
/*
**  SAFEFILE -- return true if a file exists and is safe for a user.
**
**	Parameters:
**		fn -- filename to check.
**		uid -- user id to compare against.
**		gid -- group id to compare against.
**		uname -- user name to compare against (used for group
**			sets).
**		flags -- modifiers:
**			SFF_MUSTOWN -- "uid" must own this file.
**			SFF_NOSLINK -- file cannot be a symbolic link.
**		mode -- mode bits that must match.
**		st -- if set, points to a stat structure that will
**			get the stat info for the file.
**
**	Returns:
**		0 if fn exists, is owned by uid, and matches mode.
**		An errno otherwise.  The actual errno is cleared.
**
**	Side Effects:
**		none.
*/

#include <grp.h>

int
safefile(fn, uid, gid, uname, flags, mode, st)
	char *fn;
	UID_T uid;
	GID_T gid;
	char *uname;
	int flags;
	int mode;
	struct stat *st;
{
	register char *p;
	register struct group *gr = NULL;
	int file_errno = 0;
	bool checkpath;
	struct stat stbuf;
	struct stat fstbuf;
	char fbuf[MAXPATHLEN + 1];

	if (tTd(44, 4))
		printf("safefile(%s, uid=%d, gid=%d, flags=%x, mode=%o):\n",
			fn, (int) uid, (int) gid, flags, mode);
	errno = 0;
	if (st == NULL)
		st = &fstbuf;
	if (strlen(fn) > sizeof fbuf - 1)
	{
		if (tTd(44, 4))
			printf("\tpathname too long\n");
		return ENAMETOOLONG;
	}
	strcpy(fbuf, fn);
	fn = fbuf;

	/* ignore SFF_SAFEDIRPATH if we are debugging */
	if (RealUid != 0 && RunAsUid == RealUid)
		flags &= ~SFF_SAFEDIRPATH;

	/* first check to see if the file exists at all */
#ifdef HASLSTAT
	if ((bitset(SFF_NOSLINK, flags) ? lstat(fn, st)
					: stat(fn, st)) < 0)
#else
	if (stat(fn, st) < 0)
#endif
	{
		file_errno = errno;
	}
	else if (bitset(SFF_SETUIDOK, flags) &&
		 !bitset(S_IXUSR|S_IXGRP|S_IXOTH, st->st_mode) &&
		 S_ISREG(st->st_mode))
	{
		/*
		**  If final file is setuid, run as the owner of that
		**  file.  Gotta be careful not to reveal anything too
		**  soon here!
		*/

#ifdef SUID_ROOT_FILES_OK
		if (bitset(S_ISUID, st->st_mode))
#else
		if (bitset(S_ISUID, st->st_mode) && st->st_uid != 0 &&
		    st->st_uid != TrustedFileUid)
#endif
		{
			uid = st->st_uid;
			uname = NULL;
		}
#ifdef SUID_ROOT_FILES_OK
		if (bitset(S_ISGID, st->st_mode))
#else
		if (bitset(S_ISGID, st->st_mode) && st->st_gid != 0)
#endif
			gid = st->st_gid;
	}

	checkpath = !bitset(SFF_NOPATHCHECK, flags) ||
		    (uid == 0 && !bitset(SFF_ROOTOK|SFF_OPENASROOT, flags));
	if (bitset(SFF_NOWLINK, flags) && !bitset(SFF_SAFEDIRPATH, flags))
	{
		int ret;

		/* check the directory */
		p = strrchr(fn, '/');
		if (p == NULL)
		{
			ret = safedirpath(".", uid, gid, uname, flags|SFF_SAFEDIRPATH);
		}
		else
		{
			*p = '\0';
			ret = safedirpath(fn, uid, gid, uname, flags|SFF_SAFEDIRPATH);
			*p = '/';
		}
		if (ret == 0)
		{
			/* directory is safe */
			checkpath = FALSE;
		}
		else
		{
#ifdef HASLSTAT
			/* Need lstat() information if called stat() before */
			if (!bitset(SFF_NOSLINK, flags) && lstat(fn, st) < 0)
			{
				ret = errno;
				if (tTd(44, 4))
					printf("\t%s\n", errstring(ret));
				return ret;
			}
#endif
			/* directory is writable: disallow links */
			flags |= SFF_NOLINK;
		}
	}

	if (checkpath)
	{
		int ret;

		p = strrchr(fn, '/');
		if (p == NULL)
		{
			ret = safedirpath(".", uid, gid, uname, flags);
		}
		else
		{
			*p = '\0';
			ret = safedirpath(fn, uid, gid, uname, flags);
			*p = '/';
		}
		if (ret != 0)
			return ret;
	}

	/*
	**  If the target file doesn't exist, check the directory to
	**  ensure that it is writable by this user.
	*/

	if (file_errno != 0)
	{
		int ret = file_errno;
		char *dir = fn;

		if (tTd(44, 4))
			printf("\t%s\n", errstring(ret));

		errno = 0;
		if (!bitset(SFF_CREAT, flags) || file_errno != ENOENT)
			return ret;

		/* check to see if legal to create the file */
		p = strrchr(dir, '/');
		if (p == NULL)
			dir = ".";
		else if (p == dir)
			dir = "/";
		else
			*p = '\0';
		if (stat(dir, &stbuf) >= 0)
		{
			int md = S_IWRITE|S_IEXEC;

			if (stbuf.st_uid == uid)
				;
			else if (uid == 0 && TrustedFileUid != 0 && stbuf.st_uid == TrustedFileUid)
				;
			else
			{
				md >>= 3;
				if (stbuf.st_gid == gid)
					;
#ifndef NO_GROUP_SET
				else if (uname != NULL && !DontInitGroups &&
			 		 ((gr != NULL &&
			 		   gr->gr_gid == stbuf.st_gid) ||
			  		  (gr = getgrgid(stbuf.st_gid)) != NULL))
				{
					register char **gp;
		
					for (gp = gr->gr_mem; *gp != NULL; gp++)
						if (strcmp(*gp, uname) == 0)
							break;
					if (*gp == NULL)
						md >>= 3;
				}
#endif
				else
					md >>= 3;
			}
			if ((stbuf.st_mode & md) != md)
				errno = EACCES;
		}
		ret = errno;
		if (tTd(44, 4))
			printf("\t[final dir %s uid %d mode %lo] %s\n",
				dir, (int) stbuf.st_uid, (u_long) stbuf.st_mode,
				errstring(ret));
		if (p != NULL)
			*p = '/';
		st->st_mode = ST_MODE_NOFILE;
		return ret;
	}

#ifdef S_ISLNK
	if (bitset(SFF_NOSLINK, flags) && S_ISLNK(st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[slink mode %lo]\tE_SM_NOSLINK\n",
				(u_long) st->st_mode);
		return E_SM_NOSLINK;
	}
#endif
	if (bitset(SFF_REGONLY, flags) && !S_ISREG(st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[non-reg mode %lo]\tE_SM_REGONLY\n",
				(u_long) st->st_mode);
		return E_SM_REGONLY;
	}
	if (bitset(SFF_NOGWFILES, flags) &&
	    bitset(S_IWGRP, st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[write bits %lo]\tE_SM_GWFILE\n",
			       (u_long) st->st_mode);
		return E_SM_GWFILE;
	}
	if (bitset(SFF_NOWWFILES, flags) &&
	    bitset(S_IWOTH, st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[write bits %lo]\tE_SM_WWFILE\n",
			       (u_long) st->st_mode);
		return E_SM_WWFILE;
	}
	if (bitset(S_IWUSR|S_IWGRP|S_IWOTH, mode) &&
	    bitset(S_IXUSR|S_IXGRP|S_IXOTH, st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[exec bits %lo]\tE_SM_ISEXEC]\n",
				(u_long) st->st_mode);
		return E_SM_ISEXEC;
	}
	if (bitset(SFF_NOHLINK, flags) && st->st_nlink != 1)
	{
		if (tTd(44, 4))
			printf("\t[link count %d]\tE_SM_NOHLINK\n",
				(int) st->st_nlink);
		return E_SM_NOHLINK;
	}

	if (uid == 0 && bitset(SFF_OPENASROOT, flags))
		;
	else if (uid == 0 && !bitset(SFF_ROOTOK, flags))
		mode >>= 6;
	else if (st->st_uid == uid)
		;
	else if (uid == 0 && TrustedFileUid != 0 && st->st_uid == TrustedFileUid)
		;
	else
	{
		mode >>= 3;
		if (st->st_gid == gid)
			;
#ifndef NO_GROUP_SET
		else if (uname != NULL && !DontInitGroups &&
			 ((gr != NULL && gr->gr_gid == st->st_gid) ||
			  (gr = getgrgid(st->st_gid)) != NULL))
		{
			register char **gp;

			for (gp = gr->gr_mem; *gp != NULL; gp++)
				if (strcmp(*gp, uname) == 0)
					break;
			if (*gp == NULL)
				mode >>= 3;
		}
#endif
		else
			mode >>= 3;
	}
	if (tTd(44, 4))
		printf("\t[uid %d, nlink %d, stat %lo, mode %lo] ",
			(int) st->st_uid, (int) st->st_nlink,
			(u_long) st->st_mode, (u_long) mode);
	if ((st->st_uid == uid || st->st_uid == 0 ||
	     st->st_uid == TrustedFileUid ||
	     !bitset(SFF_MUSTOWN, flags)) &&
	    (st->st_mode & mode) == mode)
	{
		if (tTd(44, 4))
			printf("\tOK\n");
		return 0;
	}
	if (tTd(44, 4))
		printf("\tEACCES\n");
	return EACCES;
}
/*
**  SAFEDIRPATH -- check to make sure a path to a directory is safe
**
**	Safe means not writable and owned by the right folks.
**
**	Parameters:
**		fn -- filename to check.
**		uid -- user id to compare against.
**		gid -- group id to compare against.
**		uname -- user name to compare against (used for group
**			sets).
**		flags -- modifiers:
**			SFF_ROOTOK -- ok to use root permissions to open.
**			SFF_SAFEDIRPATH -- writable directories are considered
**				to be fatal errors.
**
**	Returns:
**		0 -- if the directory path is "safe".
**		else -- an error number associated with the path.
*/

int
safedirpath(fn, uid, gid, uname, flags)
	char *fn;
	UID_T uid;
	GID_T gid;
	char *uname;
	int flags;
{
	char *p;
	register struct group *gr = NULL;
	int ret = 0;
	int mode = S_IWOTH;
	struct stat stbuf;

	/* special case root directory */
	if (*fn == '\0')
		fn = "/";

	if (tTd(44, 4))
		printf("safedirpath(%s, uid=%ld, gid=%ld, flags=%x):\n",
			fn, (long) uid, (long) gid, flags);

	if (!bitset(DBS_GROUPWRITABLEDIRPATHSAFE, DontBlameSendmail))
		mode |= S_IWGRP;

	p = fn;
	do
	{
		if (*p == '\0')
			*p = '/';
		p = strchr(++p, '/');
		if (p != NULL)
			*p = '\0';
		if (stat(fn, &stbuf) < 0)
		{
			ret = errno;
			break;
		}
		if ((uid == 0 || bitset(SFF_SAFEDIRPATH, flags)) &&
		    bitset(mode, stbuf.st_mode))
		{
			if (tTd(44, 4))
				printf("\t[dir %s] mode %lo\n",
					fn, (u_long) stbuf.st_mode);
			if (bitset(SFF_SAFEDIRPATH, flags))
			{
				if (bitset(S_IWOTH, stbuf.st_mode))
					ret = E_SM_WWDIR;
				else
					ret = E_SM_GWDIR;
				break;
			}
			if (Verbose > 1)
				message("051 WARNING: %s writable directory %s",
					bitset(S_IWOTH, stbuf.st_mode)
					   ? "World"
					   : "Group",
					fn);
		}
		if (uid == 0 && !bitset(SFF_ROOTOK|SFF_OPENASROOT, flags))
		{
			if (bitset(S_IXOTH, stbuf.st_mode))
				continue;
			ret = EACCES;
			break;
		}

		/*
		** Let OS determine access to file if we are not
		** running as a privileged user.  This allows ACLs
		** to work.
		*/
		if (geteuid() != 0)
			continue;

		if (stbuf.st_uid == uid &&
		    bitset(S_IXUSR, stbuf.st_mode))
			continue;
		if (stbuf.st_gid == gid &&
		    bitset(S_IXGRP, stbuf.st_mode))
			continue;
#ifndef NO_GROUP_SET
		if (uname != NULL && !DontInitGroups &&
		    ((gr != NULL && gr->gr_gid == stbuf.st_gid) ||
		     (gr = getgrgid(stbuf.st_gid)) != NULL))
		{
			register char **gp;

			for (gp = gr->gr_mem; gp != NULL && *gp != NULL; gp++)
				if (strcmp(*gp, uname) == 0)
					break;
			if (gp != NULL && *gp != NULL &&
			    bitset(S_IXGRP, stbuf.st_mode))
				continue;
		}
#endif
		if (!bitset(S_IXOTH, stbuf.st_mode))
		{
			ret = EACCES;
			break;
		}
	} while (p != NULL);
	if (ret != 0 && tTd(44, 4))
		printf("\t[dir %s] %s\n", fn, errstring(ret));
	if (p != NULL)
		*p = '/';
	return ret;
}
/*
**  SAFEOPEN -- do a file open with extra checking
**
**	Parameters:
**		fn -- the file name to open.
**		omode -- the open-style mode flags.
**		cmode -- the create-style mode flags.
**		sff -- safefile flags.
**
**	Returns:
**		Same as open.
*/

#ifndef O_ACCMODE
# define O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif

int
safeopen(fn, omode, cmode, sff)
	char *fn;
	int omode;
	int cmode;
	int sff;
{
	int rval;
	int fd;
	int smode;
	struct stat stb;

	if (bitset(O_CREAT, omode))
		sff |= SFF_CREAT;
	omode &= ~O_CREAT;
	smode = 0;
	switch (omode & O_ACCMODE)
	{
	  case O_RDONLY:
		smode = S_IREAD;
		break;

	  case O_WRONLY:
		smode = S_IWRITE;
		break;

	  case O_RDWR:
		smode = S_IREAD|S_IWRITE;
		break;

	  default:
		smode = 0;
		break;
	}
	if (bitset(SFF_OPENASROOT, sff))
		rval = safefile(fn, RunAsUid, RunAsGid, RunAsUserName,
				sff, smode, &stb);
	else
		rval = safefile(fn, RealUid, RealGid, RealUserName,
				sff, smode, &stb);
	if (rval != 0)
	{
		errno = rval;
		return -1;
	}
	if (stb.st_mode == ST_MODE_NOFILE && bitset(SFF_CREAT, sff))
		omode |= O_EXCL|O_CREAT;

	fd = dfopen(fn, omode, cmode, sff);
	if (fd < 0)
		return fd;
	if (filechanged(fn, fd, &stb))
	{
		syserr("554 cannot open: file %s changed after open", fn);
		close(fd);
		errno = E_SM_FILECHANGE;
		return -1;
	}
	return fd;
}
/*
**  SAFEFOPEN -- do a file open with extra checking
**
**	Parameters:
**		fn -- the file name to open.
**		omode -- the open-style mode flags.
**		cmode -- the create-style mode flags.
**		sff -- safefile flags.
**
**	Returns:
**		Same as fopen.
*/

FILE *
safefopen(fn, omode, cmode, sff)
	char *fn;
	int omode;
	int cmode;
	int sff;
{
	int fd;
	FILE *fp;
	char *fmode;

	switch (omode & O_ACCMODE)
	{
	  case O_RDONLY:
		fmode = "r";
		break;

	  case O_WRONLY:
		if (bitset(O_APPEND, omode))
			fmode = "a";
		else
			fmode = "w";
		break;

	  case O_RDWR:
		if (bitset(O_TRUNC, omode))
			fmode = "w+";
		else if (bitset(O_APPEND, omode))
			fmode = "a+";
		else
			fmode = "r+";
		break;

	  default:
		syserr("safefopen: unknown omode %o", omode);
		fmode = "x";
	}
	fd = safeopen(fn, omode, cmode, sff);
	if (fd < 0)
	{
		if (tTd(44, 10))
			printf("safefopen: safeopen failed: %s\n",
				errstring(errno));
		return NULL;
	}
	fp = fdopen(fd, fmode);
	if (fp != NULL)
		return fp;

	if (tTd(44, 10))
	{
		printf("safefopen: fdopen(%s, %s) failed: omode=%x, sff=%x, err=%s\n",
			fn, fmode, omode, sff, errstring(errno));
#ifndef NOT_SENDMAIL
		dumpfd(fd, TRUE, FALSE);
#endif
	}
	(void) close(fd);
	return NULL;
}
/*
**  FILECHANGED -- check to see if file changed after being opened
**
**	Parameters:
**		fn -- pathname of file to check.
**		fd -- file descriptor to check.
**		stb -- stat structure from before open.
**
**	Returns:
**		TRUE -- if a problem was detected.
**		FALSE -- if this file is still the same.
*/

bool
filechanged(fn, fd, stb)
	char *fn;
	int fd;
	struct stat *stb;
{
	struct stat sta;

	if (stb->st_mode == ST_MODE_NOFILE)
	{
#if HASLSTAT && BOGUS_O_EXCL
		/* only necessary if exclusive open follows symbolic links */
		if (lstat(fn, stb) < 0 || stb->st_nlink != 1)
			return TRUE;
#else
		return FALSE;
#endif
	}
	if (fstat(fd, &sta) < 0)
		return TRUE;

	if (sta.st_nlink != stb->st_nlink ||
	    sta.st_dev != stb->st_dev ||
	    sta.st_ino != stb->st_ino ||
#if HAS_ST_GEN && 0		/* AFS returns garbage in st_gen */
	    sta.st_gen != stb->st_gen ||
#endif
	    sta.st_uid != stb->st_uid ||
	    sta.st_gid != stb->st_gid)
	{
		if (tTd(44, 8))
		{
			printf("File changed after opening:\n");
			printf(" nlink	= %ld/%ld\n",
				(long) stb->st_nlink, (long) sta.st_nlink);
			printf(" dev	= %ld/%ld\n",
				(long) stb->st_dev, (long) sta.st_dev);
			if (sizeof sta.st_ino > sizeof (long))
			{
				printf(" ino	= %s/", 
					quad_to_string(stb->st_ino));
				printf("%s\n",
					quad_to_string(sta.st_ino));
			}
			else
				printf(" ino	= %lu/%lu\n",
					(unsigned long) stb->st_ino,
					(unsigned long) sta.st_ino);
#if HAS_ST_GEN
			printf(" gen	= %ld/%ld\n",
				(long) stb->st_gen, (long) sta.st_gen);
#endif
			printf(" uid	= %ld/%ld\n",
				(long) stb->st_uid, (long) sta.st_uid);
			printf(" gid	= %ld/%ld\n",
				(long) stb->st_gid, (long) sta.st_gid);
		}
		return TRUE;
	}

	return FALSE;
}
/*
**  DFOPEN -- determined file open
**
**	This routine has the semantics of open, except that it will
**	keep trying a few times to make this happen.  The idea is that
**	on very loaded systems, we may run out of resources (inodes,
**	whatever), so this tries to get around it.
*/

int
dfopen(filename, omode, cmode, sff)
	char *filename;
	int omode;
	int cmode;
	int sff;
{
	register int tries;
	int fd;
	struct stat st;

	for (tries = 0; tries < 10; tries++)
	{
		sleep((unsigned) (10 * tries));
		errno = 0;
		fd = open(filename, omode, cmode);
		if (fd >= 0)
			break;
		switch (errno)
		{
		  case ENFILE:		/* system file table full */
		  case EINTR:		/* interrupted syscall */
#ifdef ETXTBSY
		  case ETXTBSY:		/* Apollo: net file locked */
#endif
			continue;
		}
		break;
	}
	if (!bitset(SFF_NOLOCK, sff) &&
	    fd >= 0 &&
	    fstat(fd, &st) >= 0 &&
	    S_ISREG(st.st_mode))
	{
		int locktype;

		/* lock the file to avoid accidental conflicts */
		if ((omode & O_ACCMODE) != O_RDONLY)
			locktype = LOCK_EX;
		else
			locktype = LOCK_SH;
		(void) lockfile(fd, filename, NULL, locktype);
		errno = 0;
	}
	return fd;
}
