/*
 * Copyright (c) 1998-2001, 2003 Sendmail, Inc. and its suppliers.
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

#include <sendmail.h>

SM_RCSID("@(#)$Id: macro.c,v 8.88 2003/09/05 23:11:18 ca Exp $")

#if MAXMACROID != (BITMAPBITS - 1)
	ERROR Read the comment in conf.h
#endif /* MAXMACROID != (BITMAPBITS - 1) */

static char	*MacroName[MAXMACROID + 1];	/* macro id to name table */
int		NextMacroId = 0240;	/* codes for long named macros */

/*
**  INITMACROS -- initialize the macro system
**
**	This just involves defining some macros that are actually
**	used internally as metasymbols to be themselves.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		initializes several macros to be themselves.
*/

struct metamac	MetaMacros[] =
{
	/* LHS pattern matching characters */
	{ '*', MATCHZANY },	{ '+', MATCHANY },	{ '-', MATCHONE },
	{ '=', MATCHCLASS },	{ '~', MATCHNCLASS },

	/* these are RHS metasymbols */
	{ '#', CANONNET },	{ '@', CANONHOST },	{ ':', CANONUSER },
	{ '>', CALLSUBR },

	/* the conditional operations */
	{ '?', CONDIF },	{ '|', CONDELSE },	{ '.', CONDFI },

	/* the hostname lookup characters */
	{ '[', HOSTBEGIN },	{ ']', HOSTEND },
	{ '(', LOOKUPBEGIN },	{ ')', LOOKUPEND },

	/* miscellaneous control characters */
	{ '&', MACRODEXPAND },

	{ '\0', '\0' }
};

#define MACBINDING(name, mid) \
		stab(name, ST_MACRO, ST_ENTER)->s_macro = mid; \
		MacroName[mid] = name;

void
initmacros(e)
	register ENVELOPE *e;
{
	register struct metamac *m;
	register int c;
	char buf[5];

	for (m = MetaMacros; m->metaname != '\0'; m++)
	{
		buf[0] = m->metaval;
		buf[1] = '\0';
		macdefine(&e->e_macro, A_TEMP, m->metaname, buf);
	}
	buf[0] = MATCHREPL;
	buf[2] = '\0';
	for (c = '0'; c <= '9'; c++)
	{
		buf[1] = c;
		macdefine(&e->e_macro, A_TEMP, c, buf);
	}

	/* set defaults for some macros sendmail will use later */
	macdefine(&e->e_macro, A_PERM, 'n', "MAILER-DAEMON");

	/* set up external names for some internal macros */
	MACBINDING("opMode", MID_OPMODE);
	/*XXX should probably add equivalents for all short macros here XXX*/
}
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
	bool recurse;		/* set if recursion required */
	size_t i;
	int skiplev;		/* skipping nesting level */
	int iflev;		/* if nesting level */
	char xbuf[MACBUFSIZE];
	static int explevel = 0;

	if (tTd(35, 24))
	{
		sm_dprintf("expand(");
		xputs(sm_debug_file(), s);
		sm_dprintf(")\n");
	}

	recurse = false;
	skipping = false;
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
				break;	/* XXX: error */
			if (skiplev == 0)
				skipping = !skipping;
			continue;

		  case CONDFI:		/* stop skipping */
			if (iflev == 0)
				break;	/* XXX: error */
			iflev--;
			if (skiplev == 0)
				skipping = false;
			if (skipping)
				skiplev--;
			continue;

		  case MACROEXPAND:	/* macro interpolation */
			c = bitidx(*++s);
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
					recurse = true;
				*xp++ = c;
			}
		}
	}
	*xp = '\0';

	if (tTd(35, 24))
	{
		sm_dprintf("expand ==> ");
		xputs(sm_debug_file(), xbuf);
		sm_dprintf("\n");
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
	if (i >= bufsize)
		i = bufsize - 1;
	memmove(buf, xbuf, i);
	buf[i] = '\0';
}

/*
**  MACDEFINE -- bind a macro name to a value
**
**	Set a macro to a value, with fancy storage management.
**	macdefine will make a copy of the value, if required,
**	and will ensure that the storage for the previous value
**	is not leaked.
**
**	Parameters:
**		mac -- Macro table.
**		vclass -- storage class of 'value', ignored if value==NULL.
**			A_HEAP	means that the value was allocated by
**				malloc, and that macdefine owns the storage.
**			A_TEMP	means that value points to temporary storage,
**				and thus macdefine needs to make a copy.
**			A_PERM	means that value points to storage that
**				will remain allocated and unchanged for
**				at least the lifetime of mac.  Use A_PERM if:
**				-- value == NULL,
**				-- value points to a string literal,
**				-- value was allocated from mac->mac_rpool
**				   or (in the case of an envelope macro)
**				   from e->e_rpool,
**				-- in the case of an envelope macro,
**				   value is a string member of the envelope
**				   such as e->e_sender.
**		id -- Macro id.  This is a single character macro name
**			such as 'g', or a value returned by macid().
**		value -- Macro value: either NULL, or a string.
*/

void
#if SM_HEAP_CHECK
macdefine_tagged(mac, vclass, id, value, file, line, grp)
#else /* SM_HEAP_CHECK */
macdefine(mac, vclass, id, value)
#endif /* SM_HEAP_CHECK */
	MACROS_T *mac;
	ARGCLASS_T vclass;
	int id;
	char *value;
