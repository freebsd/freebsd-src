/*
 * Copyright (c) 1983, 1995 Eric P. Allman
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
static char sccsid[] = "@(#)macro.c	8.13 (Berkeley) 7/10/95";
#endif /* not lint */

# include "sendmail.h"

char	*MacroName[256];	/* macro id to name table */
int	NextMacroId = 0240;	/* codes for long named macros */


/*
**  EXPAND -- macro expand a string using $x escapes.
**
**	Parameters:
**		s -- the string to expand.
**		buf -- the place to put the expansion.
**		bufsize -- the size of the buffer.
**		e -- envelope in which to work.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

void
expand(s, buf, bufsize, e)
	register char *s;
	register char *buf;
	size_t bufsize;
	register ENVELOPE *e;
{
	register char *xp;
	register char *q;
	bool skipping;		/* set if conditionally skipping output */
	bool recurse = FALSE;	/* set if recursion required */
	int i;
	int iflev;		/* if nesting level */
	char xbuf[BUFSIZ];

	if (tTd(35, 24))
	{
		printf("expand(");
		xputs(s);
		printf(")\n");
	}

	skipping = FALSE;
	iflev = 0;
	if (s == NULL)
		s = "";
	for (xp = xbuf; *s != '\0'; s++)
	{
		int c;

		/*
		**  Check for non-ordinary (special?) character.
		**	'q' will be the interpolated quantity.
		*/

		q = NULL;
		c = *s;
		switch (c & 0377)
		{
		  case CONDIF:		/* see if var set */
			c = *++s;
			if (skipping)
				iflev++;
			else
				skipping = macvalue(c, e) == NULL;
			continue;

		  case CONDELSE:	/* change state of skipping */
			if (iflev == 0)
				skipping = !skipping;
			continue;

		  case CONDFI:		/* stop skipping */
			if (iflev == 0)
				skipping = FALSE;
			if (skipping)
				iflev--;
			continue;

		  case MACROEXPAND:	/* macro interpolation */
			c = *++s & 0377;
			if (c != '\0')
				q = macvalue(c, e);
			else
			{
				s--;
				q = NULL;
			}
			if (q == NULL)
				continue;
			break;
		}

		/*
		**  Interpolate q or output one character
		*/

		if (skipping || xp >= &xbuf[sizeof xbuf])
			continue;
		if (q == NULL)
			*xp++ = c;
		else
		{
			/* copy to end of q or max space remaining in buf */
			while ((c = *q++) != '\0' && xp < &xbuf[sizeof xbuf - 1])
			{
				/* check for any sendmail metacharacters */
				if ((c & 0340) == 0200)
					recurse = TRUE;
				*xp++ = c;
			}
		}
	}
	*xp = '\0';

	if (tTd(35, 24))
	{
		printf("expand ==> ");
		xputs(xbuf);
		printf("\n");
	}

	/* recurse as appropriate */
	if (recurse)
	{
		expand(xbuf, buf, bufsize, e);
		return;
	}

	/* copy results out */
	i = xp - xbuf;
	if (i >= bufsize)
		i = bufsize - 1;
	bcopy(xbuf, buf, i);
	buf[i] = '\0';
}
/*
**  DEFINE -- define a macro.
**
**	this would be better done using a #define macro.
**
**	Parameters:
**		n -- the macro name.
**		v -- the macro value.
**		e -- the envelope to store the definition in.
**
**	Returns:
**		none.
**
**	Side Effects:
**		e->e_macro[n] is defined.
**
**	Notes:
**		There is one macro for each ASCII character,
**		although they are not all used.  The currently
**		defined macros are:
**
**		$a   date in ARPANET format (preferring the Date: line
**		     of the message)
**		$b   the current date (as opposed to the date as found
**		     the message) in ARPANET format
**		$c   hop count
**		$d   (current) date in UNIX (ctime) format
**		$e   the SMTP entry message+
**		$f   raw from address
**		$g   translated from address
**		$h   to host
**		$i   queue id
**		$j   official SMTP hostname, used in messages+
**		$k   UUCP node name
**		$l   UNIX-style from line+
**		$m   The domain part of our full name.
**		$n   name of sendmail ("MAILER-DAEMON" on local
**		     net typically)+
**		$o   delimiters ("operators") for address tokens+
**		$p   my process id in decimal
**		$q   the string that becomes an address -- this is
**		     normally used to combine $g & $x.
**		$r   protocol used to talk to sender
**		$s   sender's host name
**		$t   the current time in seconds since 1/1/1970
**		$u   to user
**		$v   version number of sendmail
**		$w   our host name (if it can be determined)
**		$x   signature (full name) of from person
**		$y   the tty id of our terminal
**		$z   home directory of to person
**		$_   RFC1413 authenticated sender address
**
**		Macros marked with + must be defined in the
**		configuration file and are used internally, but
**		are not set.
**
**		There are also some macros that can be used
**		arbitrarily to make the configuration file
**		cleaner.  In general all upper-case letters
**		are available.
*/

