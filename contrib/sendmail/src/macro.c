/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
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
static char id[] = "@(#)$Id: macro.c,v 8.40.16.2 2000/09/17 17:04:26 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>

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
	int skiplev;		/* skipping nesting level */
	int iflev;		/* if nesting level */
	char xbuf[MACBUFSIZE];
	static int explevel = 0;

	if (tTd(35, 24))
	{
		dprintf("expand(");
		xputs(s);
		dprintf(")\n");
	}

	skipping = FALSE;
	skiplev = 0;
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
			iflev++;
			c = *++s;
			if (skipping)
				skiplev++;
			else
			{
				char *mv;

				mv = macvalue(c, e);
				skipping = (mv == NULL || *mv == '\0');
			}
			continue;

		  case CONDELSE:	/* change state of skipping */
			if (iflev == 0)
				break;
			if (skiplev == 0)
				skipping = !skipping;
			continue;

		  case CONDFI:		/* stop skipping */
			if (iflev == 0)
				break;
			iflev--;
			if (skiplev == 0)
				skipping = FALSE;
			if (skipping)
				skiplev--;
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

		if (skipping || xp >= &xbuf[sizeof xbuf - 1])
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
		dprintf("expand ==> ");
		xputs(xbuf);
		dprintf("\n");
	}

	/* recurse as appropriate */
	if (recurse)
	{
		if (explevel < MaxMacroRecursion)
		{
			explevel++;
			expand(xbuf, buf, bufsize, e);
			explevel--;
			return;
		}
		syserr("expand: recursion too deep (%d max)",
			MaxMacroRecursion);
	}

	/* copy results out */
	i = xp - xbuf;
	if ((size_t)i >= bufsize)
		i = bufsize - 1;
	memmove(buf, xbuf, i);
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
**		     (set via OperatorChars option in V6 or later
**		      sendmail.cf files)
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
	int m;

	m = n & 0377;
	if (tTd(35, 9))
	{
		dprintf("%sdefine(%s as ",
			(e->e_macro[m] == NULL) ? ""
						: "re", macname(n));
		xputs(v);
		dprintf(")\n");
	}
	e->e_macro[m] = v;

#if _FFR_RESET_MACRO_GLOBALS
	switch (m)
	{
	  case 'j':
		MyHostName = v;
		break;
	}
#endif /* _FFR_RESET_MACRO_GLOBALS */
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
			return p;
		e = e->e_parent;
	}
	return NULL;
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
	char mbuf[MAXMACNAMELEN + 1];

	if (tTd(35, 14))
	{
		dprintf("macid(");
		xputs(p);
		dprintf(") => ");
	}

	if (*p == '\0' || (p[0] == '{' && p[1] == '}'))
	{
		syserr("Name required for macro/class");
		if (ep != NULL)
			*ep = p;
		if (tTd(35, 14))
			dprintf("NULL\n");
		return '\0';
	}
	if (*p != '{')
	{
		/* the macro is its own code */
		if (ep != NULL)
			*ep = p + 1;
		if (tTd(35, 14))
			dprintf("%c\n", *p);
		return *p;
	}
	bp = mbuf;
	while (*++p != '\0' && *p != '}' && bp < &mbuf[sizeof mbuf - 1])
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
			if (NextMacroId > MAXMACROID)
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
		dprintf("0x%x\n", mid);
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