#if SM_HEAP_CHECK
	char *file;
	int line;
	int grp;
#endif /* SM_HEAP_CHECK */
{
	char *newvalue;

	if (id < 0 || id > MAXMACROID)
		return;

	if (tTd(35, 9))
	{
		sm_dprintf("%sdefine(%s as ",
			mac->mac_table[id] == NULL ? "" : "re", macname(id));
		xputs(sm_debug_file(), value);
		sm_dprintf(")\n");
	}

	if (mac->mac_rpool == NULL)
	{
		char *freeit = NULL;

		if (mac->mac_table[id] != NULL &&
		    bitnset(id, mac->mac_allocated))
			freeit = mac->mac_table[id];

		if (value == NULL || vclass == A_HEAP)
		{
			sm_heap_checkptr_tagged(value, file, line);
			newvalue = value;
			clrbitn(id, mac->mac_allocated);
		}
		else
		{
#if SM_HEAP_CHECK
			newvalue = sm_strdup_tagged_x(value, file, line, 0);
#else /* SM_HEAP_CHECK */
			newvalue = sm_strdup_x(value);
#endif /* SM_HEAP_CHECK */
			setbitn(id, mac->mac_allocated);
		}
		mac->mac_table[id] = newvalue;
		if (freeit != NULL)
			sm_free(freeit);
	}
	else
	{
		if (value == NULL || vclass == A_PERM)
			newvalue = value;
		else
			newvalue = sm_rpool_strdup_x(mac->mac_rpool, value);
		mac->mac_table[id] = newvalue;
		if (vclass == A_HEAP)
			sm_free(value);
	}

#if _FFR_RESET_MACRO_GLOBALS
	switch (id)
	{
	  case 'j':
		PSTRSET(MyHostName, value);
		break;
	}
#endif /* _FFR_RESET_MACRO_GLOBALS */
}

/*
**  MACSET -- set a named macro to a value (low level)
**
**	No fancy storage management; the caller takes full responsibility.
**	Often used with macget; see also macdefine.
**
**	Parameters:
**		mac -- Macro table.
**		i -- Macro name, specified as an integer offset.
**		value -- Macro value: either NULL, or a string.
*/

void
macset(mac, i, value)
	MACROS_T *mac;
	int i;
	char *value;
{
	if (i < 0 || i > MAXMACROID)
		return;

	if (tTd(35, 9))
	{
		sm_dprintf("macset(%s as ", macname(i));
		xputs(sm_debug_file(), value);
		sm_dprintf(")\n");
	}
	mac->mac_table[i] = value;
}

/*
**  MACVALUE -- return uninterpreted value of a macro.
**
**	Does fancy path searching.
**	The low level counterpart is macget.
**
**	Parameters:
**		n -- the name of the macro.
**		e -- envelope in which to start looking for the macro.
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
	n = bitidx(n);
	if (e != NULL && e->e_mci != NULL)
	{
		register char *p = e->e_mci->mci_macro.mac_table[n];

		if (p != NULL)
			return p;
	}
	while (e != NULL)
	{
		register char *p = e->e_macro.mac_table[n];

		if (p != NULL)
			return p;
		if (e == e->e_parent)
			break;
		e = e->e_parent;
	}
	return GlobalMacros.mac_table[n];
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

	n = bitidx(n);
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
**  MACID_PARSE -- return id of macro identified by its name
**
**	Parameters:
**		p -- pointer to name string -- either a single
**			character or {name}.
**		ep -- filled in with the pointer to the byte
**			after the name.
**
**	Returns:
**		0 -- An error was detected.
**		1..255 -- The internal id code for this macro.
**
**	Side Effects:
**		If this is a new macro name, a new id is allocated.
**		On error, syserr is called.
*/

int
macid_parse(p, ep)
	register char *p;
	char **ep;
{
	int mid;
	register char *bp;
	char mbuf[MAXMACNAMELEN + 1];

	if (tTd(35, 14))
	{
		sm_dprintf("macid(");
		xputs(sm_debug_file(), p);
		sm_dprintf(") => ");
	}

	if (*p == '\0' || (p[0] == '{' && p[1] == '}'))
	{
		syserr("Name required for macro/class");
		if (ep != NULL)
			*ep = p;
		if (tTd(35, 14))
			sm_dprintf("NULL\n");
		return 0;
	}
	if (*p != '{')
	{
		/* the macro is its own code */
		if (ep != NULL)
			*ep = p + 1;
		if (tTd(35, 14))
			sm_dprintf("%c\n", bitidx(*p));
		return bitidx(*p);
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
			mbuf, (int) (sizeof mbuf - 1));
	}
	else if (mbuf[1] == '\0')
	{
		/* ${x} == $x */
		mid = bitidx(mbuf[0]);
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
				syserr("Macro/class {%s}: too many long names",
					mbuf);
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
	if (mid < 0 || mid > MAXMACROID)
	{
		syserr("Unable to assign macro/class ID (mid = 0x%x)", mid);
		if (tTd(35, 14))
			sm_dprintf("NULL\n");
		return 0;
	}
	if (tTd(35, 14))
		sm_dprintf("0x%x\n", mid);
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
**		true if str can be found in cl.
**		false otherwise.
*/

bool
wordinclass(str, cl)
	char *str;
	int cl;
{
	register STAB *s;

	s = stab(str, ST_CLASS, ST_FIND);
	return s != NULL && bitnset(bitidx(cl), s->s_class);
}
