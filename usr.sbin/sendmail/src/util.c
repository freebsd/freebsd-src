/*
 * Copyright (c) 1983, 1995-1997 Eric P. Allman
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
static char sccsid[] = "@(#)util.c	8.133 (Berkeley) 8/1/97";
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
				else if (*s != '\0')
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
	putxline(l, strlen(l), mci, PXLF_MAPFROM);
}
/*
**  PUTXLINE -- putline with flags bits.
**
**	This routine always guarantees outputing a newline (or CRLF,
**	as appropriate) at the end of the string.
**
**	Parameters:
**		l -- line to put.
**		len -- the length of the line.
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
putxline(l, len, mci, pxflags)
	register char *l;
	size_t len;
	register MCI *mci;
	int pxflags;
{
	register char *p, *end;
	int slop = 0;

	/* strip out 0200 bits -- these can look like TELNET protocol */
	if (bitset(MCIF_7BIT, mci->mci_flags) ||
	    bitset(PXLF_STRIP8BIT, pxflags))
	{
		register char svchar;

		for (p = l; (svchar = *p) != '\0'; ++p)
			if (bitset(0200, svchar))
				*p = svchar &~ 0200;
	}

	end = l + len;
	do
	{
		/* find the end of the line */
		p = memchr(l, '\n', end - l);
		if (p == NULL)
			p = end;

		if (TrafficLogFile != NULL)
			fprintf(TrafficLogFile, "%05d >>> ", (int) getpid());

		/* check for line overflow */
		while (mci->mci_mailer->m_linelimit > 0 &&
		       (p - l + slop) > mci->mci_mailer->m_linelimit)
		{
			char *l_base = l;
			register char *q = &l[mci->mci_mailer->m_linelimit - slop - 1];

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
			while (l < q)
				(void) putc(*l++, mci->mci_out);
			(void) putc('!', mci->mci_out);
			fputs(mci->mci_mailer->m_eol, mci->mci_out);
			(void) putc(' ', mci->mci_out);
			if (TrafficLogFile != NULL)
			{
				for (l = l_base; l < q; l++)
					(void) putc(*l, TrafficLogFile);
				fprintf(TrafficLogFile, "!\n%05d >>>  ",
					(int) getpid());
			}
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
		for ( ; l < p; ++l)
		{
			if (TrafficLogFile != NULL)
				(void) putc(*l, TrafficLogFile);
			(void) putc(*l, mci->mci_out);
		}
		if (TrafficLogFile != NULL)
			(void) putc('\n', TrafficLogFile);
		fputs(mci->mci_mailer->m_eol, mci->mci_out);
		if (l < end && *l == '\n')
		{
			if (*++l != ' ' && *l != '\t' && *l != '\0')
			{
				(void) putc(' ', mci->mci_out);
				if (TrafficLogFile != NULL)
					(void) putc(' ', TrafficLogFile);
			}
		}
	} while (l < end);
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

	if (LogLevel > 98)
		sm_syslog(LOG_DEBUG, CurEnv->e_id,
			"unlink %s",
			f);

	i = unlink(f);
	if (i < 0 && LogLevel > 97)
		sm_syslog(LOG_DEBUG, CurEnv->e_id,
			"%s: unlink-fail %d",
			f, errno);
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
			if (LogLevel > 1)
				sm_syslog(LOG_NOTICE, CurEnv->e_id,
				       "timeout waiting for input from %.100s during %s",
				       CurHostName ? CurHostName : "local",
				       during);
			errno = 0;
			buf[0] = '\0';
#if XDEBUG
			checkfd012(during);
#endif
			if (TrafficLogFile != NULL)
				fprintf(TrafficLogFile, "%05d <<< [TIMEOUT]\n",
					(int) getpid());
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
		fill_fd(i, where);
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
			sm_syslog(LOG_DEBUG, CurEnv->e_id,
				"%s: changed fds:",
				where);
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
	auto SOCKADDR_LEN_T slen;
	int i;
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

	i = fcntl(fd, F_GETFL, NULL);
	if (i != -1)
	{
		snprintf(p, SPACELEFT(buf, p), "fl=0x%x, ", i);
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
	if (logit)
		sm_syslog(LOG_DEBUG, CurEnv->e_id,
			"%.800s", buf);
	else
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
	/* check for newlines and log if necessary */
	(void) denlstring(f, TRUE, TRUE);

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

	if (logattacks)
	{
		sm_syslog(LOG_NOTICE, CurEnv->e_id,
			"POSSIBLE ATTACK from %.100s: newline in string \"%s\"",
			RealHostName == NULL ? "[UNKNOWN]" : RealHostName,
			shortenstring(bp, 203));
	}

	return bp;
}
/*
**  PATH_IS_DIR -- check to see if file exists and is a directory.
**
**	There are some additional checks for security violations in
**	here.  This routine is intended to be used for the host status
**	support.
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

#if HASLSTAT
	if (lstat(pathname, &statbuf) < 0)
#else
	if (stat(pathname, &statbuf) < 0)
#endif
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

	/* security: don't allow writable directories */
	if (bitset(S_IWGRP|S_IWOTH, statbuf.st_mode))
	{
		errno = EACCES;
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
			if (LogLevel > 3)
				sm_syslog(LOG_DEBUG, CurEnv->e_id,
					"proc_list_probe: lost pid %d",
					ProcListVec[i]);
			ProcListVec[i] = NO_PID;
			CurChildren--;
		}
	}
	if (CurChildren < 0)
		CurChildren = 0;
}
/*
**  SM_STRCASECMP -- 8-bit clean version of strcasecmp
**
**	Thank you, vendors, for making this all necessary.
*/

