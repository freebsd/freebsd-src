/*
 * Copyright (c) 1983, 1995, 1996 Eric P. Allman
 * Copyright (c) 1988, 1993
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
static char sccsid[] = "@(#)util.c	8.113 (Berkeley) 11/24/96";
#endif /* not lint */

# include "sendmail.h"
# include <sysexits.h>
/*
**  STRIPQUOTES -- Strip quotes & quote bits from a string.
**
**	Runs through a string and strips off unquoted quote
**	characters and quote bits.  This is done in place.
**
**	Parameters:
**		s -- the string to strip.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
**
**	Called By:
**		deliver
*/

void
stripquotes(s)
	char *s;
{
	register char *p;
	register char *q;
	register char c;

	if (s == NULL)
		return;

	p = q = s;
	do
	{
		c = *p++;
		if (c == '\\')
			c = *p++;
		else if (c == '"')
			continue;
		*q++ = c;
	} while (c != '\0');
}
/*
**  XALLOC -- Allocate memory and bitch wildly on failure.
**
**	THIS IS A CLUDGE.  This should be made to give a proper
**	error -- but after all, what can we do?
**
**	Parameters:
**		sz -- size of area to allocate.
**
**	Returns:
**		pointer to data region.
**
**	Side Effects:
**		Memory is allocated.
*/

char *
xalloc(sz)
	register int sz;
{
	register char *p;

	/* some systems can't handle size zero mallocs */
	if (sz <= 0)
		sz = 1;

	p = malloc((unsigned) sz);
	if (p == NULL)
	{
		syserr("!Out of memory!!");
		/* exit(EX_UNAVAILABLE); */
	}
	return (p);
}
/*
**  COPYPLIST -- copy list of pointers.
**
**	This routine is the equivalent of newstr for lists of
**	pointers.
**
**	Parameters:
**		list -- list of pointers to copy.
**			Must be NULL terminated.
**		copycont -- if TRUE, copy the contents of the vector
**			(which must be a string) also.
**
**	Returns:
**		a copy of 'list'.
**
**	Side Effects:
**		none.
*/

char **
copyplist(list, copycont)
	char **list;
	bool copycont;
{
	register char **vp;
	register char **newvp;

	for (vp = list; *vp != NULL; vp++)
		continue;

	vp++;

	newvp = (char **) xalloc((int) (vp - list) * sizeof *vp);
	bcopy((char *) list, (char *) newvp, (int) (vp - list) * sizeof *vp);

	if (copycont)
	{
		for (vp = newvp; *vp != NULL; vp++)
			*vp = newstr(*vp);
	}

	return (newvp);
}
/*
**  COPYQUEUE -- copy address queue.
**
**	This routine is the equivalent of newstr for address queues
**	addresses marked with QDONTSEND aren't copied
**
**	Parameters:
**		addr -- list of address structures to copy.
**
**	Returns:
**		a copy of 'addr'.
**
**	Side Effects:
**		none.
*/

ADDRESS *
copyqueue(addr)
	ADDRESS *addr;
{
	register ADDRESS *newaddr;
	ADDRESS *ret;
	register ADDRESS **tail = &ret;

	while (addr != NULL)
	{
		if (!bitset(QDONTSEND, addr->q_flags))
		{
			newaddr = (ADDRESS *) xalloc(sizeof(ADDRESS));
			STRUCTCOPY(*addr, *newaddr);
			*tail = newaddr;
			tail = &newaddr->q_next;
		}
		addr = addr->q_next;
	}
	*tail = NULL;
	
	return ret;
}
/*
**  PRINTAV -- print argument vector.
**
**	Parameters:
**		av -- argument vector.
**
**	Returns:
**		none.
**
**	Side Effects:
**		prints av.
*/

void
printav(av)
	register char **av;
{
	while (*av != NULL)
	{
		if (tTd(0, 44))
			printf("\n\t%08lx=", (u_long) *av);
		else
			(void) putchar(' ');
		xputs(*av++);
	}
	(void) putchar('\n');
}
/*
**  LOWER -- turn letter into lower case.
**
**	Parameters:
**		c -- character to turn into lower case.
**
**	Returns:
**		c, in lower case.
**
**	Side Effects:
**		none.
*/

char
lower(c)
	register char c;
{
	return((isascii(c) && isupper(c)) ? tolower(c) : c);
}
/*
**  XPUTS -- put string doing control escapes.
**
**	Parameters:
**		s -- string to put.
**
**	Returns:
**		none.
**
**	Side Effects:
**		output to stdout
*/

void
xputs(s)
	register const char *s;
{
	register int c;
	register struct metamac *mp;
	bool shiftout = FALSE;
	extern struct metamac MetaMacros[];

	if (s == NULL)
	{
		printf("%s<null>%s", TermEscape.te_rv_on, TermEscape.te_rv_off);
		return;
	}
	while ((c = (*s++ & 0377)) != '\0')
	{
		if (shiftout)
		{
			printf("%s", TermEscape.te_rv_off);
			shiftout = FALSE;
		}
		if (!isascii(c))
		{
			if (c == MATCHREPL)
			{
				printf("%s$", TermEscape.te_rv_on);
				shiftout = TRUE;
				if (*s == '\0')
					continue;
				c = *s++ & 0377;
				goto printchar;
			}
			if (c == MACROEXPAND)
			{
				printf("%s$", TermEscape.te_rv_on);
				shiftout = TRUE;
				if (strchr("=~&?", *s) != NULL)
					putchar(*s++);
				if (bitset(0200, *s))
					printf("{%s}", macname(*s++ & 0377));
				else
					printf("%c", *s++);
				continue;
			}
			for (mp = MetaMacros; mp->metaname != '\0'; mp++)
			{
				if ((mp->metaval & 0377) == c)
				{
					printf("%s$%c",
						TermEscape.te_rv_on,
						mp->metaname);
					shiftout = TRUE;
					break;
				}
			}
			if (c == MATCHCLASS || c == MATCHNCLASS)
			{
				if (bitset(0200, *s))
					printf("{%s}", macname(*s++ & 0377));
				else
					printf("%c", *s++);
			}
			if (mp->metaname != '\0')
				continue;

			/* unrecognized meta character */
			printf("%sM-", TermEscape.te_rv_on);
			shiftout = TRUE;
			c &= 0177;
		}
  printchar:
		if (isprint(c))
		{
			putchar(c);
			continue;
		}

		/* wasn't a meta-macro -- find another way to print it */
		switch (c)
		{
		  case '\n':
			c = 'n';
			break;

		  case '\r':
			c = 'r';
			break;

		  case '\t':
			c = 't';
			break;
		}
		if (!shiftout)
		{
			printf("%s", TermEscape.te_rv_on);
			shiftout = TRUE;
		}
		if (isprint(c))
		{
			(void) putchar('\\');
			(void) putchar(c);
		}
		else
		{
			(void) putchar('^');
			(void) putchar(c ^ 0100);
		}
	}
	if (shiftout)
		printf("%s", TermEscape.te_rv_off);
	(void) fflush(stdout);
}
/*
**  MAKELOWER -- Translate a line into lower case
**
**	Parameters:
**		p -- the string to translate.  If NULL, return is
**			immediate.
**
**	Returns:
**		none.
**
**	Side Effects:
**		String pointed to by p is translated to lower case.
**
**	Called By:
**		parse
*/

