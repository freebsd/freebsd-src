/*
 * Copyright (c) 1983 Eric P. Allman
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
static char sccsid[] = "@(#)util.c	8.3 (Berkeley) 7/13/93";
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

	p = malloc((unsigned) sz);
	if (p == NULL)
	{
		syserr("Out of memory!!");
		abort();
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

printav(av)
	register char **av;
{
	while (*av != NULL)
	{
		if (tTd(0, 44))
			printf("\n\t%08x=", *av);
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

xputs(s)
	register char *s;
{
	register int c;
	register struct metamac *mp;
	extern struct metamac MetaMacros[];

	if (s == NULL)
	{
		printf("<null>");
		return;
	}
	while ((c = (*s++ & 0377)) != '\0')
	{
		if (!isascii(c))
		{
			if (c == MATCHREPL || c == MACROEXPAND)
			{
				putchar('$');
				continue;
			}
			for (mp = MetaMacros; mp->metaname != '\0'; mp++)
			{
				if ((mp->metaval & 0377) == c)
				{
					printf("$%c", mp->metaname);
					break;
				}
			}
			if (mp->metaname != '\0')
				continue;
			(void) putchar('\\');
			c &= 0177;
		}
		if (isprint(c))
		{
			putchar(c);
			continue;
		}

		/* wasn't a meta-macro -- find another way to print it */
		switch (c)
		{
		  case '\0':
			continue;

		  case '\n':
			c = 'n';
			break;

		  case '\r':
			c = 'r';
			break;

		  case '\t':
			c = 't';
			break;

		  default:
			(void) putchar('^');
			(void) putchar(c ^ 0100);
			continue;
		}
	}
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
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

