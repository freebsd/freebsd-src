/****************************************************************
Copyright 1990, 1991, 1992, 1993 by AT&T Bell Laboratories and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "defs.h"
#include "names.h"
#include "output.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

char _assoc_table[] = { 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 };

/* Opcode table -- This array is indexed by the OP_____ macros defined in
   defines.h; these macros are expected to be adjacent integers, so that
   this table is as small as possible. */

table_entry opcode_table[] = {
				{ 0, 0, NULL },
	/* OPPLUS 1 */		{ BINARY_OP, 12, "%l + %r" },
	/* OPMINUS 2 */		{ BINARY_OP, 12, "%l - %r" },
	/* OPSTAR 3 */		{ BINARY_OP, 13, "%l * %r" },
	/* OPSLASH 4 */		{ BINARY_OP, 13, "%l / %r" },
	/* OPPOWER 5 */		{ BINARY_OP,  0, "power (%l, %r)" },
	/* OPNEG 6 */		{ UNARY_OP,  14, "-%l" },
	/* OPOR 7 */		{ BINARY_OP,  4, "%l || %r" },
	/* OPAND 8 */		{ BINARY_OP,  5, "%l && %r" },
	/* OPEQV 9 */		{ BINARY_OP,  9, "%l == %r" },
	/* OPNEQV 10 */		{ BINARY_OP,  9, "%l != %r" },
	/* OPNOT 11 */		{ UNARY_OP,  14, "! %l" },
	/* OPCONCAT 12 */	{ BINARY_OP,  0, "concat (%l, %r)" },
	/* OPLT 13 */		{ BINARY_OP, 10, "%l < %r" },
	/* OPEQ 14 */		{ BINARY_OP,  9, "%l == %r" },
	/* OPGT 15 */		{ BINARY_OP, 10, "%l > %r" },
	/* OPLE 16 */		{ BINARY_OP, 10, "%l <= %r" },
	/* OPNE 17 */		{ BINARY_OP,  9, "%l != %r" },
	/* OPGE 18 */		{ BINARY_OP, 10, "%l >= %r" },
	/* OPCALL 19 */		{ BINARY_OP, 15, SPECIAL_FMT },
	/* OPCCALL 20 */	{ BINARY_OP, 15, SPECIAL_FMT },

/* Left hand side of an assignment cannot have outermost parens */

	/* OPASSIGN 21 */	{ BINARY_OP,  2, "%l = %r" },
	/* OPPLUSEQ 22 */	{ BINARY_OP,  2, "%l += %r" },
	/* OPSTAREQ 23 */	{ BINARY_OP,  2, "%l *= %r" },
	/* OPCONV 24 */		{ BINARY_OP, 14, "%l" },
	/* OPLSHIFT 25 */	{ BINARY_OP, 11, "%l << %r" },
	/* OPMOD 26 */		{ BINARY_OP, 13, "%l %% %r" },
	/* OPCOMMA 27 */	{ BINARY_OP,  1, "%l, %r" },

/* Don't want to nest the colon operator in parens */

	/* OPQUEST 28 */	{ BINARY_OP, 3, "%l ? %r" },
	/* OPCOLON 29 */	{ BINARY_OP, 3, "%l : %r" },
	/* OPABS 30 */		{ UNARY_OP,  0, "abs(%l)" },
	/* OPMIN 31 */		{ BINARY_OP,   0, SPECIAL_FMT },
	/* OPMAX 32 */		{ BINARY_OP,   0, SPECIAL_FMT },
	/* OPADDR 33 */		{ UNARY_OP, 14, "&%l" },

	/* OPCOMMA_ARG 34 */	{ BINARY_OP, 15, SPECIAL_FMT },
	/* OPBITOR 35 */	{ BINARY_OP,  6, "%l | %r" },
	/* OPBITAND 36 */	{ BINARY_OP,  8, "%l & %r" },
	/* OPBITXOR 37 */	{ BINARY_OP,  7, "%l ^ %r" },
	/* OPBITNOT 38 */	{ UNARY_OP,  14, "~ %l" },
	/* OPRSHIFT 39 */	{ BINARY_OP, 11, "%l >> %r" },

/* This isn't quite right -- it doesn't handle arrays, for instance */

	/* OPWHATSIN 40 */	{ UNARY_OP,  14, "*%l" },
	/* OPMINUSEQ 41 */	{ BINARY_OP,  2, "%l -= %r" },
	/* OPSLASHEQ 42 */	{ BINARY_OP,  2, "%l /= %r" },
	/* OPMODEQ 43 */	{ BINARY_OP,  2, "%l %%= %r" },
	/* OPLSHIFTEQ 44 */	{ BINARY_OP,  2, "%l <<= %r" },
	/* OPRSHIFTEQ 45 */	{ BINARY_OP,  2, "%l >>= %r" },
	/* OPBITANDEQ 46 */	{ BINARY_OP,  2, "%l &= %r" },
	/* OPBITXOREQ 47 */	{ BINARY_OP,  2, "%l ^= %r" },
	/* OPBITOREQ 48 */	{ BINARY_OP,  2, "%l |= %r" },
	/* OPPREINC 49 */	{ UNARY_OP,  14, "++%l" },
	/* OPPREDEC 50 */	{ UNARY_OP,  14, "--%l" },
	/* OPDOT 51 */		{ BINARY_OP, 15, "%l.%r" },
	/* OPARROW 52 */	{ BINARY_OP, 15, "%l -> %r"},
	/* OPNEG1 53 */		{ UNARY_OP,  14, "-%l" },
	/* OPDMIN 54 */		{ BINARY_OP, 0, "dmin(%l,%r)" },
	/* OPDMAX 55 */		{ BINARY_OP, 0, "dmax(%l,%r)" },
	/* OPASSIGNI 56 */	{ BINARY_OP,  2, "%l = &%r" },
	/* OPIDENTITY 57 */	{ UNARY_OP, 15, "%l" },
	/* OPCHARCAST 58 */	{ UNARY_OP, 14, "(char *)&%l" },
	/* OPDABS 59 */		{ UNARY_OP, 0, "dabs(%l)" },
	/* OPMIN2 60 */		{ BINARY_OP,   0, "min(%l,%r)" },
	/* OPMAX2 61 */		{ BINARY_OP,   0, "max(%l,%r)" },

/* kludge to imitate (under forcedouble) f77's bizarre treatement of OPNEG... */

	/* OPNEG KLUDGE */	{ UNARY_OP,  14, "-(doublereal)%l" }
}; /* opcode_table */