/*
 * Copyright (c) 1987, 1993
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strcasecmp.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

/*
 * This array is designed for mapping upper and lower case letter
 * together for a case independent comparison.  The mappings are
 * based upon ascii character sequences.
 */
static const u_char charmap[] = {
	0000, 0001, 0002, 0003, 0004, 0005, 0006, 0007,
	0010, 0011, 0012, 0013, 0014, 0015, 0016, 0017,
	0020, 0021, 0022, 0023, 0024, 0025, 0026, 0027,
	0030, 0031, 0032, 0033, 0034, 0035, 0036, 0037,
	0040, 0041, 0042, 0043, 0044, 0045, 0046, 0047,
	0050, 0051, 0052, 0053, 0054, 0055, 0056, 0057,
	0060, 0061, 0062, 0063, 0064, 0065, 0066, 0067,
	0070, 0071, 0072, 0073, 0074, 0075, 0076, 0077,
	0100, 0141, 0142, 0143, 0144, 0145, 0146, 0147,
	0150, 0151, 0152, 0153, 0154, 0155, 0156, 0157,
	0160, 0161, 0162, 0163, 0164, 0165, 0166, 0167,
	0170, 0171, 0172, 0133, 0134, 0135, 0136, 0137,
	0140, 0141, 0142, 0143, 0144, 0145, 0146, 0147,
	0150, 0151, 0152, 0153, 0154, 0155, 0156, 0157,
	0160, 0161, 0162, 0163, 0164, 0165, 0166, 0167,
	0170, 0171, 0172, 0173, 0174, 0175, 0176, 0177,
	0200, 0201, 0202, 0203, 0204, 0205, 0206, 0207,
	0210, 0211, 0212, 0213, 0214, 0215, 0216, 0217,
	0220, 0221, 0222, 0223, 0224, 0225, 0226, 0227,
	0230, 0231, 0232, 0233, 0234, 0235, 0236, 0237,
	0240, 0241, 0242, 0243, 0244, 0245, 0246, 0247,
	0250, 0251, 0252, 0253, 0254, 0255, 0256, 0257,
	0260, 0261, 0262, 0263, 0264, 0265, 0266, 0267,
	0270, 0271, 0272, 0273, 0274, 0275, 0276, 0277,
	0300, 0301, 0302, 0303, 0304, 0305, 0306, 0307,
	0310, 0311, 0312, 0313, 0314, 0315, 0316, 0317,
	0320, 0321, 0322, 0323, 0324, 0325, 0326, 0327,
	0330, 0331, 0332, 0333, 0334, 0335, 0336, 0337,
	0340, 0341, 0342, 0343, 0344, 0345, 0346, 0347,
	0350, 0351, 0352, 0353, 0354, 0355, 0356, 0357,
	0360, 0361, 0362, 0363, 0364, 0365, 0366, 0367,
	0370, 0371, 0372, 0373, 0374, 0375, 0376, 0377,
};

int
sm_strcasecmp(s1, s2)
	const char *s1, *s2;
{
	register const u_char *cm = charmap,
			*us1 = (const u_char *)s1,
			*us2 = (const u_char *)s2;

	while (cm[*us1] == cm[*us2++])
		if (*us1++ == '\0')
			return (0);
	return (cm[*us1] - cm[*--us2]);
}

int
sm_strncasecmp(s1, s2, n)
	const char *s1, *s2;
	register size_t n;
{
	if (n != 0) {
		register const u_char *cm = charmap,
				*us1 = (const u_char *)s1,
				*us2 = (const u_char *)s2;

		do {
			if (cm[*us1] != cm[*us2++])
				return (cm[*us1] - cm[*--us2]);
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}