void
makelower(p)
	register char *p;
{
	register char c;

	if (p == NULL)
		return;
	for (; (c = *p) != '\0'; p++)
		if (isascii(c) && isupper(c))
			*p = tolower(c);
}
/*
**  BUILDFNAME -- build full name from gecos style entry.
**
**	This routine interprets the strange entry that would appear
**	in the GECOS field of the password file.
**
**	Parameters:
**		p -- name to build.
**		login -- the login name of this user (for &).
**		buf -- place to put the result.
**		buflen -- length of buf.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

void
buildfname(gecos, login, buf, buflen)
	register char *gecos;
	char *login;
	char *buf;
	int buflen;
{
	register char *p;
	register char *bp = buf;

	if (*gecos == '*')
		gecos++;

	/* copy gecos, interpolating & to be full name */
	for (p = gecos; *p != '\0' && *p != ',' && *p != ';' && *p != '%'; p++)
	{
		if (bp >= &buf[buflen - 1])
		{
			/* buffer overflow -- just use login name */
			snprintf(buf, buflen, "%s", login);
			return;
		}
		if (*p == '&')
		{
			/* interpolate full name */
			snprintf(bp, buflen - (bp - buf), "%s", login);
			*bp = toupper(*bp);
			bp += strlen(bp);
		}
		else
			*bp++ = *p;
	}
	*bp = '\0';
}
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

#ifndef S_IXOTH
# define S_IXOTH	(S_IEXEC >> 6)
#endif

#ifndef S_IXGRP
# define S_IXGRP	(S_IEXEC >> 3)
#endif

#ifndef S_IXUSR
# define S_IXUSR	(S_IEXEC)
#endif

#define ST_MODE_NOFILE	0171147		/* unlikely to occur */

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
	struct stat stbuf;
	struct stat fstbuf;

	if (tTd(44, 4))
		printf("safefile(%s, uid=%d, gid=%d, flags=%x, mode=%o):\n",
			fn, (int) uid, (int) gid, flags, mode);
	errno = 0;
	if (st == NULL)
		st = &fstbuf;

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
		if (bitset(S_ISUID, st->st_mode) && st->st_uid != 0)
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

	if (!bitset(SFF_NOPATHCHECK, flags) ||
	    (uid == 0 && !bitset(SFF_ROOTOK, flags)))
	{
		/* check the path to the file for acceptability */
		for (p = fn; (p = strchr(++p, '/')) != NULL; *p = '/')
		{
			*p = '\0';
			if (stat(fn, &stbuf) < 0)
				break;
			if (uid == 0 && bitset(S_IWGRP|S_IWOTH, stbuf.st_mode))
				message("051 WARNING: writable directory %s",
					fn);
			if (uid == 0 && !bitset(SFF_ROOTOK, flags))
			{
				if (bitset(S_IXOTH, stbuf.st_mode))
					continue;
				break;
			}
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
				break;
		}
		if (p != NULL)
		{
			int ret = errno;

			if (ret == 0)
				ret = EACCES;
			if (tTd(44, 4))
				printf("\t[dir %s] %s\n", fn, errstring(ret));
			*p = '/';
			return ret;
		}
	}

	/*
	**  If the target file doesn't exist, check the directory to
	**  ensure that it is writable by this user.
	*/

	if (file_errno != 0)
	{
		int ret = file_errno;

		if (tTd(44, 4))
			printf("\t%s\n", errstring(ret));

		errno = 0;
		if (!bitset(SFF_CREAT, flags))
			return ret;

		/* check to see if legal to create the file */
		p = strrchr(fn, '/');
		if (p == NULL)
			return ENOTDIR;
		*p = '\0';
		if (stat(fn, &stbuf) >= 0)
		{
			int md = S_IWRITE|S_IEXEC;
			if (stbuf.st_uid != uid)
				md >>= 6;
			if ((stbuf.st_mode & md) != md)
				errno = EACCES;
		}
		ret = errno;
		if (tTd(44, 4))
			printf("\t[final dir %s uid %d mode %lo] %s\n",
				fn, (int) stbuf.st_uid, (u_long) stbuf.st_mode,
				errstring(ret));
		*p = '/';
		st->st_mode = ST_MODE_NOFILE;
		return ret;
	}