#define OPNEG_KLUDGE (sizeof(opcode_table)/sizeof(table_entry) - 1)

static char opeqable[sizeof(opcode_table)/sizeof(table_entry)];


static void output_prim ();
static void output_unary (), output_binary (), output_arg_list ();
static void output_list (), output_literal ();


void expr_out (fp, e)
FILE *fp;
expptr e;
{
    if (e == (expptr) NULL)
	return;

    switch (e -> tag) {
	case TNAME:	out_name (fp, (struct Nameblock *) e);
			return;

	case TCONST:	out_const(fp, &e->constblock);
			goto end_out;
	case TEXPR:
	    		break;

	case TADDR:	out_addr (fp, &(e -> addrblock));
			goto end_out;

	case TPRIM:	warn ("expr_out: got TPRIM");
			output_prim (fp, &(e -> primblock));
			return;

	case TLIST:	output_list (fp, &(e -> listblock));
 end_out:		frexpr(e);
			return;

	case TIMPLDO:	err ("expr_out: got TIMPLDO");
			return;

	case TERROR:
	default:
			erri ("expr_out: bad tag '%d'", e -> tag);
    } /* switch */

/* Now we know that the tag is TEXPR */

/* Optimize on simple expressions, such as "a = a + b" ==> "a += b" */

    if (e -> exprblock.opcode == OPASSIGN && e -> exprblock.rightp &&
	e -> exprblock.rightp -> tag == TEXPR) {
	int opcode;

	opcode = e -> exprblock.rightp -> exprblock.opcode;

	if (opeqable[opcode]) {
	    expptr leftp, rightp;

	    if ((leftp = e -> exprblock.leftp) &&
		(rightp = e -> exprblock.rightp -> exprblock.leftp)) {

		if (same_ident (leftp, rightp)) {
		    expptr temp = e -> exprblock.rightp;

		    e -> exprblock.opcode = op_assign(opcode);

		    e -> exprblock.rightp = temp -> exprblock.rightp;
		    temp->exprblock.rightp = 0;
		    frexpr(temp);
		} /* if same_ident (leftp, rightp) */
	    } /* if leftp && rightp */
	} /* if opcode == OPPLUS || */
    } /* if e -> exprblock.opcode == OPASSIGN */


/* Optimize on increment or decrement by 1 */

    {
	int opcode = e -> exprblock.opcode;
	expptr leftp = e -> exprblock.leftp;
	expptr rightp = e -> exprblock.rightp;

	if (leftp && rightp && (leftp -> headblock.vstg == STGARG ||
		ISINT (leftp -> headblock.vtype)) &&
		(opcode == OPPLUSEQ || opcode == OPMINUSEQ) &&
		ISINT (rightp -> headblock.vtype) &&
		ISICON (e -> exprblock.rightp) &&
		(ISONE (e -> exprblock.rightp) ||
		e -> exprblock.rightp -> constblock.Const.ci == -1)) {

/* Allow for the '-1' constant value */

	    if (!ISONE (e -> exprblock.rightp))
		opcode = (opcode == OPPLUSEQ) ? OPMINUSEQ : OPPLUSEQ;

/* replace the existing opcode */

	    if (opcode == OPPLUSEQ)
		e -> exprblock.opcode = OPPREINC;
	    else
		e -> exprblock.opcode = OPPREDEC;

/* Free up storage used by the right hand side */

	    frexpr (e -> exprblock.rightp);
	    e->exprblock.rightp = 0;
	} /* if opcode == OPPLUS */
    } /* block */


    if (is_unary_op (e -> exprblock.opcode))
	output_unary (fp, &(e -> exprblock));
    else if (is_binary_op (e -> exprblock.opcode))
	output_binary (fp, &(e -> exprblock));
    else
	erri ("expr_out: bad opcode '%d'", (int) e -> exprblock.opcode);

    free((char *)e);

} /* expr_out */


void out_and_free_statement (outfile, expr)
FILE *outfile;
expptr expr;
{
    if (expr)
	expr_out (outfile, expr);

    nice_printf (outfile, ";\n");
} /* out_and_free_statement */



int same_ident (left, right)
expptr left, right;
{
    if (!left || !right)
	return 0;

    if (left -> tag == TNAME && right -> tag == TNAME && left == right)
	return 1;

    if (left -> tag == TADDR && right -> tag == TADDR &&
	    left -> addrblock.uname_tag == right -> addrblock.uname_tag)
	switch (left -> addrblock.uname_tag) {
	    case UNAM_REF:
	    case UNAM_NAME:

/* Check for array subscripts */

		if (left -> addrblock.user.name -> vdim ||
			right -> addrblock.user.name -> vdim)
		    if (left -> addrblock.user.name !=
			    right -> addrblock.user.name ||
			    !same_expr (left -> addrblock.memoffset,
			    right -> addrblock.memoffset))
			return 0;

		return same_ident ((expptr) (left -> addrblock.user.name),
			(expptr) right -> addrblock.user.name);
	    case UNAM_IDENT:
		return strcmp(left->addrblock.user.ident,
				right->addrblock.user.ident) == 0;
	    case UNAM_CHARP:
		return strcmp(left->addrblock.user.Charp,
				right->addrblock.user.Charp) == 0;
	    default:
	        return 0;
	} /* switch */

    if (left->tag == TEXPR && left->exprblock.opcode == OPWHATSIN
	&& right->tag == TEXPR && right->exprblock.opcode == OPWHATSIN)
		return same_ident(left->exprblock.leftp,
				 right->exprblock.leftp);

    return 0;
} /* same_ident */

 static int
