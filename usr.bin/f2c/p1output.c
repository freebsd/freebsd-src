/****************************************************************
Copyright 1990, 1991, 1993, 1994 by AT&T, Lucent Technologies and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T, Bell Laboratories,
Lucent or Bellcore or any of their entities not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

AT&T, Lucent and Bellcore disclaim all warranties with regard to
this software, including all implied warranties of
merchantability and fitness.  In no event shall AT&T, Lucent or
Bellcore be liable for any special, indirect or consequential
damages or any damages whatsoever resulting from loss of use,
data or profits, whether in an action of contract, negligence or
other tortious action, arising out of or in connection with the
use or performance of this software.
****************************************************************/

#include "defs.h"
#include "p1defs.h"
#include "output.h"
#include "names.h"


static void p1_addr Argdcl((Addrp));
static void p1_big_addr Argdcl((Addrp));
static void p1_binary Argdcl((Exprp));
static void p1_const Argdcl((Constp));
static void p1_list Argdcl((struct Listblock*));
static void p1_literal Argdcl((long int));
static void p1_name Argdcl((Namep));
static void p1_unary Argdcl((Exprp));
static void p1putd Argdcl((int, long int));
static void p1putdd Argdcl((int, int, int));
static void p1putddd Argdcl((int, int, int, int));
static void p1putdds Argdcl((int, int, int, char*));
static void p1putds Argdcl((int, int, char*));
static void p1putn Argdcl((int, int, char*));


/* p1_comment -- save the text of a Fortran comment in the intermediate
   file.  Make sure that there are no spurious "/ *" or "* /" characters by
   mapping them onto "/+" and "+/".   str   is assumed to hold no newlines and be
   null terminated; it may be modified by this function. */

 void
#ifdef KR_headers
p1_comment(str)
	char *str;
#else
p1_comment(char *str)
#endif
{
    register unsigned char *pointer, *ustr;

    if (!str)
	return;

/* Get rid of any open or close comment combinations that may be in the
   Fortran input */

	ustr = (unsigned char *)str;
	for(pointer = ustr; *pointer; pointer++)
		if (*pointer == '*' && (pointer[1] == '/'
					|| pointer > ustr && pointer[-1] == '/'))
			*pointer = '+';
	/* trim trailing white space */
#ifdef isascii
	while(--pointer >= ustr && (!isascii(*pointer) || isspace(*pointer)));
#else
	while(--pointer >= ustr && isspace(*pointer));
#endif
	pointer[1] = 0;
	p1puts (P1_COMMENT, str);
} /* p1_comment */

/* p1_name -- Writes the address of a hash table entry into the
   intermediate file */

 static void
#ifdef KR_headers
p1_name(namep)
	Namep namep;
#else
p1_name(Namep namep)
#endif
{
	p1putd (P1_NAME_POINTER, (long) namep);
	namep->visused = 1;
} /* p1_name */



 void
#ifdef KR_headers
p1_expr(expr)
	expptr expr;
#else
p1_expr(expptr expr)
#endif
{
/* An opcode of 0 means a null entry */

    if (expr == ENULL) {
	p1putdd (P1_EXPR, 0, TYUNKNOWN);	/* Should this be TYERROR? */
	return;
    } /* if (expr == ENULL) */

    switch (expr -> tag) {
        case TNAME:
		p1_name ((Namep) expr);
		return;
	case TCONST:
		p1_const(&expr->constblock);
		return;
	case TEXPR:
		/* Fall through the switch */
		break;
	case TADDR:
		p1_addr (&(expr -> addrblock));
		goto freeup;
	case TPRIM:
		warn ("p1_expr:  got TPRIM");
		return;
	case TLIST:
		p1_list (&(expr->listblock));
		frchain( &(expr->listblock.listp) );
		return;
	case TERROR:
		return;
	default:
		erri ("p1_expr: bad tag '%d'", (int) (expr -> tag));
		return;
	}

/* Now we know that the tag is TEXPR */

    if (is_unary_op (expr -> exprblock.opcode))
	p1_unary (&(expr -> exprblock));
    else if (is_binary_op (expr -> exprblock.opcode))
	p1_binary (&(expr -> exprblock));
    else
	erri ("p1_expr:  bad opcode '%d'", (int) expr -> exprblock.opcode);
 freeup:
    free((char *)expr);

} /* p1_expr */



 static void
#ifdef KR_headers
p1_const(cp)
	register Constp cp;
