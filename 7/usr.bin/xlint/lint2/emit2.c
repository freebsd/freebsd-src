/* $NetBSD: emit2.c,v 1.8 2002/01/21 19:49:52 tv Exp $ */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All Rights Reserved.
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: emit2.c,v 1.8 2002/01/21 19:49:52 tv Exp $");
#endif
__FBSDID("$FreeBSD$");

#include <err.h>

#include "lint2.h"

static	void	outtype(type_t *);
static	void	outdef(hte_t *, sym_t *);
static	void	dumpname(hte_t *);
static	void	outfiles(void);

/*
 * Write type into the output buffer.
 */
static void
outtype(type_t *tp)
{
	int	t, s, na;
	tspec_t	ts;
	type_t	**ap;

	while (tp != NULL) {
		if ((ts = tp->t_tspec) == INT && tp->t_isenum)
			ts = ENUM;
		switch (ts) {
		case CHAR:	t = 'C';	s = '\0';	break;
		case SCHAR:	t = 'C';	s = 's';	break;
		case UCHAR:	t = 'C';	s = 'u';	break;
		case SHORT:	t = 'S';	s = '\0';	break;
		case USHORT:	t = 'S';	s = 'u';	break;
		case INT:	t = 'I';	s = '\0';	break;
		case UINT:	t = 'I';	s = 'u';	break;
		case LONG:	t = 'L';	s = '\0';	break;
		case ULONG:	t = 'L';	s = 'u';	break;
		case QUAD:	t = 'Q';	s = '\0';	break;
		case UQUAD:	t = 'Q';	s = 'u';	break;
		case FLOAT:	t = 'D';	s = 's';	break;
		case DOUBLE:	t = 'D';	s = '\0';	break;
		case LDOUBLE:	t = 'D';	s = 'l';	break;
		case VOID:	t = 'V';	s = '\0';	break;
		case PTR:	t = 'P';	s = '\0';	break;
		case ARRAY:	t = 'A';	s = '\0';	break;
		case ENUM:	t = 'T';	s = 'e';	break;
		case STRUCT:	t = 'T';	s = 's';	break;
		case UNION:	t = 'T';	s = 'u';	break;
		case FUNC:
			if (tp->t_args != NULL && !tp->t_proto) {
				t = 'f';
			} else {
				t = 'F';
			}
			s = '\0';
			break;
		default:
			errx(1, "internal error: outtype() 1");
		}
		if (tp->t_const)
			outchar('c');
		if (tp->t_volatile)
			outchar('v');
		if (s != '\0')
			outchar(s);
		outchar(t);
		if (ts == ARRAY) {
			outint(tp->t_dim);
		} else if (ts == ENUM || ts == STRUCT || ts == UNION) {
			if (tp->t_istag) {
				outint(1);
				outname(tp->t_tag->h_name);
			} else if (tp->t_istynam) {
				outint(2);
				outname(tp->t_tynam->h_name);
			} else if (tp->t_isuniqpos) {
				outint(3);
				outint(tp->t_uniqpos.p_line);
				outchar('.');
				outint(tp->t_uniqpos.p_file);
				outchar('.');
				outint(tp->t_uniqpos.p_uniq);
			} else
				errx(1, "internal error: outtype() 2");
		} else if (ts == FUNC && tp->t_args != NULL) {
			na = 0;
			for (ap = tp->t_args; *ap != NULL; ap++)
				na++;
			if (tp->t_vararg)
				na++;
			outint(na);
			for (ap = tp->t_args; *ap != NULL; ap++)
				outtype(*ap);
			if (tp->t_vararg)
				outchar('E');
		}
		tp = tp->t_subt;
	}
}

/*
 * Write a definition.
 */
static void
outdef(hte_t *hte, sym_t *sym)
{

	/* reset output buffer */
	outclr();

	/* line number in C source file */
	outint(0);

	/* this is a definition */
	outchar('d');

	/* index of file where symbol was defined and line number of def. */
	outint(0);
	outchar('.');
	outint(0);

	/* flags */
	if (sym->s_va) {
		outchar('v');		/* varargs */
		outint(sym->s_nva);
	}
	if (sym->s_scfl) {
		outchar('S');		/* scanflike */
		outint(sym->s_nscfl);
	}
	if (sym->s_prfl) {
		outchar('P');		/* printflike */
		outint(sym->s_nprfl);
	}
	/* definition or tentative definition */
	outchar(sym->s_def == DEF ? 'd' : 't');
	if (TP(sym->s_type)->t_tspec == FUNC) {
		if (sym->s_rval)
			outchar('r');	/* fkt. has return value */
		if (sym->s_osdef)
			outchar('o');	/* old style definition */
	}
	outchar('u');			/* used (no warning if not used) */

	/* name */
	outname(hte->h_name);

	/* type */
	outtype(TP(sym->s_type));
}

/*
 * Write the first definition of a name into the lint library.
 */
static void
dumpname(hte_t *hte)
{
	sym_t	*sym, *def;

	/* static and undefined symbols are not written */
	if (hte->h_static || !hte->h_def)
		return;

	/*
	 * If there is a definition, write it. Otherwise write a tentative
	 * definition. This is necessary because more than one tentative
	 * definition is allowed (except with sflag).
	 */
	def = NULL;
	for (sym = hte->h_syms; sym != NULL; sym = sym->s_nxt) {
		if (sym->s_def == DEF) {
			def = sym;
			break;
		}
		if (sym->s_def == TDEF && def == NULL)
			def = sym;
	}
	if (def == NULL)
		errx(1, "internal error: dumpname() %s", hte->h_name);

	outdef(hte, def);
}

/*
 * Write a new lint library.
 */
void
outlib(const char *name)
{
	/* Open of output file and initialisation of the output buffer */
	outopen(name);

	/* write name of lint library */
	outsrc(name);

	/* name of lint lib has index 0 */
	outclr();
	outint(0);
	outchar('s');
	outstrg(name);

	/*
	 * print the names of all files references by unnamed
	 * struct/union/enum declarations.
	 */
	outfiles();

	/* write all definitions with external linkage */
	forall(dumpname);

	/* close the output */
	outclose();
}

/*
 * Write out the name of a file referenced by a type.
 */
struct outflist {
	short		ofl_num;
	struct outflist *ofl_next;
};
static struct outflist *outflist;

int
addoutfile(short num)
{
	struct outflist *ofl, **pofl;
	int i;

	ofl = outflist;
	pofl = &outflist;
	i = 1;				/* library is 0 */

	while (ofl != NULL) {
		if (ofl->ofl_num == num)
			break;

		pofl = &ofl->ofl_next;
		ofl = ofl->ofl_next;
		i++;
	}

	if (ofl == NULL) {
		ofl = *pofl = xmalloc(sizeof (struct outflist));
		ofl->ofl_num = num;
		ofl->ofl_next = NULL;
	}
	return (i);
}

static void
outfiles(void)
{
	struct outflist *ofl;
	int i;

	for (ofl = outflist, i = 1; ofl != NULL; ofl = ofl->ofl_next, i++) {
		/* reset output buffer */
		outclr();

		outint(i);
		outchar('s');
		outstrg(fnames[ofl->ofl_num]);
	}
}