samefpconst(c1, c2, n)
 register Constp c1, c2;
 register int n;
{
	char *s1, *s2;
	if (!c1->vstg && !c2->vstg)
		return c1->Const.cd[n] == c2->Const.cd[n];
	s1 = c1->vstg ? c1->Const.cds[n] : dtos(c1->Const.cd[n]);
	s2 = c2->vstg ? c2->Const.cds[n] : dtos(c2->Const.cd[n]);
	return !strcmp(s1, s2);
	}

 static int
sameconst(c1, c2)
 register Constp c1, c2;
{
	switch(c1->vtype) {
		case TYCOMPLEX:
		case TYDCOMPLEX:
			if (!samefpconst(c1,c2,1))
				return 0;
		case TYREAL:
		case TYDREAL:
			return samefpconst(c1,c2,0);
		case TYCHAR:
			return c1->Const.ccp1.blanks == c2->Const.ccp1.blanks
			    &&	   c1->vleng->constblock.Const.ci
				== c2->vleng->constblock.Const.ci
			    && !memcmp(c1->Const.ccp, c2->Const.ccp,
					(int)c1->vleng->constblock.Const.ci);
		case TYSHORT:
		case TYINT:
		case TYLOGICAL:
			return c1->Const.ci == c2->Const.ci;
		}
	err("unexpected type in sameconst");
	return 0;
	}

/* same_expr -- Returns true only if   e1 and e2   match.  This is
   somewhat pessimistic, but can afford to be because it's just used to
   optimize on the assignment operators (+=, -=, etc). */

int same_expr (e1, e2)
expptr e1, e2;
{
    if (!e1 || !e2)
	return !e1 && !e2;

    if (e1 -> tag != e2 -> tag || e1 -> headblock.vtype != e2 -> headblock.vtype)
	return 0;

    switch (e1 -> tag) {
        case TEXPR:
	    if (e1 -> exprblock.opcode != e2 -> exprblock.opcode)
		return 0;

	    return same_expr (e1 -> exprblock.leftp, e2 -> exprblock.leftp) &&
		   same_expr (e1 -> exprblock.rightp, e2 -> exprblock.rightp);
	case TNAME:
	case TADDR:
	    return same_ident (e1, e2);
	case TCONST:
	    return sameconst(&e1->constblock, &e2->constblock);
	default:
	    return 0;
    } /* switch */
} /* same_expr */



void out_name (fp, namep)
 FILE *fp;
 Namep namep;
{
    extern int usedefsforcommon;
    Extsym *comm;

    if (namep == NULL)
	return;

/* DON'T want to use oneof_stg() here; need to find the right common name
   */

    if (namep->vstg == STGCOMMON && !namep->vcommequiv && !usedefsforcommon) {
	comm = &extsymtab[namep->vardesc.varno];
	extern_out(fp, comm);
	nice_printf(fp, "%d.", comm->curno);
    } /* if namep -> vstg == STGCOMMON */

    if (namep->vprocclass == PTHISPROC && namep->vtype != TYSUBR)
	nice_printf(fp, xretslot[namep->vtype]->user.ident);
    else
	nice_printf (fp, "%s", namep->cvarname);
} /* out_name */


static char *Longfmt = "%ld";

#define cpd(n) cp->vstg ? cp->Const.cds[n] : dtos(cp->Const.cd[n])

void out_const(fp, cp)
 FILE *fp;
 register Constp cp;
{
    static char real_buf[50], imag_buf[50];
    unsigned int k;
    int type = cp->vtype;

    switch (type) {
	case TYINT1:
        case TYSHORT:
	    nice_printf (fp, "%ld", cp->Const.ci);	/* don't cast ci! */
	    break;
	case TYLONG:
#ifdef TYQUAD
	case TYQUAD:
#endif
	    nice_printf (fp, Longfmt, cp->Const.ci);	/* don't cast ci! */
	    break;
	case TYREAL:
	    nice_printf(fp, "%s", flconst(real_buf, cpd(0)));
	    break;
	case TYDREAL:
	    nice_printf(fp, "%s", cpd(0));
	    break;
	case TYCOMPLEX:
	    nice_printf(fp, cm_fmt_string, flconst(real_buf, cpd(0)),
			flconst(imag_buf, cpd(1)));
	    break;
	case TYDCOMPLEX:
	    nice_printf(fp, dcm_fmt_string, cpd(0), cpd(1));
	    break;
	case TYLOGICAL1:
	case TYLOGICAL2:
	case TYLOGICAL:
	    nice_printf (fp, "%s", cp->Const.ci ? "TRUE_" : "FALSE_");
	    break;
	case TYCHAR: {
	    char *c = cp->Const.ccp, *ce;

	    if (c == NULL) {
		nice_printf (fp, "\"\"");
		break;
	    } /* if c == NULL */

	    nice_printf (fp, "\"");
	    ce = c + cp->vleng->constblock.Const.ci;
	    while(c < ce) {
		k = *(unsigned char *)c++;
		nice_printf(fp, str_fmt[k], k);
		}
	    for(k = cp->Const.ccp1.blanks; k > 0; k--)
		nice_printf(fp, " ");
	    nice_printf (fp, "\"");
	    break;
	} /* case TYCHAR */
	default:
	    erri ("out_const:  bad type '%d'", (int) type);
	    break;
    } /* switch */

} /* out_const */
#undef cpd

 static void
out_args(fp, ep) FILE *fp; expptr ep;
{
	chainp arglist;

	if(ep->tag != TLIST)
		badtag("out_args", ep->tag);
	for(arglist = ep->listblock.listp;;) {
		expr_out(fp, (expptr)arglist->datap);
		arglist->datap = 0;
		if (!(arglist = arglist->nextp))
			break;
		nice_printf(fp, ", ");
		}
	}


/* out_addr -- this routine isn't local because it is called by the
   system-generated identifier printing routines */

void out_addr (fp, addrp)
FILE *fp;
struct Addrblock *addrp;
{
	extern Extsym *extsymtab;
	int was_array = 0;
	char *s;


	if (addrp == NULL)
		return;
	if (doin_setbound
			&& addrp->vstg == STGARG
			&& addrp->vtype != TYCHAR
			&& ISICON(addrp->memoffset)
			&& !addrp->memoffset->constblock.Const.ci)
		nice_printf(fp, "*");