#else
p1_const(register Constp cp)
#endif
{
	int type = cp->vtype;
	expptr vleng = cp->vleng;
	union Constant *c = &cp->Const;
	char cdsbuf0[64], cdsbuf1[64];
	char *cds0, *cds1;

    switch (type) {
	case TYINT1:
        case TYSHORT:
	case TYLONG:
#ifdef TYQUAD
	case TYQUAD:
#endif
	case TYLOGICAL:
	case TYLOGICAL1:
	case TYLOGICAL2:
	    fprintf(pass1_file, "%d: %d %ld\n", P1_CONST, type, c->ci);
	    break;
	case TYREAL:
	case TYDREAL:
		fprintf(pass1_file, "%d: %d %s\n", P1_CONST, type,
			cp->vstg ? c->cds[0] : cds(dtos(c->cd[0]), cdsbuf0));
	    break;
	case TYCOMPLEX:
	case TYDCOMPLEX:
		if (cp->vstg) {
			cds0 = c->cds[0];
			cds1 = c->cds[1];
			}
		else {
			cds0 = cds(dtos(c->cd[0]), cdsbuf0);
			cds1 = cds(dtos(c->cd[1]), cdsbuf1);
			}
		fprintf(pass1_file, "%d: %d %s %s\n", P1_CONST, type,
			cds0, cds1);
	    break;
	case TYCHAR:
	    if (vleng && !ISICON (vleng))
		erri("p1_const:  bad vleng '%d'\n", (int) vleng);
	    else
		fprintf(pass1_file, "%d: %d %lx\n", P1_CONST, type,
			cpexpr((expptr)cp));
	    break;
	default:
	    erri ("p1_const:  bad constant type '%d'", type);
	    break;
    } /* switch */
} /* p1_const */


 void
#ifdef KR_headers
p1_asgoto(addrp)
	Addrp addrp;
#else
p1_asgoto(Addrp addrp)
#endif
{
    p1put (P1_ASGOTO);
    p1_addr (addrp);
} /* p1_asgoto */


 void
#ifdef KR_headers
p1_goto(stateno)
	ftnint stateno;
#else
p1_goto(ftnint stateno)
#endif
{
    p1putd (P1_GOTO, stateno);
} /* p1_goto */


 static void
#ifdef KR_headers
p1_addr(addrp)
	register struct Addrblock *addrp;
#else
p1_addr(register struct Addrblock *addrp)
#endif
{
    int stg;

    if (addrp == (struct Addrblock *) NULL)
	return;

    stg = addrp -> vstg;

    if (ONEOF(stg, M(STGINIT)|M(STGREG))
	|| ONEOF(stg, M(STGCOMMON)|M(STGEQUIV)) &&
		(!ISICON(addrp->memoffset)
		|| (addrp->uname_tag == UNAM_NAME
			? addrp->memoffset->constblock.Const.ci
				!= addrp->user.name->voffset
			: addrp->memoffset->constblock.Const.ci))
	|| ONEOF(stg, M(STGBSS)|M(STGINIT)|M(STGAUTO)|M(STGARG)) &&
		(!ISICON(addrp->memoffset)
			|| addrp->memoffset->constblock.Const.ci)
	|| addrp->Field || addrp->isarray || addrp->vstg == STGLENG)
	{
		p1_big_addr (addrp);
		return;
	}

/* Write out a level of indirection for non-array arguments, which have
   addrp -> memoffset   set and are handled by   p1_big_addr().
   Lengths are passed by value, so don't check STGLENG
   28-Jun-89 (dmg)  Added the check for != TYCHAR
 */

    if (oneof_stg ( addrp -> uname_tag == UNAM_NAME ? addrp -> user.name : NULL,
	    stg, M(STGARG)|M(STGEQUIV)) && addrp->vtype != TYCHAR) {
	p1putdd (P1_EXPR, OPWHATSIN, addrp -> vtype);
	p1_expr (ENULL);	/* Put dummy   vleng   */
    } /* if stg == STGARG */

    switch (addrp -> uname_tag) {
        case UNAM_NAME:
	    p1_name (addrp -> user.name);
	    break;
	case UNAM_IDENT:
	    p1putdds(P1_IDENT, addrp->vtype, addrp->vstg,
				addrp->user.ident);
	    break;
	case UNAM_CHARP:
		p1putdds(P1_CHARP, addrp->vtype, addrp->vstg,
				addrp->user.Charp);
		break;
	case UNAM_EXTERN:
	    p1putd (P1_EXTERN, (long) addrp -> memno);
	    if (addrp->vclass == CLPROC)
		extsymtab[addrp->memno].extype = addrp->vtype;
	    break;
	case UNAM_CONST:
	    if (addrp -> memno != BAD_MEMNO)
		p1_literal (addrp -> memno);
	    else
		p1_const((struct Constblock *)addrp);
	    break;
	case UNAM_UNKNOWN:
	default:
	    erri ("p1_addr:  unknown uname_tag '%d'", addrp -> uname_tag);
	    break;
    } /* switch */
} /* p1_addr */


 static void