void
define(n, v, e)
	int n;
	char *v;
	register ENVELOPE *e;
{
	if (tTd(35, 9))
	{
		printf("%sdefine(%s as ", 
		    (e->e_macro[n & 0377] == NULL) ? "" : "re", macname(n));
		xputs(v);
		printf(")\n");
	}
	e->e_macro[n & 0377] = v;
}
/*
**  MACVALUE -- return uninterpreted value of a macro.
**
**	Parameters:
**		n -- the name of the macro.
**
**	Returns:
**		The value of n.
**
**	Side Effects:
**		none.
*/

char *
macvalue(n, e)
	int n;
	register ENVELOPE *e;
{
	n &= 0377;
	while (e != NULL)
	{
		register char *p = e->e_macro[n];

		if (p != NULL)
			return (p);
		e = e->e_parent;
	}
	return (NULL);
}
/*
**  MACNAME -- return the name of a macro given its internal id
**
**	Parameter:
**		n -- the id of the macro
**
**	Returns:
**		The name of n.
**
**	Side Effects:
**		none.
*/

char *
macname(n)
	int n;
{
	static char mbuf[2];

	n &= 0377;
	if (bitset(0200, n))
	{
		char *p = MacroName[n];

		if (p != NULL)
			return p;
		return "***UNDEFINED MACRO***";
	}
	mbuf[0] = n;
	mbuf[1] = '\0';
	return mbuf;
}
/*
**  MACID -- return id of macro identified by its name
**
**	Parameters:
**		p -- pointer to name string -- either a single
**			character or {name}.
**		ep -- filled in with the pointer to the byte
**			after the name.
**
**	Returns:
**		The internal id code for this macro.  This will
**		fit into a single byte.
**
**	Side Effects:
**		If this is a new macro name, a new id is allocated.
*/

int
macid(p, ep)
	register char *p;
	char **ep;
{
	int mid;
	register char *bp;
	char mbuf[21];

	if (tTd(35, 14))
	{
		printf("macid(");
		xputs(p);
		printf(") => ");
	}

	if (*p == '\0' || (p[0] == '{' && p[1] == '}'))
	{
		syserr("Name required for macro/class");
		if (ep != NULL)
			*ep = p;
		if (tTd(35, 14))
			printf("NULL\n");
		return '\0';
	}
	if (*p != '{')
	{
		/* the macro is its own code */
		if (ep != NULL)
			*ep = p + 1;
		if (tTd(35, 14))
			printf("%c\n", *p);
		return *p;
	}
	bp = mbuf;
	while (*++p != '\0' && *p != '}' && bp < &mbuf[sizeof mbuf])
	{
		if (isascii(*p) && (isalnum(*p) || *p == '_'))
			*bp++ = *p;
		else
			syserr("Invalid macro/class character %c", *p);
	}
	*bp = '\0';
	mid = -1;
	if (*p == '\0')
	{
		syserr("Unbalanced { on %s", mbuf);	/* missing } */
	}
	else if (*p != '}')
	{
		syserr("Macro/class name ({%s}) too long (%d chars max)",
			mbuf, sizeof mbuf - 1);
	}
	else if (mbuf[1] == '\0')
	{
		/* ${x} == $x */
		mid = mbuf[0];
		p++;
	}
	else
	{
		register STAB *s;

		s = stab(mbuf, ST_MACRO, ST_ENTER);
		if (s->s_macro != 0)
			mid = s->s_macro;
		else
		{
			if (NextMacroId > 0377)
			{
				syserr("Macro/class {%s}: too many long names", mbuf);
				s->s_macro = -1;
			}
			else
			{
				MacroName[NextMacroId] = s->s_name;
				s->s_macro = mid = NextMacroId++;
			}
		}
		p++;
	}
	if (ep != NULL)
		*ep = p;
	if (tTd(35, 14))
		printf("0x%x\n", mid);
	return mid;
}
/*
**  WORDINCLASS -- tell if a word is in a specific class
**
**	Parameters:
**		str -- the name of the word to look up.
**		cl -- the class name.
**
**	Returns:
**		TRUE if str can be found in cl.
**		FALSE otherwise.
*/

bool
wordinclass(str, cl)
	char *str;
	int cl;
{
	register STAB *s;

	s = stab(str, ST_CLASS, ST_FIND);
	return s != NULL && bitnset(cl & 0xff, s->s_class);
}