	switch (addrp -> uname_tag) {
	    case UNAM_REF:
		nice_printf(fp, "%s_%s(", addrp->user.name->cvarname,
			addrp->cmplx_sub ? "subscr" : "ref");
		out_args(fp, addrp->memoffset);
		nice_printf(fp, ")");
		return;
	    case UNAM_NAME:
		out_name (fp, addrp -> user.name);
		break;
	    case UNAM_IDENT:
		if (*(s = addrp->user.ident) == ' ') {
			if (multitype)
				nice_printf(fp, "%s",
					xretslot[addrp->vtype]->user.ident);
			else
				nice_printf(fp, "%s", s+1);
			}
		else {
			nice_printf(fp, "%s", s);
			}
		break;
	    case UNAM_CHARP:
		nice_printf(fp, "%s", addrp->user.Charp);
		break;
	    case UNAM_EXTERN:
		extern_out (fp, &extsymtab[addrp -> memno]);
		break;
	    case UNAM_CONST:
		switch(addrp->vstg) {
			case STGCONST:
				out_const(fp, (Constp)addrp);
				break;
			case STGMEMNO:
				output_literal (fp, (int)addrp->memno,
					(Constp)addrp);
				break;
			default:
			Fatal("unexpected vstg in out_addr");
			}
		break;
	    case UNAM_UNKNOWN:
	    default:
		nice_printf (fp, "Unknown Addrp");
		break;
	} /* switch */

/* It's okay to just throw in the brackets here because they have a
   precedence level of 15, the highest value.  */

    if ((addrp->uname_tag == UNAM_NAME && addrp->user.name->vdim
			|| addrp->ntempelt > 1 || addrp->isarray)
	&& addrp->vtype != TYCHAR) {
	expptr offset;

	was_array = 1;

	offset = addrp -> memoffset;
	addrp->memoffset = 0;
	if (ONEOF(addrp->vstg, M(STGCOMMON)|M(STGEQUIV))
		&& addrp -> uname_tag == UNAM_NAME
		&& !addrp->skip_offset)
	    offset = mkexpr (OPMINUS, offset, mkintcon (
		    addrp -> user.name -> voffset));

	nice_printf (fp, "[");

	offset = mkexpr (OPSLASH, offset,
		ICON (typesize[addrp -> vtype] * (addrp -> Field ? 2 : 1)));
	expr_out (fp, offset);
	nice_printf (fp, "]");
	}

/* Check for structure field reference */

    if (addrp -> Field && addrp -> uname_tag != UNAM_CONST &&
	    addrp -> uname_tag != UNAM_UNKNOWN) {
	if (oneof_stg((addrp -> uname_tag == UNAM_NAME ? addrp -> user.name :
		(Namep) NULL), addrp -> vstg, M(STGARG)|M(STGEQUIV))
		&& !was_array && (addrp->vclass != CLPROC || !multitype))
	    nice_printf (fp, "->%s", addrp -> Field);
	else
	    nice_printf (fp, ".%s", addrp -> Field);
    } /* if */

/* Check for character subscripting */

    if (addrp->vtype == TYCHAR &&
	    (addrp->vclass != CLPROC || addrp->uname_tag == UNAM_NAME
			&& addrp->user.name->vprocclass == PTHISPROC) &&
	    addrp -> memoffset &&
	    (addrp -> uname_tag != UNAM_NAME ||
	     addrp -> user.name -> vtype == TYCHAR) &&
	    (!ISICON (addrp -> memoffset) ||
	     (addrp -> memoffset -> constblock.Const.ci))) {

	int use_paren = 0;
	expptr e = addrp -> memoffset;

	if (!e)
		return;
	addrp->memoffset = 0;

	if (ONEOF(addrp->vstg, M(STGCOMMON)|M(STGEQUIV))
	 && addrp -> uname_tag == UNAM_NAME) {
	    e = mkexpr (OPMINUS, e, mkintcon (addrp -> user.name -> voffset));

/* mkexpr will simplify it to zero if possible */
	    if (e->tag == TCONST && e->constblock.Const.ci == 0)
		return;
	} /* if addrp -> vstg == STGCOMMON */

/* In the worst case, parentheses might be needed OUTSIDE the expression,
   too.  But since I think this subscripting can only appear as a
   parameter in a procedure call, I don't think outside parens will ever
   be needed.  INSIDE parens are handled below */

	nice_printf (fp, " + ");
	if (e -> tag == TEXPR) {
	    int arg_prec = op_precedence (e -> exprblock.opcode);
	    int prec = op_precedence (OPPLUS);
	    use_paren = arg_prec && (arg_prec < prec || (arg_prec == prec &&
		    is_left_assoc (OPPLUS)));
	} /* if e -> tag == TEXPR */
	if (use_paren) nice_printf (fp, "(");
	expr_out (fp, e);
	if (use_paren) nice_printf (fp, ")");
    } /* if */
} /* out_addr */


static void output_literal (fp, memno, cp)
 FILE *fp;
 int memno;
 Constp cp;
{
    struct Literal *litp, *lastlit;
    extern char *lit_name ();

    lastlit = litpool + nliterals;

    for (litp = litpool; litp < lastlit; litp++) {
	if (litp -> litnum == memno)
	    break;
    } /* for litp */

    if (litp >= lastlit)
	out_const (fp, cp);
    else {
	nice_printf (fp, "%s", lit_name (litp));
	litp->lituse++;
	}
} /* output_literal */


static void output_prim (fp, primp)
FILE *fp;
struct Primblock *primp;
{
    if (primp == NULL)
	return;

    out_name (fp, primp -> namep);
    if (primp -> argsp)
	output_arg_list (fp, primp -> argsp);

    if (primp -> fcharp != (expptr) NULL || primp -> lcharp != (expptr) NULL)
	nice_printf (fp, "Sorry, no substrings yet");
}



static void output_arg_list (fp, listp)
FILE *fp;
struct Listblock *listp;
{
    chainp arg_list;