#ifdef KR_headers
p1_list(listp)
	struct Listblock *listp;
#else
p1_list(struct Listblock *listp)
#endif
{
    chainp lis;
    int count = 0;

    if (listp == (struct Listblock *) NULL)
	return;

/* Count the number of parameters in the list */

    for (lis = listp -> listp; lis; lis = lis -> nextp)
	count++;

    p1putddd (P1_LIST, listp -> tag, listp -> vtype, count);

    for (lis = listp -> listp; lis; lis = lis -> nextp)
	p1_expr ((expptr) lis -> datap);

} /* p1_list */


 void
#ifdef KR_headers
p1_label(lab)
	long lab;
#else
p1_label(long lab)
#endif
{
	if (parstate < INDATA)
		earlylabs = mkchain((char *)lab, earlylabs);
	else
		p1putd (P1_LABEL, lab);
	}



 static void
#ifdef KR_headers
p1_literal(memno)
	long memno;
#else
p1_literal(long memno)
#endif
{
    p1putd (P1_LITERAL, memno);
} /* p1_literal */


 void
#ifdef KR_headers
p1_if(expr)
	expptr expr;
#else
p1_if(expptr expr)
#endif
{
    p1put (P1_IF);
    p1_expr (expr);
} /* p1_if */




 void
#ifdef KR_headers
p1_elif(expr)
	expptr expr;
#else
p1_elif(expptr expr)
#endif
{
    p1put (P1_ELIF);
    p1_expr (expr);
} /* p1_elif */




 void
p1_else(Void)
{
    p1put (P1_ELSE);
} /* p1_else */




 void
p1_endif(Void)
{
    p1put (P1_ENDIF);
} /* p1_endif */




 void
p1else_end(Void)
{
    p1put (P1_ENDELSE);
} /* p1else_end */


 static void
#ifdef KR_headers
p1_big_addr(addrp)
	Addrp addrp;
#else
p1_big_addr(Addrp addrp)
#endif
{
    if (addrp == (Addrp) NULL)
	return;

    p1putn (P1_ADDR, (int)sizeof(struct Addrblock), (char *) addrp);
    p1_expr (addrp -> vleng);
    p1_expr (addrp -> memoffset);
    if (addrp->uname_tag == UNAM_NAME)
	addrp->user.name->visused = 1;
} /* p1_big_addr */



 static void
#ifdef KR_headers
p1_unary(e)
	struct Exprblock *e;
#else
p1_unary(struct Exprblock *e)
#endif
{
    if (e == (struct Exprblock *) NULL)
	return;

    p1putdd (P1_EXPR, (int) e -> opcode, e -> vtype);
    p1_expr (e -> vleng);

    switch (e -> opcode) {
        case OPNEG:
	case OPNEG1:
	case OPNOT:
	case OPABS:
	case OPBITNOT:
	case OPPREINC:
	case OPPREDEC:
	case OPADDR:
	case OPIDENTITY:
	case OPCHARCAST:
	case OPDABS:
	    p1_expr(e -> leftp);
	    break;
	default:
	    erri ("p1_unary: bad opcode '%d'", (int) e -> opcode);
	    break;
    } /* switch */

} /* p1_unary */


 static void
#ifdef KR_headers
p1_binary(e)
	struct Exprblock *e;
#else
p1_binary(struct Exprblock *e)
#endif
{
    if (e == (struct Exprblock *) NULL)
	return;

    p1putdd (P1_EXPR, e -> opcode, e -> vtype);
    p1_expr (e -> vleng);
    p1_expr (e -> leftp);
    p1_expr (e -> rightp);
} /* p1_binary */


 void
#ifdef KR_headers
p1_head(class, name)
	int class;
	char *name;
#else
p1_head(int class, char *name)
#endif
{
    p1putds (P1_HEAD, class, name ? name : "");
} /* p1_head */


 void