#ifdef S_ISLNK
	if (bitset(SFF_NOSLINK, flags) && S_ISLNK(st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[slink mode %o]\tEPERM\n", st->st_mode);
		return EPERM;
	}
#endif
	if (bitset(SFF_REGONLY, flags) && !S_ISREG(st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[non-reg mode %o]\tEPERM\n", st->st_mode);
		return EPERM;
	}
	if (bitset(S_IWUSR|S_IWGRP|S_IWOTH, mode) &&
	    bitset(S_IXUSR|S_IXGRP|S_IXOTH, st->st_mode))
	{
		if (tTd(44, 4))
			printf("\t[exec bits %o]\tEPERM]\n", st->st_mode);
		return EPERM;
	}
	if (st->st_nlink > 1)
	{
		if (tTd(44, 4))
			printf("\t[link count %d]\tEPERM\n", st->st_nlink);
		return EPERM;
	}

	if (uid == 0 && !bitset(SFF_ROOTOK, flags))
		mode >>= 6;
	else if (st->st_uid != uid)
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

#ifndef O_ACCMODE
# define O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif

FILE *
safefopen(fn, omode, cmode, sff)
	char *fn;
	int omode;
	int cmode;
	int sff;
{
	int rval;
	FILE *fp;
	int smode;
	struct stat stb, sta;

	if (bitset(O_CREAT, omode))
		sff |= SFF_CREAT;
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
		rval = safefile(fn, 0, 0, NULL, sff, smode, &stb);
	else
		rval = safefile(fn, RealUid, RealGid, RealUserName,
				sff, smode, &stb);
	if (rval != 0)
	{
		errno = rval;
		return NULL;
	}
	if (stb.st_mode == ST_MODE_NOFILE)
		omode |= O_EXCL;

	fp = dfopen(fn, omode, cmode);
	if (fp == NULL)
		return NULL;
	if (bitset(O_EXCL, omode))
		return fp;
	if (fstat(fileno(fp), &sta) < 0 ||
	    sta.st_nlink != stb.st_nlink ||
	    sta.st_dev != stb.st_dev ||
	    sta.st_ino != stb.st_ino ||
	    sta.st_uid != stb.st_uid ||
	    sta.st_gid != stb.st_gid)
	{
		syserr("554 cannot open: file %s changed after open", fn);
		fclose(fp);
		errno = EPERM;
		return NULL;
	}
	return fp;
}
/*
**  FIXCRLF -- fix <CR><LF> in line.
**
**	Looks for the <CR><LF> combination and turns it into the
**	UNIX canonical <NL> character.  It only takes one line,
**	i.e., it is assumed that the first <NL> found is the end
**	of the line.
**
**	Parameters:
**		line -- the line to fix.
**		stripnl -- if true, strip the newline also.
**
**	Returns:
**		none.
**
**	Side Effects:
**		line is changed in place.
*/

void
fixcrlf(line, stripnl)
	char *line;
	bool stripnl;
{
	register char *p;

	p = strchr(line, '\n');
	if (p == NULL)
		return;
	if (p > line && p[-1] == '\r')
		p--;
	if (!stripnl)
		*p++ = '\n';
	*p = '\0';
}
/*
**  DFOPEN -- determined file open
**
**	This routine has the semantics of fopen, except that it will
**	keep trying a few times to make this happen.  The idea is that
**	on very loaded systems, we may run out of resources (inodes,
**	whatever), so this tries to get around it.
*/

struct omodes
{
	int	mask;
	int	mode;
	char	*farg;
} OpenModes[] =
{
	{ O_ACCMODE,		O_RDONLY,		"r"	},
	{ O_ACCMODE|O_APPEND,	O_WRONLY,		"w"	},
	{ O_ACCMODE|O_APPEND,	O_WRONLY|O_APPEND,	"a"	},
	{ O_TRUNC,		0,			"w+"	},
	{ O_APPEND,		O_APPEND,		"a+"	},
	{ 0,			0,			"r+"	},
};

FILE *
dfopen(filename, omode, cmode)
	char *filename;
	int omode;
	int cmode;
{
	register int tries;
	int fd;
	register struct omodes *om;
	struct stat st;

	for (om = OpenModes; om->mask != 0; om++)
		if ((omode & om->mask) == om->mode)
			break;

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
	if (fd >= 0 && fstat(fd, &st) >= 0 && S_ISREG(st.st_mode))
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
	if (fd < 0)
		return NULL;
	else
		return fdopen(fd, om->farg);
}
/*
**  PUTLINE -- put a line like fputs obeying SMTP conventions
**
**	This routine always guarantees outputing a newline (or CRLF,
**	as appropriate) at the end of the string.
**
**	Parameters:
**		l -- line to put.
**		mci -- the mailer connection information.
**
**	Returns:
**		none
**
**	Side Effects:
**		output of l to fp.
*/

void
putline(l, mci)
	register char *l;
	register MCI *mci;
{
	putxline(l, mci, PXLF_MAPFROM);
}
/*
**  PUTXLINE -- putline with flags bits.
**
**	This routine always guarantees outputing a newline (or CRLF,
**	as appropriate) at the end of the string.
**
**	Parameters:
**		l -- line to put.
**		mci -- the mailer connection information.
**		pxflags -- flag bits:
**		    PXLF_MAPFROM -- map From_ to >From_.
**		    PXLF_STRIP8BIT -- strip 8th bit.
**
**	Returns:
**		none
**
**	Side Effects:
**		output of l to fp.
*/

void
putxline(l, mci, pxflags)
	register char *l;
	register MCI *mci;
	int pxflags;
{
	register char *p;
	register char svchar;
	int slop = 0;

	/* strip out 0200 bits -- these can look like TELNET protocol */
	if (bitset(MCIF_7BIT, mci->mci_flags) ||
	    bitset(PXLF_STRIP8BIT, pxflags))
	{
		for (p = l; (svchar = *p) != '\0'; ++p)
			if (bitset(0200, svchar))
				*p = svchar &~ 0200;
	}

	do
	{
		/* find the end of the line */
		p = strchr(l, '\n');
		if (p == NULL)
			p = &l[strlen(l)];

		if (TrafficLogFile != NULL)
			fprintf(TrafficLogFile, "%05d >>> ", (int) getpid());

		/* check for line overflow */
		while (mci->mci_mailer->m_linelimit > 0 &&
		       (p - l + slop) > mci->mci_mailer->m_linelimit)
		{
			register char *q = &l[mci->mci_mailer->m_linelimit - slop - 1];

			svchar = *q;
			*q = '\0';
			if (l[0] == '.' && slop == 0 &&
			    bitnset(M_XDOT, mci->mci_mailer->m_flags))
			{
				(void) putc('.', mci->mci_out);
				if (TrafficLogFile != NULL)
					(void) putc('.', TrafficLogFile);
			}
			else if (l[0] == 'F' && slop == 0 &&
				 bitset(PXLF_MAPFROM, pxflags) &&
				 strncmp(l, "From ", 5) == 0 &&
				 bitnset(M_ESCFROM, mci->mci_mailer->m_flags))
			{
				(void) putc('>', mci->mci_out);
				if (TrafficLogFile != NULL)
					(void) putc('>', TrafficLogFile);
			}
			fputs(l, mci->mci_out);
			(void) putc('!', mci->mci_out);
			fputs(mci->mci_mailer->m_eol, mci->mci_out);
			(void) putc(' ', mci->mci_out);
			if (TrafficLogFile != NULL)
				fprintf(TrafficLogFile, "%s!\n%05d >>>  ",
					l, (int) getpid());
			*q = svchar;
			l = q;
			slop = 1;
		}

		/* output last part */
		if (l[0] == '.' && slop == 0 &&
		    bitnset(M_XDOT, mci->mci_mailer->m_flags))
		{
			(void) putc('.', mci->mci_out);
			if (TrafficLogFile != NULL)
				(void) putc('.', TrafficLogFile);
		}
		else if (l[0] == 'F' && slop == 0 &&
			 bitset(PXLF_MAPFROM, pxflags) &&
			 strncmp(l, "From ", 5) == 0 &&
			 bitnset(M_ESCFROM, mci->mci_mailer->m_flags))
		{
			(void) putc('>', mci->mci_out);
			if (TrafficLogFile != NULL)
				(void) putc('>', TrafficLogFile);
		}
		if (TrafficLogFile != NULL)
			fprintf(TrafficLogFile, "%.*s\n", p - l, l);
		for ( ; l < p; ++l)
			(void) putc(*l, mci->mci_out);
		fputs(mci->mci_mailer->m_eol, mci->mci_out);
		if (*l == '\n')
		{
			if (*++l != ' ' && *l != '\t' && *l != '\0')
			{
				(void) putc(' ', mci->mci_out);
				if (TrafficLogFile != NULL)
					(void) putc(' ', TrafficLogFile);
			}
		}
	} while (l[0] != '\0');
}
/*
**  XUNLINK -- unlink a file, doing logging as appropriate.
**
**	Parameters:
**		f -- name of file to unlink.
**
**	Returns:
**		none.
**
**	Side Effects:
**		f is unlinked.
*/

void
xunlink(f)
	char *f;
{
	register int i;

# ifdef LOG
	if (LogLevel > 98)
		syslog(LOG_DEBUG, "%s: unlink %s", CurEnv->e_id, f);
# endif /* LOG */

	i = unlink(f);
# ifdef LOG
	if (i < 0 && LogLevel > 97)
		syslog(LOG_DEBUG, "%s: unlink-fail %d", f, errno);
# endif /* LOG */
}
/*
**  XFCLOSE -- close a file, doing logging as appropriate.
**
**	Parameters:
**		fp -- file pointer for the file to close
**		a, b -- miscellaneous crud to print for debugging
**
**	Returns:
**		none.
**
**	Side Effects:
**		fp is closed.
*/

void
xfclose(fp, a, b)
	FILE *fp;
	char *a, *b;
{
	if (tTd(53, 99))
		printf("xfclose(%lx) %s %s\n", (u_long) fp, a, b);
#if XDEBUG
	if (fileno(fp) == 1)
		syserr("xfclose(%s %s): fd = 1", a, b);
#endif
	if (fclose(fp) < 0 && tTd(53, 99))
		printf("xfclose FAILURE: %s\n", errstring(errno));
}
/*
**  SFGETS -- "safe" fgets -- times out and ignores random interrupts.
**
**	Parameters:
**		buf -- place to put the input line.
**		siz -- size of buf.
**		fp -- file to read from.
**		timeout -- the timeout before error occurs.
**		during -- what we are trying to read (for error messages).
**
**	Returns:
**		NULL on error (including timeout).  This will also leave
**			buf containing a null string.
**		buf otherwise.
**
**	Side Effects:
**		none.
*/

static jmp_buf	CtxReadTimeout;
static void	readtimeout();

char *
sfgets(buf, siz, fp, timeout, during)
	char *buf;
	int siz;
	FILE *fp;
	time_t timeout;
	char *during;
{
	register EVENT *ev = NULL;
	register char *p;

	if (fp == NULL)
	{
		buf[0] = '\0';
		return NULL;
	}

	/* set the timeout */
	if (timeout != 0)
	{
		if (setjmp(CtxReadTimeout) != 0)
		{
# ifdef LOG
			if (LogLevel > 1)
				syslog(LOG_NOTICE,
				       "timeout waiting for input from %.100s during %s",
				       CurHostName ? CurHostName : "local",
				       during);
# endif
			errno = 0;
			usrerr("451 timeout waiting for input during %s",
				during);
			buf[0] = '\0';
#if XDEBUG
			checkfd012(during);
#endif
			return (NULL);
		}
		ev = setevent(timeout, readtimeout, 0);
	}

	/* try to read */
	p = NULL;
	while (!feof(fp) && !ferror(fp))
	{
		errno = 0;
		p = fgets(buf, siz, fp);
		if (p != NULL || errno != EINTR)
			break;
		clearerr(fp);
	}

	/* clear the event if it has not sprung */
	clrevent(ev);

	/* clean up the books and exit */
	LineNumber++;
	if (p == NULL)
	{
		buf[0] = '\0';
		if (TrafficLogFile != NULL)
			fprintf(TrafficLogFile, "%05d <<< [EOF]\n", (int) getpid());
		return (NULL);
	}
	if (TrafficLogFile != NULL)
		fprintf(TrafficLogFile, "%05d <<< %s", (int) getpid(), buf);
	if (SevenBitInput)
	{
		for (p = buf; *p != '\0'; p++)
			*p &= ~0200;
	}
	else if (!HasEightBits)
	{
		for (p = buf; *p != '\0'; p++)
		{
			if (bitset(0200, *p))
			{
				HasEightBits = TRUE;
				break;
			}
		}
	}
	return (buf);
}

static void
readtimeout(timeout)
	time_t timeout;
{
	longjmp(CtxReadTimeout, 1);
}
/*
**  FGETFOLDED -- like fgets, but know about folded lines.
**
**	Parameters:
**		buf -- place to put result.
**		n -- bytes available.
**		f -- file to read from.
**
**	Returns:
**		input line(s) on success, NULL on error or EOF.
**		This will normally be buf -- unless the line is too
**			long, when it will be xalloc()ed.
**
**	Side Effects:
**		buf gets lines from f, with continuation lines (lines
**		with leading white space) appended.  CRLF's are mapped
**		into single newlines.  Any trailing NL is stripped.
*/

char *
fgetfolded(buf, n, f)
	char *buf;
	register int n;
	FILE *f;
{
	register char *p = buf;
	char *bp = buf;
	register int i;

	n--;
	while ((i = getc(f)) != EOF)
	{
		if (i == '\r')
		{
			i = getc(f);
			if (i != '\n')
			{
				if (i != EOF)
					(void) ungetc(i, f);
				i = '\r';
			}
		}
		if (--n <= 0)
		{
			/* allocate new space */
			char *nbp;
			int nn;

			nn = (p - bp);
			if (nn < MEMCHUNKSIZE)
				nn *= 2;
			else
				nn += MEMCHUNKSIZE;
			nbp = xalloc(nn);
			bcopy(bp, nbp, p - bp);
			p = &nbp[p - bp];
			if (bp != buf)
				free(bp);
			bp = nbp;
			n = nn - (p - bp);
		}
		*p++ = i;
		if (i == '\n')
		{
			LineNumber++;
			i = getc(f);
			if (i != EOF)
				(void) ungetc(i, f);
			if (i != ' ' && i != '\t')
				break;
		}
	}
	if (p == bp)
		return (NULL);
	if (p[-1] == '\n')
		p--;
	*p = '\0';
	return (bp);
}
/*
**  CURTIME -- return current time.
**
**	Parameters:
**		none.
**
**	Returns:
**		the current time.
**
**	Side Effects:
**		none.
*/

time_t
curtime()
{
	auto time_t t;

	(void) time(&t);
	return (t);
}
/*
**  ATOBOOL -- convert a string representation to boolean.
**
**	Defaults to "TRUE"
**
**	Parameters:
**		s -- string to convert.  Takes "tTyY" as true,
**			others as false.
**
**	Returns:
**		A boolean representation of the string.
**
**	Side Effects:
**		none.
*/

bool
atobool(s)
	register char *s;
{
	if (s == NULL || *s == '\0' || strchr("tTyY", *s) != NULL)
		return (TRUE);
	return (FALSE);
}
/*
**  ATOOCT -- convert a string representation to octal.
**
**	Parameters:
**		s -- string to convert.
**
**	Returns:
**		An integer representing the string interpreted as an
**		octal number.
**
**	Side Effects:
**		none.
*/

int
atooct(s)
	register char *s;
{
	register int i = 0;

	while (*s >= '0' && *s <= '7')
		i = (i << 3) | (*s++ - '0');
	return (i);
}
/*
**  WAITFOR -- wait for a particular process id.
**
**	Parameters:
**		pid -- process id to wait for.
**
**	Returns:
**		status of pid.
**		-1 if pid never shows up.
**
**	Side Effects:
**		none.
*/

int
waitfor(pid)
	pid_t pid;
{
#ifdef WAITUNION
	union wait st;
#else
	auto int st;
#endif
	pid_t i;

	do
	{
		errno = 0;
		i = wait(&st);
		if (i > 0)
			proc_list_drop(i);
	} while ((i >= 0 || errno == EINTR) && i != pid);
	if (i < 0)
		return -1;
#ifdef WAITUNION
	return st.w_status;
#else
	return st;
#endif
}
/*
**  BITINTERSECT -- tell if two bitmaps intersect
**
**	Parameters:
**		a, b -- the bitmaps in question
**
**	Returns:
**		TRUE if they have a non-null intersection
**		FALSE otherwise
**
**	Side Effects:
**		none.
*/

bool
bitintersect(a, b)
	BITMAP a;
	BITMAP b;
{
	int i;

	for (i = BITMAPBYTES / sizeof (int); --i >= 0; )
		if ((a[i] & b[i]) != 0)
			return (TRUE);
	return (FALSE);
}
/*
**  BITZEROP -- tell if a bitmap is all zero
**
**	Parameters:
**		map -- the bit map to check
**
**	Returns:
**		TRUE if map is all zero.
**		FALSE if there are any bits set in map.
**
**	Side Effects:
**		none.
*/

bool
bitzerop(map)
	BITMAP map;
{
	int i;

	for (i = BITMAPBYTES / sizeof (int); --i >= 0; )
		if (map[i] != 0)
			return (FALSE);
	return (TRUE);
}
/*
**  STRCONTAINEDIN -- tell if one string is contained in another
**
**	Parameters:
**		a -- possible substring.
**		b -- possible superstring.
**
**	Returns:
**		TRUE if a is contained in b.
**		FALSE otherwise.
*/

bool
strcontainedin(a, b)
	register char *a;
	register char *b;
{
	int la;
	int lb;
	int c;

	la = strlen(a);
	lb = strlen(b);
	c = *a;
	if (isascii(c) && isupper(c))
		c = tolower(c);
	for (; lb-- >= la; b++)
	{
		if (*b != c && isascii(*b) && isupper(*b) && tolower(*b) != c)
			continue;
		if (strncasecmp(a, b, la) == 0)
			return TRUE;
	}
	return FALSE;
}
/*
**  CHECKFD012 -- check low numbered file descriptors
**
**	File descriptors 0, 1, and 2 should be open at all times.
**	This routine verifies that, and fixes it if not true.
**
**	Parameters:
**		where -- a tag printed if the assertion failed
**
**	Returns:
**		none
*/

void
checkfd012(where)
	char *where;
{
#if XDEBUG
	register int i;
	struct stat stbuf;

	for (i = 0; i < 3; i++)
	{
		if (fstat(i, &stbuf) < 0 && errno == EBADF)
		{
			/* oops.... */
			int fd;

			syserr("%s: fd %d not open", where, i);
			fd = open("/dev/null", i == 0 ? O_RDONLY : O_WRONLY, 0666);
			if (fd != i)
			{
				(void) dup2(fd, i);
				(void) close(fd);
			}
		}
	}
#endif /* XDEBUG */
}
/*
**  CHECKFDOPEN -- make sure file descriptor is open -- for extended debugging
**
**	Parameters:
**		fd -- file descriptor to check.
**		where -- tag to print on failure.
**
**	Returns:
**		none.
*/

void
checkfdopen(fd, where)
	int fd;
	char *where;
{
#if XDEBUG
	struct stat st;

	if (fstat(fd, &st) < 0 && errno == EBADF)
	{
		syserr("checkfdopen(%d): %s not open as expected!", fd, where);
		printopenfds(TRUE);
	}
#endif
}
/*
**  CHECKFDS -- check for new or missing file descriptors
**
**	Parameters:
**		where -- tag for printing.  If null, take a base line.
**
**	Returns:
**		none
**
**	Side Effects:
**		If where is set, shows changes since the last call.
*/

void
checkfds(where)
	char *where;
{
	int maxfd;
	register int fd;
	bool printhdr = TRUE;
	int save_errno = errno;
	static BITMAP baseline;
	extern int DtableSize;

	if (DtableSize > 256)
		maxfd = 256;
	else
		maxfd = DtableSize;
	if (where == NULL)
		clrbitmap(baseline);

	for (fd = 0; fd < maxfd; fd++)
	{
		struct stat stbuf;

		if (fstat(fd, &stbuf) < 0 && errno != EOPNOTSUPP)
		{
			if (!bitnset(fd, baseline))
				continue;
			clrbitn(fd, baseline);
		}
		else if (!bitnset(fd, baseline))
			setbitn(fd, baseline);
		else
			continue;

		/* file state has changed */
		if (where == NULL)
			continue;
		if (printhdr)
		{
			syslog(LOG_DEBUG, "%s: changed fds:", where);
			printhdr = FALSE;
		}
		dumpfd(fd, TRUE, TRUE);
	}
	errno = save_errno;
}
/*
**  PRINTOPENFDS -- print the open file descriptors (for debugging)
**
**	Parameters:
**		logit -- if set, send output to syslog; otherwise
**			print for debugging.
**
**	Returns:
**		none.
*/

#include <arpa/inet.h>

void
printopenfds(logit)
	bool logit;
{
	register int fd;
	extern int DtableSize;

	for (fd = 0; fd < DtableSize; fd++)
		dumpfd(fd, FALSE, logit);
}
/*
**  DUMPFD -- dump a file descriptor
**
**	Parameters:
**		fd -- the file descriptor to dump.
**		printclosed -- if set, print a notification even if
**			it is closed; otherwise print nothing.
**		logit -- if set, send output to syslog instead of stdout.
*/

void
dumpfd(fd, printclosed, logit)
	int fd;
	bool printclosed;
	bool logit;
{
	register char *p;
	char *hp;
	char *fmtstr;
#ifdef S_IFSOCK
	SOCKADDR sa;
#endif
	auto int slen;
	struct stat st;
	char buf[200];
	extern char *hostnamebyanyaddr();

	p = buf;
	snprintf(p, SPACELEFT(buf, p), "%3d: ", fd);
	p += strlen(p);

	if (fstat(fd, &st) < 0)
	{
		if (errno != EBADF)
		{
			snprintf(p, SPACELEFT(buf, p), "CANNOT STAT (%s)",
				errstring(errno));
			goto printit;
		}
		else if (printclosed)
		{
			snprintf(p, SPACELEFT(buf, p), "CLOSED");
			goto printit;
		}
		return;
	}

	slen = fcntl(fd, F_GETFL, NULL);
	if (slen != -1)
	{
		snprintf(p, SPACELEFT(buf, p), "fl=0x%x, ", slen);
		p += strlen(p);
	}

	snprintf(p, SPACELEFT(buf, p), "mode=%o: ", st.st_mode);
	p += strlen(p);
	switch (st.st_mode & S_IFMT)
	{
#ifdef S_IFSOCK
	  case S_IFSOCK:
		snprintf(p, SPACELEFT(buf, p), "SOCK ");
		p += strlen(p);
		slen = sizeof sa;
		if (getsockname(fd, &sa.sa, &slen) < 0)
			snprintf(p, SPACELEFT(buf, p), "(%s)", errstring(errno));
		else
		{
			hp = hostnamebyanyaddr(&sa);
			if (sa.sa.sa_family == AF_INET)
				snprintf(p, SPACELEFT(buf, p), "%s/%d",
					hp, ntohs(sa.sin.sin_port));
			else
				snprintf(p, SPACELEFT(buf, p), "%s", hp);
		}
		p += strlen(p);
		snprintf(p, SPACELEFT(buf, p), "->");
		p += strlen(p);
		slen = sizeof sa;
		if (getpeername(fd, &sa.sa, &slen) < 0)
			snprintf(p, SPACELEFT(buf, p), "(%s)", errstring(errno));
		else
		{
			hp = hostnamebyanyaddr(&sa);
			if (sa.sa.sa_family == AF_INET)
				snprintf(p, SPACELEFT(buf, p), "%s/%d",
					hp, ntohs(sa.sin.sin_port));
			else
				snprintf(p, SPACELEFT(buf, p), "%s", hp);
		}
		break;
#endif

	  case S_IFCHR:
		snprintf(p, SPACELEFT(buf, p), "CHR: ");
		p += strlen(p);
		goto defprint;

	  case S_IFBLK:
		snprintf(p, SPACELEFT(buf, p), "BLK: ");
		p += strlen(p);
		goto defprint;

#if defined(S_IFIFO) && (!defined(S_IFSOCK) || S_IFIFO != S_IFSOCK)
	  case S_IFIFO:
		snprintf(p, SPACELEFT(buf, p), "FIFO: ");
		p += strlen(p);
		goto defprint;
#endif

#ifdef S_IFDIR
	  case S_IFDIR:
		snprintf(p, SPACELEFT(buf, p), "DIR: ");
		p += strlen(p);
		goto defprint;
#endif

#ifdef S_IFLNK
	  case S_IFLNK:
		snprintf(p, SPACELEFT(buf, p), "LNK: ");
		p += strlen(p);
		goto defprint;
#endif

	  default:
defprint:
		if (sizeof st.st_size > sizeof (long))
			fmtstr = "dev=%d/%d, ino=%d, nlink=%d, u/gid=%d/%d, size=%qd";
		else
			fmtstr = "dev=%d/%d, ino=%d, nlink=%d, u/gid=%d/%d, size=%ld";
		snprintf(p, SPACELEFT(buf, p), fmtstr,
			major(st.st_dev), minor(st.st_dev), st.st_ino,
			st.st_nlink, st.st_uid, st.st_gid, st.st_size);
		break;
	}

printit:
#ifdef LOG
	if (logit)
		syslog(LOG_DEBUG, "%.800s", buf);
	else
#endif
		printf("%s\n", buf);
}
/*
**  SHORTENSTRING -- return short version of a string
**
**	If the string is already short, just return it.  If it is too
**	long, return the head and tail of the string.
**
**	Parameters:
**		s -- the string to shorten.
**		m -- the max length of the string.
**
**	Returns:
**		Either s or a short version of s.
*/

#ifndef MAXSHORTSTR
# define MAXSHORTSTR	203
#endif

char *
shortenstring(s, m)
	register const char *s;
	int m;
{
	int l;
	static char buf[MAXSHORTSTR + 1];

	l = strlen(s);
	if (l < m)
		return (char *) s;
	if (m > MAXSHORTSTR)
		m = MAXSHORTSTR;
	else if (m < 10)
	{
		if (m < 5)
		{
			strncpy(buf, s, m);
			buf[m] = '\0';
			return buf;
		}
		strncpy(buf, s, m - 3);
		strcpy(buf + m - 3, "...");
		return buf;
	}
	m = (m - 3) / 2;
	strncpy(buf, s, m);
	strcpy(buf + m, "...");
	strcpy(buf + m + 3, s + l - m);
	return buf;
}
/*
**  SHORTEN_HOSTNAME -- strip local domain information off of hostname.
**
**	Parameters:
**		host -- the host to shorten (stripped in place).
**
**	Returns:
**		none.
*/

void
shorten_hostname(host)
	char host[];
{
	register char *p;
	char *mydom;
	int i;
	bool canon = FALSE;

	/* strip off final dot */
	p = &host[strlen(host) - 1];
	if (*p == '.')
	{
		*p = '\0';
		canon = TRUE;
	}

	/* see if there is any domain at all -- if not, we are done */
	p = strchr(host, '.');
	if (p == NULL)
		return;

	/* yes, we have a domain -- see if it looks like us */
	mydom = macvalue('m', CurEnv);
	if (mydom == NULL)
		mydom = "";
	i = strlen(++p);
	if ((canon ? strcasecmp(p, mydom) : strncasecmp(p, mydom, i)) == 0 &&
	    (mydom[i] == '.' || mydom[i] == '\0'))
		*--p = '\0';
}
/*
**  PROG_OPEN -- open a program for reading
**
**	Parameters:
**		argv -- the argument list.
**		pfd -- pointer to a place to store the file descriptor.
**		e -- the current envelope.
**
**	Returns:
**		pid of the process -- -1 if it failed.
*/

int
prog_open(argv, pfd, e)
	char **argv;
	int *pfd;
	ENVELOPE *e;
{
	int pid;
	int i;
	int saveerrno;
	int fdv[2];
	char *p, *q;
	char buf[MAXLINE + 1];
	extern int DtableSize;

	if (pipe(fdv) < 0)
	{
		syserr("%s: cannot create pipe for stdout", argv[0]);
		return -1;
	}
	pid = fork();
	if (pid < 0)
	{
		syserr("%s: cannot fork", argv[0]);
		close(fdv[0]);
		close(fdv[1]);
		return -1;
	}
	if (pid > 0)
	{
		/* parent */
		close(fdv[1]);
		*pfd = fdv[0];
		return pid;
	}

	/* child -- close stdin */
	close(0);

	/* stdout goes back to parent */
	close(fdv[0]);
	if (dup2(fdv[1], 1) < 0)
	{
		syserr("%s: cannot dup2 for stdout", argv[0]);
		_exit(EX_OSERR);
	}
	close(fdv[1]);

	/* stderr goes to transcript if available */
	if (e->e_xfp != NULL)
	{
		if (dup2(fileno(e->e_xfp), 2) < 0)
		{
			syserr("%s: cannot dup2 for stderr", argv[0]);
			_exit(EX_OSERR);
		}
	}

	/* this process has no right to the queue file */
	if (e->e_lockfp != NULL)
		close(fileno(e->e_lockfp));

	/* run as default user */
	endpwent();
	if (setgid(DefGid) < 0)
		syserr("prog_open: setgid(%ld) failed", (long) DefGid);
	if (setuid(DefUid) < 0)
		syserr("prog_open: setuid(%ld) failed", (long) DefUid);

	/* run in some directory */
	if (ProgMailer != NULL)
		p = ProgMailer->m_execdir;
	else
		p = NULL;
	for (; p != NULL; p = q)
	{
		q = strchr(p, ':');
		if (q != NULL)
			*q = '\0';
		expand(p, buf, sizeof buf, e);
		if (q != NULL)
			*q++ = ':';
		if (buf[0] != '\0' && chdir(buf) >= 0)
			break;
	}
	if (p == NULL)
	{
		/* backup directories */
		if (chdir("/tmp") < 0)
			(void) chdir("/");
	}

	/* arrange for all the files to be closed */
	for (i = 3; i < DtableSize; i++)
	{
		register int j;

		if ((j = fcntl(i, F_GETFD, 0)) != -1)
			(void) fcntl(i, F_SETFD, j | 1);
	}

	/* now exec the process */
	execve(argv[0], (ARGV_T) argv, (ARGV_T) UserEnviron);

	/* woops!  failed */
	saveerrno = errno;
	syserr("%s: cannot exec", argv[0]);
	if (transienterror(saveerrno))
		_exit(EX_OSERR);
	_exit(EX_CONFIG);
	return -1;	/* avoid compiler warning on IRIX */
}
/*
**  GET_COLUMN  -- look up a Column in a line buffer
**
**	Parameters:
**		line -- the raw text line to search.
**		col -- the column number to fetch.
**		delim -- the delimiter between columns.  If null,
**			use white space.
**		buf -- the output buffer.
**		buflen -- the length of buf.
**
**	Returns:
**		buf if successful.
**		NULL otherwise.
*/

char *
get_column(line, col, delim, buf, buflen)
	char line[];
	int col;
	char delim;
	char buf[];
	int buflen;
{
	char *p;
	char *begin, *end;
	int i;
	char delimbuf[4];
	
	if (delim == '\0')
		strcpy(delimbuf, "\n\t ");
	else
	{
		delimbuf[0] = delim;
		delimbuf[1] = '\0';
	}

	p = line;
	if (*p == '\0')
		return NULL;			/* line empty */
	if (*p == delim && col == 0)
		return NULL;			/* first column empty */

	begin = line;

	if (col == 0 && delim == '\0')
	{
		while (*begin != '\0' && isascii(*begin) && isspace(*begin))
			begin++;
	}

	for (i = 0; i < col; i++)
	{
		if ((begin = strpbrk(begin, delimbuf)) == NULL)
			return NULL;		/* no such column */
		begin++;
		if (delim == '\0')
		{
			while (*begin != '\0' && isascii(*begin) && isspace(*begin))
				begin++;
		}
	}
	
	end = strpbrk(begin, delimbuf);
	if (end == NULL)
		i = strlen(begin);
	else
		i = end - begin;
	if (i >= buflen)
		i = buflen - 1;
	strncpy(buf, begin, i);
	buf[i] = '\0';
	return buf;
}
/*
**  CLEANSTRCPY -- copy string keeping out bogus characters
**
**	Parameters:
**		t -- "to" string.
**		f -- "from" string.
**		l -- length of space available in "to" string.
**
**	Returns:
**		none.
*/

void
cleanstrcpy(t, f, l)
	register char *t;
	register char *f;
	int l;
{
#ifdef LOG
	/* check for newlines and log if necessary */
	(void) denlstring(f, TRUE, TRUE);
#endif

	l--;
	while (l > 0 && *f != '\0')
	{
		if (isascii(*f) &&
		    (isalnum(*f) || strchr("!#$%&'*+-./^_`{|}~", *f) != NULL))
		{
			l--;
			*t++ = *f;
		}
		f++;
	}
	*t = '\0';
}
/*
**  DENLSTRING -- convert newlines in a string to spaces
**
**	Parameters:
**		s -- the input string
**		strict -- if set, don't permit continuation lines.
**		logattacks -- if set, log attempted attacks.
**
**	Returns:
**		A pointer to a version of the string with newlines
**		mapped to spaces.  This should be copied.
*/

char *
denlstring(s, strict, logattacks)
	char *s;
	bool strict;
	bool logattacks;
{
	register char *p;
	int l;
	static char *bp = NULL;
	static int bl = 0;

	p = s;
	while ((p = strchr(p, '\n')) != NULL)
		if (strict || (*++p != ' ' && *p != '\t'))
			break;
	if (p == NULL)
		return s;

	l = strlen(s) + 1;
	if (bl < l)
	{
		/* allocate more space */
		if (bp != NULL)
			free(bp);
		bp = xalloc(l);
		bl = l;
	}
	strcpy(bp, s);
	for (p = bp; (p = strchr(p, '\n')) != NULL; )
		*p++ = ' ';

#ifdef LOG
	if (logattacks)
	{
		syslog(LOG_NOTICE, "POSSIBLE ATTACK from %.100s: newline in string \"%s\"",
			RealHostName == NULL ? "[UNKNOWN]" : RealHostName,
			shortenstring(bp, 203));
	}
#endif

	return bp;
}
/*
**  PATH_IS_DIR -- check to see if file exists and is a directory.
**
**	Parameters:
**		pathname -- pathname to check for directory-ness.
**		createflag -- if set, create directory if needed.
**
**	Returns:
**		TRUE -- if the indicated pathname is a directory
**		FALSE -- otherwise
*/

int
path_is_dir(pathname, createflag)
	char *pathname;
	bool createflag;
{
	struct stat statbuf;

	if (stat(pathname, &statbuf) < 0)
	{
		if (errno != ENOENT || !createflag)
			return FALSE;
		if (mkdir(pathname, 0755) < 0)
			return FALSE;
		return TRUE;
	}
	if (!S_ISDIR(statbuf.st_mode))
	{
		errno = ENOTDIR;
		return FALSE;
	}
	return TRUE;
}
/*
**  PROC_LIST_ADD -- add process id to list of our children
**
**	Parameters:
**		pid -- pid to add to list.
**
**	Returns:
**		none
*/

static pid_t	*ProcListVec	= NULL;
static int	ProcListSize	= 0;

#define NO_PID		((pid_t) 0)
#ifndef PROC_LIST_SEG
# define PROC_LIST_SEG	32		/* number of pids to alloc at a time */
#endif

void
proc_list_add(pid)
	pid_t pid;
{
	int i;
	extern void proc_list_probe __P((void));

	for (i = 0; i < ProcListSize; i++)
	{
		if (ProcListVec[i] == NO_PID)
			break;
	}
	if (i >= ProcListSize)
	{
		/* probe the existing vector to avoid growing infinitely */
		proc_list_probe();

		/* now scan again */
		for (i = 0; i < ProcListSize; i++)
		{
			if (ProcListVec[i] == NO_PID)
				break;
		}
	}
	if (i >= ProcListSize)
	{
		/* grow process list */
		pid_t *npv;

		npv = (pid_t *) xalloc(sizeof (pid_t) * (ProcListSize + PROC_LIST_SEG));
		if (ProcListSize > 0)
		{
			bcopy(ProcListVec, npv, ProcListSize * sizeof (pid_t));
			free(ProcListVec);
		}
		for (i = ProcListSize; i < ProcListSize + PROC_LIST_SEG; i++)
			npv[i] = NO_PID;
		i = ProcListSize;
		ProcListSize += PROC_LIST_SEG;
		ProcListVec = npv;
	}
	ProcListVec[i] = pid;
	CurChildren++;
}
/*
**  PROC_LIST_DROP -- drop pid from process list
**
**	Parameters:
**		pid -- pid to drop
**
**	Returns:
**		none.
*/

void
proc_list_drop(pid)
	pid_t pid;
{
	int i;

	for (i = 0; i < ProcListSize; i++)
	{
		if (ProcListVec[i] == pid)
		{
			ProcListVec[i] = NO_PID;
			break;
		}
	}
	if (CurChildren > 0)
		CurChildren--;
}
/*
**  PROC_LIST_CLEAR -- clear the process list
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
proc_list_clear()
{
	int i;

	for (i = 0; i < ProcListSize; i++)
		ProcListVec[i] = NO_PID;
	CurChildren = 0;
}
/*
**  PROC_LIST_PROBE -- probe processes in the list to see if they still exist
**
**	Parameters:
**		none
**
**	Returns:
**		none
*/

void
proc_list_probe()
{
	int i;

	for (i = 0; i < ProcListSize; i++)
	{
		if (ProcListVec[i] == NO_PID)
			continue;
		if (kill(ProcListVec[i], 0) < 0)
		{
#ifdef LOG
			if (LogLevel > 3)
				syslog(LOG_DEBUG, "proc_list_probe: lost pid %d",
					ProcListVec[i]);
#endif
			ProcListVec[i] = NO_PID;
			CurChildren--;
		}
	}
	if (CurChildren < 0)
		CurChildren = 0;
}