    if (listp == (struct Listblock *) NULL || listp -> listp == (chainp) NULL)
	return;

    nice_printf (fp, "(");

    for (arg_list = listp -> listp; arg_list; arg_list = arg_list -> nextp) {
	expr_out (fp, (expptr) arg_list -> datap);
	if (arg_list -> nextp != (chainp) NULL)

/* Might want to add a hook in here to accomodate the style setting which
   wants spaces after commas */

	    nice_printf (fp, ",");
    } /* for arg_list */

    nice_printf (fp, ")");
} /* output_arg_list */



static void output_unary (fp, e)
FILE *fp;
struct Exprblock *e;
{
    if (e == NULL)
	return;

    switch (e -> opcode) {
        case OPNEG:
		if (e->vtype == TYREAL && forcedouble) {
			e->opcode = OPNEG_KLUDGE;
			output_binary(fp,e);
			e->opcode = OPNEG;
			break;
			}
	case OPNEG1:
	case OPNOT:
	case OPABS:
	case OPBITNOT:
	case OPWHATSIN:
	case OPPREINC:
	case OPPREDEC:
	case OPADDR:
	case OPIDENTITY:
	case OPCHARCAST:
	case OPDABS:
	    output_binary (fp, e);
	    break;
	case OPCALL:
	case OPCCALL:
	    nice_printf (fp, "Sorry, no OPCALL yet");
	    break;
	default:
	    erri ("output_unary: bad opcode", (int) e -> opcode);
	    break;
    } /* switch */
} /* output_unary */


 static char *
findconst(m)
 register long m;
{
	register struct Literal *litp, *litpe;

	litp = litpool;
	for(litpe = litp + nliterals; litp < litpe; litp++)
		if (litp->litnum ==  m)
			return litp->cds[0];
	Fatal("findconst failure!");
	return 0;
	}

 static int
opconv_fudge(fp,e)
 FILE *fp;
 struct Exprblock *e;
{
	/* special handling for ichar and character*1 */
	register expptr lp;
	register union Expression *Offset;
	register char *cp;
	int lt;
	char buf[8];
	unsigned int k;
	Namep np;

	if (!(lp = e->leftp))	/* possible with erroneous Fortran */
		return 1;
	lt = lp->headblock.vtype;
	if (lt == TYCHAR) {
		switch(lp->tag) {
			case TNAME:
				nice_printf(fp, "*");
				out_name(fp, (Namep)lp);
				return 1;
			case TCONST:
 tconst:
				cp = lp->constblock.Const.ccp;
 tconst1:
				k = *(unsigned char *)cp;
				sprintf(buf, chr_fmt[k], k);
				nice_printf(fp, "'%s'", buf);
				return 1;
			case TADDR:
				switch(lp->addrblock.vstg) {
				    case STGMEMNO:
					if (halign && e->vtype != TYCHAR) {
						nice_printf(fp, "*(%s *)",
						    c_type_decl(e->vtype,0));
						expr_out(fp, lp);
						return 1;
						}
					cp = findconst(lp->addrblock.memno);
					goto tconst1;
				    case STGCONST:
					goto tconst;
				    }
				lp->addrblock.vtype = tyint;
				Offset = lp->addrblock.memoffset;
				switch(lp->addrblock.uname_tag) {
				  case UNAM_REF:
					nice_printf(fp, "*");
					return 0;
				  case UNAM_NAME:
					np = lp->addrblock.user.name;
					if (ONEOF(np->vstg,
					    M(STGCOMMON)|M(STGEQUIV)))
						Offset = mkexpr(OPMINUS, Offset,
							ICON(np->voffset));
					}
				lp->addrblock.memoffset = Offset ?
					mkexpr(OPSTAR, Offset,
						ICON(typesize[tyint]))
					: ICON(0);
				lp->addrblock.isarray = 1;
				/* STGCOMMON or STGEQUIV would cause */
				/* voffset to be added in a second time */
				lp->addrblock.vstg = STGUNKNOWN;
				break;
			default:
				badtag("opconv_fudge", lp->tag);
			}
		}
	if (lt != e->vtype)
		nice_printf(fp, "(%s) ",
			c_type_decl(e->vtype, 0));
	return 0;
	}


static void output_binary (fp, e)
FILE *fp;
struct Exprblock *e;
{
    char *format;
    extern table_entry opcode_table[];
    int prec;

    if (e == NULL || e -> tag != TEXPR)
	return;

/* Instead of writing a huge switch, I've incorporated the output format
   into a table.  Things like "%l" and "%r" stand for the left and
   right subexpressions.  This should allow both prefix and infix
   functions to be specified (e.g. "(%l * %r", "z_div (%l, %r").  Of
   course, I should REALLY think out the ramifications of writing out
   straight text, as opposed to some intermediate format, which could
   figure out and optimize on the the number of required blanks (we don't
   want "x - (-y)" to become "x --y", for example).  Special cases (such as
   incomplete implementations) could still be implemented as part of the
   switch, they will just have some dummy value instead of the string
   pattern.  Another difficulty is the fact that the complex functions
   will differ from the integer and real ones */

/* Handle a special case.  We don't want to output "x + - 4", or "y - - 3"
*/
    if ((e -> opcode == OPPLUS || e -> opcode == OPMINUS) &&
	    e -> rightp && e -> rightp -> tag == TCONST &&
	    isnegative_const (&(e -> rightp -> constblock)) &&
	    is_negatable (&(e -> rightp -> constblock))) {

	e -> opcode = (e -> opcode == OPPLUS) ? OPMINUS : OPPLUS;
	negate_const (&(e -> rightp -> constblock));
    } /* if e -> opcode == PLUS or MINUS */

    prec = op_precedence (e -> opcode);
    format = op_format (e -> opcode);

