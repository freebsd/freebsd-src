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
static char sccsid[] = "@(#)stab.c	8.13 (Berkeley) 4/19/97";
#endif /* not lint */

# include "sendmail.h"

/*
**  STAB -- manage the symbol table
**
**	Parameters:
**		name -- the name to be looked up or inserted.
**		type -- the type of symbol.
**		op -- what to do:
**			ST_ENTER -- enter the name if not
**				already present.
**			ST_FIND -- find it only.
**
**	Returns:
**		pointer to a STAB entry for this name.
**		NULL if not found and not entered.
**
**	Side Effects:
**		can update the symbol table.
*/

# define STABSIZE	2003

static STAB	*SymTab[STABSIZE];

STAB *
stab(name, type, op)
	char *name;
	int type;
	int op;
{
	register STAB *s;
	register STAB **ps;
	register int hfunc;
	register char *p;
	int len;
	extern char lower();

	if (tTd(36, 5))
		printf("STAB: %s %d ", name, type);

	/*
	**  Compute the hashing function
	*/

	hfunc = type;
	for (p = name; *p != '\0'; p++)
		hfunc = ((hfunc << 1) ^ (lower(*p) & 0377)) % STABSIZE;

	if (tTd(36, 9))
		printf("(hfunc=%d) ", hfunc);

	ps = &SymTab[hfunc];
	if (type == ST_MACRO || type == ST_RULESET)
	{
		while ((s = *ps) != NULL &&
		       (s->s_type != type || strcmp(name, s->s_name)))
			ps = &s->s_next;
	}
	else
	{
		while ((s = *ps) != NULL &&
		       (s->s_type != type || strcasecmp(name, s->s_name)))
			ps = &s->s_next;
	}

	/*
	**  Dispose of the entry.
	*/

	if (s != NULL || op == ST_FIND)
	{
		if (tTd(36, 5))
		{
			if (s == NULL)
				printf("not found\n");
			else
			{
				long *lp = (long *) s->s_class;

				printf("type %d val %lx %lx %lx %lx\n",
					s->s_type, lp[0], lp[1], lp[2], lp[3]);
			}
		}
		return (s);
	}

	/*
	**  Make a new entry and link it in.
	*/

	if (tTd(36, 5))
		printf("entered\n");

	/* determine size of new entry */
#if _FFR_MEMORY_MISER
	switch (type)
	{
	  case ST_CLASS:
		len = sizeof s->s_class;
		break;

	  case ST_ADDRESS:
		len = sizeof s->s_address;
		break;

	  case ST_MAILER:
		len = sizeof s->s_mailer;

	  case ST_ALIAS:
		len = sizeof s->s_alias;
		break;

	  case ST_MAPCLASS:
		len = sizeof s->s_mapclass;
		break;

	  case ST_MAP:
		len = sizeof s->s_map;
		break;

	  case ST_HOSTSIG:
		len = sizeof s->s_hostsig;
		break;

	  case ST_NAMECANON:
		len = sizeof s->s_namecanon;
		break;

	  case ST_MACRO:
		len = sizeof s->s_macro;
		break;

	  case ST_RULESET:
		len = sizeof s->s_ruleset;
		break;

	  case ST_SERVICE:
		len = sizeof s->s_service;
		break;

	  case ST_HEADER:
		len = sizeof s->s_header;
		break;

	  default:
		if (type >= ST_MCI)
			len = sizeof s->s_mci;
		else
		{
			syserr("stab: unknown symbol type %d", type);
			len = sizeof s->s_value;
		}
		break;
	}
	len += sizeof *s - sizeof s->s_value;
#else
	len = sizeof *s;
#endif

	/* make new entry */
	s = (STAB *) xalloc(len);
	bzero((char *) s, len);
	s->s_name = newstr(name);
	s->s_type = type;
	s->s_len = len;

	/* link it in */
	*ps = s;

	return (s);
}
/*
**  STABAPPLY -- apply function to all stab entries
**
**	Parameters:
**		func -- the function to apply.  It will be given one
**			parameter (the stab entry).
**		arg -- an arbitrary argument, passed to func.
**
**	Returns:
**		none.
*/

void
stabapply(func, arg)
	void (*func)__P((STAB *, int));
	int arg;
{
	register STAB **shead;
	register STAB *s;

	for (shead = SymTab; shead < &SymTab[STABSIZE]; shead++)
	{
		for (s = *shead; s != NULL; s = s->s_next)
		{
			if (tTd(36, 90))
				printf("stabapply: trying %d/%s\n",
					s->s_type, s->s_name);
			func(s, arg);
		}
	}
}
