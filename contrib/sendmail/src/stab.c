/*
 * Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
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
static char id[] = "@(#)$Id: stab.c,v 8.40.16.2 2000/06/05 21:46:59 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>

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

#define STABSIZE	2003

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

	if (tTd(36, 5))
		dprintf("STAB: %s %d ", name, type);

	/*
	**  Compute the hashing function
	*/

	hfunc = type;
	for (p = name; *p != '\0'; p++)
		hfunc = ((hfunc << 1) ^ (lower(*p) & 0377)) % STABSIZE;

	if (tTd(36, 9))
		dprintf("(hfunc=%d) ", hfunc);

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
				dprintf("not found\n");
			else
			{
				long *lp = (long *) s->s_class;

				dprintf("type %d val %lx %lx %lx %lx\n",
					s->s_type, lp[0], lp[1], lp[2], lp[3]);
			}
		}
		return s;
	}

	/*
	**  Make a new entry and link it in.
	*/

	if (tTd(36, 5))
		dprintf("entered\n");

	/* determine size of new entry */
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
		break;

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

	  case ST_HEADER:
		len = sizeof s->s_header;
		break;

	  case ST_SERVICE:
		len = sizeof s->s_service;
		break;

#ifdef LDAPMAP
	  case ST_LDAP:
		len = sizeof s->s_ldap;
		break;
#endif /* LDAPMAP */

#if _FFR_MILTER
	  case ST_MILTER:
		len = sizeof s->s_milter;
		break;
#endif /* _FFR_MILTER */

	  default:
		/*
		**  Each mailer has it's own MCI stab entry:
		**
		**  s = stab(host, ST_MCI + m->m_mno, ST_ENTER);
		**
		**  Therefore, anything ST_MCI or larger is an s_mci.
		*/

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

	if (tTd(36, 15))
		dprintf("size of stab entry: %d\n", len);

	/* make new entry */
	s = (STAB *) xalloc(len);
	memset((char *) s, '\0', len);
	s->s_name = newstr(name);
	s->s_type = type;
	s->s_len = len;

	/* link it in */
	*ps = s;

	/* set a default value for rulesets */
	if (type == ST_RULESET)
		s->s_ruleset = -1;

	return s;
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
				dprintf("stabapply: trying %d/%s\n",
					s->s_type, s->s_name);
			func(s, arg);
		}
	}
}
/*
**  QUEUEUP_MACROS -- queueup the macros in a class
**
**	Write the macros listed in the specified class into the
**	file referenced by qfp.
**
**	Parameters:
**		class -- class ID.
**		qfp -- file pointer to the qf file.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

void
queueup_macros(class, qfp, e)
	int class;
	FILE *qfp;
	ENVELOPE *e;
{
	register STAB **shead;
	register STAB *s;

	if (e == NULL)
		return;

	for (shead = SymTab; shead < &SymTab[STABSIZE]; shead++)
	{
		for (s = *shead; s != NULL; s = s->s_next)
		{
			int m;
			char *p;

			if (s->s_type == ST_CLASS &&
			    bitnset(class & 0xff, s->s_class) &&
			    (m = macid(s->s_name, NULL)) != '\0' &&
			    (p = macvalue(m, e)) != NULL)
			{
				/*
				**  HACK ALERT: Unfortunately, 8.10 and
				**  8.11 reused the ${if_addr} and
				**  ${if_family} macros for both the incoming
				**  interface address/family (getrequests())
				**  and the outgoing interface address/family
				**  (makeconnection()).  In order for D_BINDIF
				**  to work properly, have to preserve the
				**  incoming information in the queue file for
				**  later delivery attempts.  The original
				**  information is stored in the envelope
				**  in readqf() so it can be stored in
				**  queueup_macros().  This should be fixed
				**  in 8.12.
				*/

				if (e->e_if_macros[EIF_ADDR] != NULL &&
				    strcmp(s->s_name, "{if_addr}") == 0)
					p = e->e_if_macros[EIF_ADDR];

				fprintf(qfp, "$%s%s\n",
					s->s_name,
					denlstring(p, TRUE, FALSE));
			}
		}
	}
}
/*
**  COPY_CLASS -- copy class members from one class to another
**
**	Parameters:
**		src -- source class.
**		dst -- destination class.
**
**	Returns:
**		none.
*/

void
copy_class(src, dst)
	int src;
	int dst;
{
	register STAB **shead;
	register STAB *s;

	for (shead = SymTab; shead < &SymTab[STABSIZE]; shead++)
	{
		for (s = *shead; s != NULL; s = s->s_next)
		{
			if (s->s_type == ST_CLASS &&
			    bitnset(src & 0xff, s->s_class))
				setbitn(dst, s->s_class);
		}
	}
}