    if (format != SPECIAL_FMT) {
	while (*format) {
	    if (*format == '%') {
		int arg_prec, use_paren = 0;
		expptr lp, rp;

		switch (*(format + 1)) {
		    case 'l':
			lp = e->leftp;
			if (lp && lp->tag == TEXPR) {
			    arg_prec = op_precedence(lp->exprblock.opcode);

			    use_paren = arg_prec &&
			        (arg_prec < prec || (arg_prec == prec &&
				    is_right_assoc (prec)));
			} /* if e -> leftp */
			if (e->opcode == OPCONV && opconv_fudge(fp,e))
				break;
			if (use_paren)
			    nice_printf (fp, "(");
		        expr_out(fp, lp);
			if (use_paren)
			    nice_printf (fp, ")");
		        break;
		    case 'r':
			rp = e->rightp;
			if (rp && rp->tag == TEXPR) {
			    arg_prec = op_precedence(rp->exprblock.opcode);

			    use_paren = arg_prec &&
			        (arg_prec < prec || (arg_prec == prec &&
				    is_left_assoc (prec)));
			    use_paren = use_paren ||
				(rp->exprblock.opcode == OPNEG
				&& prec >= op_precedence(OPMINUS));
			} /* if e -> rightp */
			if (use_paren)
			    nice_printf (fp, "(");
		        expr_out(fp, rp);
			if (use_paren)
			    nice_printf (fp, ")");
		        break;
		    case '\0':
		    case '%':
		        nice_printf (fp, "%%");
		        break;
		    default:
		        erri ("output_binary: format err: '%%%c' illegal",
				(int) *(format + 1));
		        break;
		} /* switch */
		format += 2;
	    } else
		nice_printf (fp, "%c", *format++);
	} /* while *format */
    } else {

/* Handle Special cases of formatting */

	switch (e -> opcode) {
		case OPCCALL:
		case OPCALL:
			out_call (fp, (int) e -> opcode, e -> vtype,
					e -> vleng, e -> leftp, e -> rightp);
			break;

		case OPCOMMA_ARG:
			doin_setbound = 1;
			nice_printf(fp, "(");
			expr_out(fp, e->leftp);
			nice_printf(fp, ", &");
			doin_setbound = 0;
			expr_out(fp, e->rightp);
			nice_printf(fp, ")");
			break;

		case OPADDR:
		default:
	        	nice_printf (fp, "Sorry, can't format OPCODE '%d'",
				e -> opcode);
	        	break;
		}

    } /* else */
} /* output_binary */