#ifdef KR_headers
p1_subr_ret(retexp)
	expptr retexp;
#else
p1_subr_ret(expptr retexp)
#endif
{

    p1put (P1_SUBR_RET);
    p1_expr (cpexpr(retexp));
} /* p1_subr_ret */



 void
#ifdef KR_headers
p1comp_goto(index, count, labels)
	expptr index;
	int count;
	struct Labelblock **labels;
#else
p1comp_goto(expptr index, int count, struct Labelblock **labels)
#endif
{
    struct Constblock c;
    int i;
    register struct Labelblock *L;

    p1put (P1_COMP_GOTO);
    p1_expr (index);

/* Write out a P1_LIST directly, to avoid the overhead of allocating a
   list before it's needed HACK HACK HACK */

    p1putddd (P1_LIST, TLIST, TYUNKNOWN, count);
    c.vtype = TYLONG;
    c.vleng = 0;

    for (i = 0; i < count; i++) {
	L = labels[i];
	L->labused = 1;
	c.Const.ci = L->stateno;
	p1_const(&c);
    } /* for i = 0 */
} /* p1comp_goto */



 void
#ifdef KR_headers
p1_for(init, test, inc)
	expptr init;
	expptr test;
	expptr inc;
#else
p1_for(expptr init, expptr test, expptr inc)
#endif
{
    p1put (P1_FOR);
    p1_expr (init);
    p1_expr (test);
    p1_expr (inc);
} /* p1_for */


 void
p1for_end(Void)
{
    p1put (P1_ENDFOR);
} /* p1for_end */




/* ----------------------------------------------------------------------
   The intermediate file actually gets written ONLY by the routines below.
   To change the format of the file, you need only change these routines.
   ----------------------------------------------------------------------
*/


/* p1puts -- Put a typed string into the Pass 1 intermediate file.  Assumes that
   str   contains no newlines and is null-terminated. */

 void
#ifdef KR_headers
p1puts(type, str)
	int type;
	char *str;
#else
p1puts(int type, char *str)
#endif
{
    fprintf (pass1_file, "%d: %s\n", type, str);
} /* p1puts */


/* p1putd -- Put a typed integer into the Pass 1 intermediate file. */

 static void
#ifdef KR_headers
p1putd(type, value)
	int type;
	long value;
#else
p1putd(int type, long value)
#endif
{
    fprintf (pass1_file, "%d: %ld\n", type, value);
} /* p1_putd */


/* p1putdd -- Put a typed pair of integers into the intermediate file. */

 static void
#ifdef KR_headers
p1putdd(type, v1, v2)
	int type;
	int v1;
	int v2;
#else
p1putdd(int type, int v1, int v2)
#endif
{
    fprintf (pass1_file, "%d: %d %d\n", type, v1, v2);
} /* p1putdd */


/* p1putddd -- Put a typed triple of integers into the intermediate file. */

 static void
#ifdef KR_headers
p1putddd(type, v1, v2, v3)
	int type;
	int v1;
	int v2;
	int v3;
#else
p1putddd(int type, int v1, int v2, int v3)
#endif
{
    fprintf (pass1_file, "%d: %d %d %d\n", type, v1, v2, v3);
} /* p1putddd */

 union dL {
	double d;
	long L[2];
	};

 static void
#ifdef KR_headers
p1putn(type, count, str)
	int type;
	int count;
	char *str;
#else
p1putn(int type, int count, char *str)
#endif
{
    int i;

    fprintf (pass1_file, "%d: ", type);

    for (i = 0; i < count; i++)
	putc (str[i], pass1_file);

    putc ('\n', pass1_file);
} /* p1putn */



/* p1put -- Put a type marker into the intermediate file. */

 void
#ifdef KR_headers
p1put(type)
	int type;
#else
p1put(int type)
#endif
{
    fprintf (pass1_file, "%d:\n", type);
} /* p1put */



 static void
#ifdef KR_headers
p1putds(type, i, str)
	int type;
	int i;
	char *str;
#else
p1putds(int type, int i, char *str)
#endif
{
    fprintf (pass1_file, "%d: %d %s\n", type, i, str);
} /* p1putds */


 static void
#ifdef KR_headers
p1putdds(token, type, stg, str)
	int token;
	int type;
	int stg;
	char *str;
#else
p1putdds(int token, int type, int stg, char *str)
#endif
{
    fprintf (pass1_file, "%d: %d %d %s\n", token, type, stg, str);
} /* p1putdds */
