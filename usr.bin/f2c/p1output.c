/****************************************************************
Copyright 1990, 1991, 1993 by AT&T Bell Laboratories and Bellcore.

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
#include "p1defs.h"
#include "output.h"
#include "names.h"


static void p1_addr(), p1_big_addr(), p1_binary(), p1_const(), p1_list(),
	p1_literal(), p1_name(), p1_unary(), p1putn();
static void p1putd (/* int, int */);
static void p1putds (/* int, int, char * */);
static void p1putdds (/* int, int, int, char * */);
static void p1putdd (/* int, int, int */);
static void p1putddd (/* int, int, int, int */);


/* p1_comment -- save the text of a Fortran comment in the intermediate
   file.  Make sure that there are no spurious "/ *" or "* /" characters by
   mapping them onto "/+" and "+/".   str   is assumed to hold no newlines and be
   null terminated; it may be modified by this function. */

void p1_comment (str)
char *str;
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

static void p1_name (namep)
Namep namep;
{
	p1putd (P1_NAME_POINTER, (long) namep);
	namep->visused = 1;
} /* p1_name */



void p1_expr (expr)
expptr expr;
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



static void p1_const(cp)
 register Constp cp;
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


void p1_asgoto (addrp)
Addrp addrp;
{
    p1put (P1_ASGOTO);
    p1_addr (addrp);
} /* p1_asgoto */


void p1_goto (stateno)
ftnint stateno;
{
    p1putd (P1_GOTO, stateno);
} /* p1_goto */


static void p1_addr (addrp)
 register struct Addrblock *addrp;
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


static void p1_list (listp)
struct Listblock *listp;
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


void p1_label (lab)
long lab;
{
	if (parstate < INDATA)
		earlylabs = mkchain((char *)lab, earlylabs);
	else
		p1putd (P1_LABEL, lab);
	}



static void p1_literal (memno)
long memno;
{
    p1putd (P1_LITERAL, memno);
} /* p1_literal */


void p1_if (expr)
expptr expr;
{
    p1put (P1_IF);
    p1_expr (expr);
} /* p1_if */




void p1_elif (expr)
expptr expr;
{
    p1put (P1_ELIF);
    p1_expr (expr);
} /* p1_elif */




void p1_else ()
{
    p1put (P1_ELSE);
} /* p1_else */




void p1_endif ()
{
    p1put (P1_ENDIF);
} /* p1_endif */




void p1else_end ()
{
    p1put (P1_ENDELSE);
} /* p1else_end */


static void p1_big_addr (addrp)
Addrp addrp;
{
    if (addrp == (Addrp) NULL)
	return;

    p1putn (P1_ADDR, (int)sizeof(struct Addrblock), (char *) addrp);
    p1_expr (addrp -> vleng);
    p1_expr (addrp -> memoffset);
    if (addrp->uname_tag == UNAM_NAME)
	addrp->user.name->visused = 1;
} /* p1_big_addr */



static void p1_unary (e)
struct Exprblock *e;
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


static void p1_binary (e)
struct Exprblock *e;
{
    if (e == (struct Exprblock *) NULL)
	return;

    p1putdd (P1_EXPR, e -> opcode, e -> vtype);
    p1_expr (e -> vleng);
    p1_expr (e -> leftp);
    p1_expr (e -> rightp);
} /* p1_binary */


void p1_head (class, name)
int class;
char *name;
{
    p1putds (P1_HEAD, class, name ? name : "");
} /* p1_head */


void p1_subr_ret (retexp)
expptr retexp;
{

    p1put (P1_SUBR_RET);
    p1_expr (cpexpr(retexp));
} /* p1_subr_ret */



void p1comp_goto (index, count, labels)
expptr index;
int count;
struct Labelblock *labels[];
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



void p1_for (init, test, inc)
expptr init, test, inc;
{
    p1put (P1_FOR);
    p1_expr (init);
    p1_expr (test);
    p1_expr (inc);
} /* p1_for */


void p1for_end ()
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

void p1puts (type, str)
int type;
char *str;
{
    fprintf (pass1_file, "%d: %s\n", type, str);
} /* p1puts */


/* p1putd -- Put a typed integer into the Pass 1 intermediate file. */

static void p1putd (type, value)
int type;
long value;
{
    fprintf (pass1_file, "%d: %ld\n", type, value);
} /* p1_putd */


/* p1putdd -- Put a typed pair of integers into the intermediate file. */

static void p1putdd (type, v1, v2)
int type, v1, v2;
{
    fprintf (pass1_file, "%d: %d %d\n", type, v1, v2);
} /* p1putdd */


/* p1putddd -- Put a typed triple of integers into the intermediate file. */

static void p1putddd (type, v1, v2, v3)
int type, v1, v2, v3;
{
    fprintf (pass1_file, "%d: %d %d %d\n", type, v1, v2, v3);
} /* p1putddd */

 union dL {
	double d;
	long L[2];
	};

static void p1putn (type, count, str)
int type, count;
char *str;
{
    int i;

    fprintf (pass1_file, "%d: ", type);

    for (i = 0; i < count; i++)
	putc (str[i], pass1_file);

    putc ('\n', pass1_file);
} /* p1putn */



/* p1put -- Put a type marker into the intermediate file. */

void p1put(type)
int type;
{
    fprintf (pass1_file, "%d:\n", type);
} /* p1put */



static void p1putds (type, i, str)
int type;
int i;
char *str;
{
    fprintf (pass1_file, "%d: %d %s\n", type, i, str);
} /* p1putds */


static void p1putdds (token, type, stg, str)
int token, type, stg;
char *str;
{
    fprintf (pass1_file, "%d: %d %d %s\n", token, type, stg, str);
} /* p1putdds */