out_call (outfile, op, ftype, len, name, args)
FILE *outfile;
int op, ftype;
expptr len, name, args;
{
    chainp arglist;		/* Pointer to any actual arguments */
    chainp cp;			/* Iterator over argument lists */
    Addrp ret_val = (Addrp) NULL;
				/* Function return value buffer, if any is
				   required */
    int byvalue;		/* True iff we're calling a C library
				   routine */
    int done_once;		/* Used for writing commas to   outfile   */
    int narg, t;
    register expptr q;
    long L;
    Argtypes *at;
    Atype *A, *Ac;
    Namep np;
    extern int forcereal;

/* Don't use addresses if we're calling a C function */

    byvalue = op == OPCCALL;

    if (args)
	arglist = args -> listblock.listp;
    else
	arglist = CHNULL;

/* If this is a CHARACTER function, the first argument is the result */

    if (ftype == TYCHAR)
	if (ISICON (len)) {
	    ret_val = (Addrp) (arglist -> datap);
	    arglist = arglist -> nextp;
	} else {
	    err ("adjustable character function");
	    return;
	} /* else */

/* If this is a COMPLEX function, the first argument is the result */

    else if (ISCOMPLEX (ftype)) {
	ret_val = (Addrp) (arglist -> datap);
	arglist = arglist -> nextp;
    } /* if ISCOMPLEX */

/* Now we can actually start to write out the function invocation */

    if (ftype == TYREAL && forcereal)
	nice_printf(outfile, "(real)");
    if (name -> tag == TEXPR && name -> exprblock.opcode == OPWHATSIN) {
	nice_printf (outfile, "(");
	np = (Namep)name->exprblock.leftp; /*expr_out will free name */
	expr_out (outfile, name);
	nice_printf (outfile, ")");
	}
    else {
	np = (Namep)name;
	expr_out(outfile, name);
	}

    /* prepare to cast procedure parameters -- set A if we know how */

    A = Ac = 0;
    if (np->tag == TNAME && (at = np->arginfo)) {
	if (at->nargs > 0)
		A = at->atypes;
	if (Ansi && (at->defined || at->nargs > 0))
		Ac = at->atypes;
    	}

    nice_printf(outfile, "(");

    if (ret_val) {
	if (ISCOMPLEX (ftype))
	    nice_printf (outfile, "&");
	expr_out (outfile, (expptr) ret_val);
	if (Ac)
		Ac++;

/* The length of the result of a character function is the second argument */
/* It should be in place from putcall(), so we won't touch it explicitly */

    } /* if ret_val */
    done_once = ret_val ? TRUE : FALSE;

/* Now run through the named arguments */

    narg = -1;
    for (cp = arglist; cp; cp = cp -> nextp, done_once = TRUE) {

	if (done_once)
	    nice_printf (outfile, ", ");
	narg++;

	if (!( q = (expptr)cp->datap) )
		continue;

	if (q->tag == TADDR) {
		if (q->addrblock.vtype > TYERROR) {
			/* I/O block */
			nice_printf(outfile, "&%s", q->addrblock.user.ident);
			continue;
			}
		if (!byvalue && q->addrblock.isarray
		&& q->addrblock.vtype != TYCHAR
		&& q->addrblock.memoffset->tag == TCONST) {

			/* check for 0 offset -- after */
			/* correcting for equivalence. */
			L = q->addrblock.memoffset->constblock.Const.ci;
			if (ONEOF(q->addrblock.vstg, M(STGCOMMON)|M(STGEQUIV))
					&& q->addrblock.uname_tag == UNAM_NAME)
				L -= q->addrblock.user.name->voffset;
			if (L)
				goto skip_deref;

			if (Ac && narg < at->dnargs
			 && q->headblock.vtype != (t = Ac[narg].type)
			 && t > TYADDR && t < TYSUBR)
				nice_printf(outfile, "(%s*)", typename[t]);

			/* &x[0] == x */
			/* This also prevents &sizeof(doublereal)[0] */

			switch(q->addrblock.uname_tag) {
			    case UNAM_NAME:
				out_name(outfile, q->addrblock.user.name);
				continue;
			    case UNAM_IDENT:
				nice_printf(outfile, "%s",
					q->addrblock.user.ident);
				continue;
			    case UNAM_CHARP:
				nice_printf(outfile, "%s",
					q->addrblock.user.Charp);
				continue;
			    case UNAM_EXTERN:
				extern_out(outfile,
					&extsymtab[q->addrblock.memno]);
				continue;
			    }
			}
		}

/* Skip over the dereferencing operator generated only for the
   intermediate file */
 skip_deref:
	if (q -> tag == TEXPR && q -> exprblock.opcode == OPWHATSIN)
	    q = q -> exprblock.leftp;

	if (q->headblock.vclass == CLPROC) {
	    if (Castargs && (q->tag != TNAME
				|| q->nameblock.vprocclass != PTHISPROC)
			 && (q->tag != TADDR
				|| q->addrblock.uname_tag != UNAM_NAME
				|| q->addrblock.user.name->vprocclass
								!= PTHISPROC))
		{
		if (A && (t = A[narg].type) >= 200)
			t %= 100;
		else {
			t = q->headblock.vtype;
			if (q->tag == TNAME && q->nameblock.vimpltype)
				t = TYUNKNOWN;
			}
		nice_printf(outfile, "(%s)", usedcasts[t] = casttypes[t]);
		}
	    }
	else if (Ac && narg < at->dnargs
		&& q->headblock.vtype != (t = Ac[narg].type)
		&& t > TYADDR && t < TYSUBR)
		nice_printf(outfile, "(%s*)", typename[t]);

	if ((q -> tag == TADDR || q-> tag == TNAME) &&
		(byvalue || q -> headblock.vstg != STGREG)) {
	    if (q -> headblock.vtype != TYCHAR)
	      if (byvalue) {

		if (q -> tag == TADDR &&
			q -> addrblock.uname_tag == UNAM_NAME &&
			! q -> addrblock.user.name -> vdim &&
			oneof_stg(q -> addrblock.user.name, q -> addrblock.vstg,
					M(STGARG)|M(STGEQUIV)) &&
			! ISCOMPLEX(q->addrblock.user.name->vtype))
		    nice_printf (outfile, "*");
		else if (q -> tag == TNAME
			&& oneof_stg(&q->nameblock, q -> nameblock.vstg,
				M(STGARG)|M(STGEQUIV))
			&& !(q -> nameblock.vdim))
		    nice_printf (outfile, "*");

	      } else {
		expptr memoffset;

		if (q->tag == TADDR &&
			!ONEOF (q -> addrblock.vstg, M(STGEXT)|M(STGLENG))
			&& (
			ONEOF(q->addrblock.vstg,
				M(STGCOMMON)|M(STGEQUIV)|M(STGMEMNO))
			|| ((memoffset = q->addrblock.memoffset)
				&& (!ISICON(memoffset)
				|| memoffset->constblock.Const.ci)))
			|| ONEOF(q->addrblock.vstg,
					M(STGINIT)|M(STGAUTO)|M(STGBSS))
				&& !q->addrblock.isarray)
		    nice_printf (outfile, "&");
		else if (q -> tag == TNAME
			&& !oneof_stg(&q->nameblock, q -> nameblock.vstg,
				M(STGARG)|M(STGEXT)|M(STGEQUIV)))
		    nice_printf (outfile, "&");
	    } /* else */

	    expr_out (outfile, q);
	} /* if q -> tag == TADDR || q -> tag == TNAME */

/* Might be a Constant expression, e.g. string length, character constants */

	else if (q -> tag == TCONST) {
	    if (tyioint == TYLONG)
	   	Longfmt = "%ldL";
	    out_const(outfile, &q->constblock);
	    Longfmt = "%ld";
	    }

/* Must be some other kind of expression, or register var, or constant.
   In particular, this is likely to be a temporary variable assignment
   which was generated in p1put_call */

	else if (!ISCOMPLEX (q -> headblock.vtype) && !ISCHAR (q)){
	    int use_paren = q -> tag == TEXPR &&
		    op_precedence (q -> exprblock.opcode) <=
		    op_precedence (OPCOMMA);

	    if (use_paren) nice_printf (outfile, "(");
	    expr_out (outfile, q);
	    if (use_paren) nice_printf (outfile, ")");
	} /* if !ISCOMPLEX */
	else
	    err ("out_call:  unknown parameter");

    } /* for (cp = arglist */

    if (arglist)
	frchain (&arglist);

    nice_printf (outfile, ")");

} /* out_call */


 char *
flconst(buf, x)
 char *buf, *x;
{
	sprintf(buf, fl_fmt_string, x);
	return buf;
	}

 char *
dtos(x)
 double x;
{
	static char buf[64];
	sprintf(buf, db_fmt_string, x);
	return buf;
	}

char tr_tab[Table_size];

/* out_init -- Initialize the data structures used by the routines in
   output.c.  These structures include the output format to be used for
   Float, Double, Complex, and Double Complex constants. */

void out_init ()
{
    extern int tab_size;
    register char *s;

    s = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+-.";
    while(*s)
	tr_tab[*s++] = 3;
    tr_tab['>'] = 1;

	opeqable[OPPLUS] = 1;
	opeqable[OPMINUS] = 1;
	opeqable[OPSTAR] = 1;
	opeqable[OPSLASH] = 1;
	opeqable[OPMOD] = 1;
	opeqable[OPLSHIFT] = 1;
	opeqable[OPBITAND] = 1;
	opeqable[OPBITXOR] = 1;
	opeqable[OPBITOR ] = 1;


/* Set the output format for both types of floating point constants */

    if (fl_fmt_string == NULL || *fl_fmt_string == '\0')
	fl_fmt_string = Ansi == 1 ? "%sf" : "(float)%s";

    if (db_fmt_string == NULL || *db_fmt_string == '\0')
	db_fmt_string = "%.17g";

/* Set the output format for both types of complex constants.  They will
   have string parameters rather than float or double so that the decimal
   point may be added to the strings generated by the {db,fl}_fmt_string
   formats above */

    if (cm_fmt_string == NULL || *cm_fmt_string == '\0') {
	cm_fmt_string = "{%s,%s}";
    } /* if cm_fmt_string == NULL */

    if (dcm_fmt_string == NULL || *dcm_fmt_string == '\0') {
	dcm_fmt_string = "{%s,%s}";
    } /* if dcm_fmt_string == NULL */

    tab_size = 4;
} /* out_init */