buildfname(gecos, login, buf)
	register char *gecos;
	char *login;
	char *buf;
{
	register char *p;
	register char *bp = buf;
	int l;

	if (*gecos == '*')
		gecos++;

	/* find length of final string */
	l = 0;
	for (p = gecos; *p != '\0' && *p != ',' && *p != ';' && *p != '%'; p++)
	{
		if (*p == '&')
			l += strlen(login);
		else
			l++;
	}

	/* now fill in buf */
	for (p = gecos; *p != '\0' && *p != ',' && *p != ';' && *p != '%'; p++)
	{
		if (*p == '&')
		{
			(void) strcpy(bp, login);
			*bp = toupper(*bp);
			while (*bp != '\0')
				bp++;
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
**		uid -- uid to compare against.
**		mustown -- to be safe, this uid must own the file.
**		mode -- mode bits that must match.
**
**	Returns:
**		0 if fn exists, is owned by uid, and matches mode.
**		An errno otherwise.  The actual errno is cleared.
**
**	Side Effects:
**		none.
*/

#ifndef S_IXOTH
# define S_IXOTH	(S_IEXEC >> 6)
#endif

#ifndef S_IXUSR
# define S_IXUSR	(S_IEXEC)
#endif

int
safefile(fn, uid, mustown, mode)
	char *fn;
	uid_t uid;
	bool mustown;
	int mode;
{
	register char *p;
	struct stat stbuf;

	if (tTd(54, 4))
		printf("safefile(%s, %d, %d, %o): ", fn, uid, mustown, mode);
	errno = 0;

	for (p = fn; (p = strchr(++p, '/')) != NULL; *p = '/')
	{
		*p = '\0';
		if (stat(fn, &stbuf) < 0 ||
		    !bitset(stbuf.st_uid == uid ? S_IXUSR : S_IXOTH,
			    stbuf.st_mode))
		{
			int ret = errno;

			if (ret == 0)
				ret = EACCES;
			if (tTd(54, 4))
				printf("[dir %s] %s\n", fn, errstring(ret));
			*p = '/';
			return ret;
		}
	}

	if (stat(fn, &stbuf) < 0)
	{
		int ret = errno;

		if (tTd(54, 4))
			printf("%s\n", errstring(ret));

		errno = 0;
		return ret;
	}
	if (stbuf.st_uid != uid || uid == 0 || !mustown)
		mode >>= 6;
	if (tTd(54, 4))
		printf("[uid %d, stat %o] ", stbuf.st_uid, stbuf.st_mode);
	if ((stbuf.st_uid == uid || uid == 0 || !mustown) &&
	    (stbuf.st_mode & mode) == mode)
	{
		if (tTd(54, 4))
			printf("OK\n");
		return 0;
	}
	if (tTd(54, 4))
		printf("EACCES\n");
	return EACCES;
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

#ifndef O_ACCMODE
# define O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif

struct omodes
{
	int	mask;
	int	mode;
	char	*farg;
} OpenModes[] =
{
	O_ACCMODE,		O_RDONLY,		"r",
	O_ACCMODE|O_APPEND,	O_WRONLY,		"w",
	O_ACCMODE|O_APPEND,	O_WRONLY|O_APPEND,	"a",
	O_TRUNC,		0,			"w+",
	O_APPEND,		O_APPEND,		"a+",
	0,			0,			"r+",
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
		if (errno != ENFILE && errno != EINTR)
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
		(void) lockfile(fd, filename, locktype);
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
**		fp -- file to put it onto.
**		m -- the mailer used to control output.
**
**	Returns:
**		none
**
**	Side Effects:
**		output of l to fp.
*/

putline(l, fp, m)
	register char *l;
	FILE *fp;
	MAILER *m;
{
	register char *p;
	register char svchar;

	/* strip out 0200 bits -- these can look like TELNET protocol */
	if (bitnset(M_7BITS, m->m_flags))
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
			fprintf(TrafficLogFile, "%05d >>> ", getpid());

		/* check for line overflow */
		while (m->m_linelimit > 0 && (p - l) > m->m_linelimit)
		{
			register char *q = &l[m->m_linelimit - 1];

			svchar = *q;
			*q = '\0';
			if (l[0] == '.' && bitnset(M_XDOT, m->m_flags))
			{
				(void) putc('.', fp);
				if (TrafficLogFile != NULL)
					(void) putc('.', TrafficLogFile);
			}
			fputs(l, fp);
			(void) putc('!', fp);
			fputs(m->m_eol, fp);
			if (TrafficLogFile != NULL)
				fprintf(TrafficLogFile, "%s!\n%05d >>> ",
					l, getpid());
			*q = svchar;
			l = q;
		}

		/* output last part */
		if (l[0] == '.' && bitnset(M_XDOT, m->m_flags))
		{
			(void) putc('.', fp);
			if (TrafficLogFile != NULL)
				(void) putc('.', TrafficLogFile);
		}
		if (TrafficLogFile != NULL)
			fprintf(TrafficLogFile, "%.*s\n", p - l, l);
		for ( ; l < p; ++l)
			(void) putc(*l, fp);
		fputs(m->m_eol, fp);
		if (*l == '\n')
			++l;
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

xfclose(fp, a, b)
	FILE *fp;
	char *a, *b;
{
	if (tTd(53, 99))
		printf("xfclose(%x) %s %s\n", fp, a, b);
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
	static int readtimeout();

	/* set the timeout */
	if (timeout != 0)
	{
		if (setjmp(CtxReadTimeout) != 0)
		{
# ifdef LOG
			syslog(LOG_NOTICE,
			    "timeout waiting for input from %s during %s\n",
			    CurHostName? CurHostName: "local", during);
# endif
			errno = 0;
			usrerr("451 timeout waiting for input during %s",
				during);
			buf[0] = '\0';
#ifdef XDEBUG
			checkfd012(during);
#endif
			return (NULL);
		}
		ev = setevent(timeout, readtimeout, 0);
	}

	/* try to read */
	p = NULL;
	while (p == NULL && !feof(fp) && !ferror(fp))
	{
		errno = 0;
		p = fgets(buf, siz, fp);
		if (errno == EINTR)
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
			fprintf(TrafficLogFile, "%05d <<< [EOF]\n", getpid());
		return (NULL);
	}
	if (TrafficLogFile != NULL)
		fprintf(TrafficLogFile, "%05d <<< %s", getpid(), buf);
	if (SevenBit)
		for (p = buf; *p != '\0'; p++)
			*p &= ~0200;
	return (buf);
}

static
readtimeout()
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
	*--p = '\0';
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
	if (*s == '\0' || strchr("tTyY", *s) != NULL)
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

waitfor(pid)
	int pid;
{
	auto int st;
	int i;

	do
	{
		errno = 0;
		i = wait(&st);
	} while ((i >= 0 || errno == EINTR) && i != pid);
	if (i < 0)
		st = -1;
	return (st);
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
	int l;

	l = strlen(a);
	for (;;)
	{
		b = strchr(b, a[0]);
		if (b == NULL)
			return FALSE;
		if (strncmp(a, b, l) == 0)
			return TRUE;
		b++;
	}
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

checkfd012(where)
	char *where;
{
#ifdef XDEBUG
	register int i;
	struct stat stbuf;

	for (i = 0; i < 3; i++)
	{
		if (fstat(i, &stbuf) < 0)
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
#endif XDEBUG
}