void extern_out (fp, extsym)
FILE *fp;
Extsym *extsym;
{
    if (extsym == (Extsym *) NULL)
	return;

    nice_printf (fp, "%s", extsym->cextname);

} /* extern_out */



static void output_list (fp, listp)
FILE *fp;
struct Listblock *listp;
{
    int did_one = 0;
    chainp elts;

    nice_printf (fp, "(");
    if (listp)
	for (elts = listp -> listp; elts; elts = elts -> nextp) {
	    if (elts -> datap) {
		if (did_one)
		    nice_printf (fp, ", ");
		expr_out (fp, (expptr) elts -> datap);
		did_one = 1;
	    } /* if elts -> datap */
	} /* for elts */
    nice_printf (fp, ")");
} /* output_list */


void out_asgoto (outfile, expr)
FILE *outfile;
expptr expr;
{
    char *user_label();
    chainp value;
    Namep namep;
    int k;

    if (expr == (expptr) NULL) {
	err ("out_asgoto:  NULL variable expr");
	return;
    } /* if expr */

    nice_printf (outfile, Ansi ? "switch (" : "switch ((int)"); /*)*/
    expr_out (outfile, expr);
    nice_printf (outfile, ") {\n");
    next_tab (outfile);

/* The initial addrp value will be stored as a namep pointer */

    switch(expr->tag) {
	case TNAME:
		/* local variable */
		namep = &expr->nameblock;
		break;
	case TEXPR:
		if (expr->exprblock.opcode == OPWHATSIN
		 && expr->exprblock.leftp->tag == TNAME)
			/* argument */
			namep = &expr->exprblock.leftp->nameblock;
		else
			goto bad;
		break;
	case TADDR:
		if (expr->addrblock.uname_tag == UNAM_NAME) {
			/* initialized local variable */
			namep = expr->addrblock.user.name;
			break;
			}
	default:
 bad:
		err("out_asgoto:  bad expr");
		return;
	}

    for(k = 0, value = namep -> varxptr.assigned_values; value;
	    value = value->nextp, k++) {
	nice_printf (outfile, "case %d: goto %s;\n", k,
		user_label((long)value->datap));
    } /* for value */
    prev_tab (outfile);

    nice_printf (outfile, "}\n");
} /* out_asgoto */

void out_if (outfile, expr)
FILE *outfile;
expptr expr;
{
    nice_printf (outfile, "if (");
    expr_out (outfile, expr);
    nice_printf (outfile, ") {\n");
    next_tab (outfile);
} /* out_if */

 static void
output_rbrace(outfile, s)
 FILE *outfile;
 char *s;
{
	extern int last_was_label;
	register char *fmt;

	if (last_was_label) {
		last_was_label = 0;
		fmt = ";%s";
		}
	else
		fmt = "%s";
	nice_printf(outfile, fmt, s);
	}

void out_else (outfile)
FILE *outfile;
{
    prev_tab (outfile);
    output_rbrace(outfile, "} else {\n");
    next_tab (outfile);
} /* out_else */

void elif_out (outfile, expr)
FILE *outfile;
expptr expr;
{
    prev_tab (outfile);
    output_rbrace(outfile, "} else ");
    out_if (outfile, expr);
} /* elif_out */

void endif_out (outfile)
FILE *outfile;
{
    prev_tab (outfile);
    output_rbrace(outfile, "}\n");
} /* endif_out */

void end_else_out (outfile)
FILE *outfile;
{
    prev_tab (outfile);
    output_rbrace(outfile, "}\n");
} /* end_else_out */



void compgoto_out (outfile, index, labels)
FILE *outfile;
expptr index, labels;
{
    char *s1, *s2;

    if (index == ENULL)
	err ("compgoto_out:  null index for computed goto");
    else if (labels && labels -> tag != TLIST)
	erri ("compgoto_out:  expected label list, got tag '%d'",
		labels -> tag);
    else {
	extern char *user_label ();
	chainp elts;
	int i = 1;

	s2 = /*(*/ ") {\n"; /*}*/
	if (Ansi)
		s1 = "switch ("; /*)*/
	else if (index->tag == TNAME || index->tag == TEXPR
				&& index->exprblock.opcode == OPWHATSIN)
		s1 = "switch ((int)"; /*)*/
	else {
		s1 = "switch ((int)(";
		s2 = ")) {\n"; /*}*/
		}
	nice_printf(outfile, s1);
	expr_out (outfile, index);
	nice_printf (outfile, s2);
	next_tab (outfile);

	for (elts = labels -> listblock.listp; elts; elts = elts -> nextp, i++) {
	    if (elts -> datap) {
		if (ISICON(((expptr) (elts -> datap))))
		    nice_printf (outfile, "case %d:  goto %s;\n", i,
			user_label(((expptr)(elts->datap))->constblock.Const.ci));
		else
		    err ("compgoto_out:  bad label in label list");
	    } /* if (elts -> datap) */
	} /* for elts */
	prev_tab (outfile);
	nice_printf (outfile, /*{*/ "}\n");
    } /* else */
} /* compgoto_out */


void out_for (outfile, init, test, inc)
FILE *outfile;
expptr init, test, inc;
{
    nice_printf (outfile, "for (");
    expr_out (outfile, init);
    nice_printf (outfile, "; ");
    expr_out (outfile, test);
    nice_printf (outfile, "; ");
    expr_out (outfile, inc);
    nice_printf (outfile, ") {\n");
    next_tab (outfile);
} /* out_for */


void out_end_for (outfile)
FILE *outfile;
{
    prev_tab (outfile);
    nice_printf (outfile, "}\n");
} /* out_end_for */
