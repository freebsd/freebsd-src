/* $RCSfile: a2p.h,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:53 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: a2p.h,v $
 * Revision 1.1.1.1  1994/09/10  06:27:53  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:30:09  nate
 * PERL!
 *
 * Revision 4.0.1.2  92/06/08  16:12:23  lwall
 * patch20: hash tables now split only if the memory is available to do so
 *
 * Revision 4.0.1.1  91/06/07  12:12:27  lwall
 * patch4: new copyright notice
 *
 * Revision 4.0  91/03/20  01:57:07  lwall
 * 4.0 baseline.
 *
 */

#define VOIDUSED 1
#include "config.h"

#ifndef HAS_BCOPY
#   define bcopy(s1,s2,l) memcpy(s2,s1,l)
#endif
#ifndef HAS_BZERO
#   define bzero(s,l) memset(s,0,l)
#endif

#include "handy.h"
#define Nullop 0

#define OPROG		1
#define OJUNK		2
#define OHUNKS		3
#define ORANGE		4
#define OPAT		5
#define OHUNK		6
#define OPPAREN		7
#define OPANDAND	8
#define OPOROR		9
#define OPNOT		10
#define OCPAREN		11
#define OCANDAND	12
#define OCOROR		13
#define OCNOT		14
#define ORELOP		15
#define ORPAREN		16
#define OMATCHOP	17
#define OMPAREN		18
#define OCONCAT		19
#define OASSIGN		20
#define OADD		21
#define OSUBTRACT	22
#define OMULT		23
#define ODIV		24
#define OMOD		25
#define OPOSTINCR	26
#define OPOSTDECR	27
#define OPREINCR	28
#define OPREDECR	29
#define OUMINUS		30
#define OUPLUS		31
#define OPAREN		32
#define OGETLINE	33
#define OSPRINTF	34
#define OSUBSTR		35
#define OSTRING		36
#define OSPLIT		37
#define OSNEWLINE	38
#define OINDEX		39
#define ONUM		40
#define OSTR		41
#define OVAR		42
#define OFLD		43
#define ONEWLINE	44
#define OCOMMENT	45
#define OCOMMA		46
#define OSEMICOLON	47
#define OSCOMMENT	48
#define OSTATES		49
#define OSTATE		50
#define OPRINT		51
#define OPRINTF		52
#define OBREAK		53
#define ONEXT		54
#define OEXIT		55
#define OCONTINUE	56
#define OREDIR		57
#define OIF		58
#define OWHILE		59
#define OFOR		60
#define OFORIN		61
#define OVFLD		62
#define OBLOCK		63
#define OREGEX		64
#define OLENGTH		65
#define OLOG		66
#define OEXP		67
#define OSQRT		68
#define OINT		69
#define ODO		70
#define OPOW		71
#define OSUB		72
#define OGSUB		73
#define OMATCH		74
#define OUSERFUN	75
#define OUSERDEF	76
#define OCLOSE		77
#define OATAN2		78
#define OSIN		79
#define OCOS		80
#define ORAND		81
#define OSRAND		82
#define ODELETE		83
#define OSYSTEM		84
#define OCOND		85
#define ORETURN		86
#define ODEFINED	87
#define OSTAR		88

#ifdef DOINIT
char *opname[] = {
    "0",
    "PROG",
    "JUNK",
    "HUNKS",
    "RANGE",
    "PAT",
    "HUNK",
    "PPAREN",
    "PANDAND",
    "POROR",
    "PNOT",
    "CPAREN",
    "CANDAND",
    "COROR",
    "CNOT",
    "RELOP",
    "RPAREN",
    "MATCHOP",
    "MPAREN",
    "CONCAT",
    "ASSIGN",
    "ADD",
    "SUBTRACT",
    "MULT",
    "DIV",
    "MOD",
    "POSTINCR",
    "POSTDECR",
    "PREINCR",
    "PREDECR",
    "UMINUS",
    "UPLUS",
    "PAREN",
    "GETLINE",
    "SPRINTF",
    "SUBSTR",
    "STRING",
    "SPLIT",
    "SNEWLINE",
    "INDEX",
    "NUM",
    "STR",
    "VAR",
    "FLD",
    "NEWLINE",
    "COMMENT",
    "COMMA",
    "SEMICOLON",
    "SCOMMENT",
    "STATES",
    "STATE",
    "PRINT",
    "PRINTF",
    "BREAK",
    "NEXT",
    "EXIT",
    "CONTINUE",
    "REDIR",
    "IF",
    "WHILE",
    "FOR",
    "FORIN",
    "VFLD",
    "BLOCK",
    "REGEX",
    "LENGTH",
    "LOG",
    "EXP",
    "SQRT",
    "INT",
    "DO",
    "POW",
    "SUB",
    "GSUB",
    "MATCH",
    "USERFUN",
    "USERDEF",
    "CLOSE",
    "ATAN2",
    "SIN",
    "COS",
    "RAND",
    "SRAND",
    "DELETE",
    "SYSTEM",
    "COND",
    "RETURN",
    "DEFINED",
    "STAR",
    "89"
};
#else
extern char *opname[];
#endif

EXT int mop INIT(1);

union u_ops {
    int ival;
    char *cval;
};
#if defined(iAPX286) || defined(M_I286) || defined(I80286) 	/* 80286 hack */
#define OPSMAX (64000/sizeof(union u_ops))	/* approx. max segment size */
#else
#define OPSMAX 50000
#endif						 	/* 80286 hack */
union u_ops ops[OPSMAX];

#include <stdio.h>
#include <ctype.h>

typedef struct string STR;
typedef struct htbl HASH;

#include "str.h"
#include "hash.h"

/* A string is TRUE if not "" or "0". */
#define True(val) (tmps = (val), (*tmps && !(*tmps == '0' && !tmps[1])))
EXT char *Yes INIT("1");
EXT char *No INIT("");

#define str_true(str) (Str = (str), (Str->str_pok ? True(Str->str_ptr) : (Str->str_nok ? (Str->str_nval != 0.0) : 0 )))

#define str_peek(str) (Str = (str), (Str->str_pok ? Str->str_ptr : (Str->str_nok ? (sprintf(buf,"num(%g)",Str->str_nval),buf) : "" )))
#define str_get(str) (Str = (str), (Str->str_pok ? Str->str_ptr : str_2ptr(Str)))
#define str_gnum(str) (Str = (str), (Str->str_nok ? Str->str_nval : str_2num(Str)))
EXT STR *Str;

#define GROWSTR(pp,lp,len) if (*(lp) < (len)) growstr(pp,lp,len)

STR *str_new();

char *scanpat();
char *scannum();

void str_free();

EXT int line INIT(0);

EXT FILE *rsfp;
EXT char buf[2048];
EXT char *bufptr INIT(buf);

EXT STR *linestr INIT(Nullstr);

EXT char tokenbuf[2048];
EXT int expectterm INIT(TRUE);

#ifdef DEBUGGING
EXT int debug INIT(0);
EXT int dlevel INIT(0);
#define YYDEBUG 1
extern int yydebug;
#endif

EXT STR *freestrroot INIT(Nullstr);

EXT STR str_no;
EXT STR str_yes;

EXT bool do_split INIT(FALSE);
EXT bool split_to_array INIT(FALSE);
EXT bool set_array_base INIT(FALSE);
EXT bool saw_RS INIT(FALSE);
EXT bool saw_OFS INIT(FALSE);
EXT bool saw_ORS INIT(FALSE);
EXT bool saw_line_op INIT(FALSE);
EXT bool in_begin INIT(TRUE);
EXT bool do_opens INIT(FALSE);
EXT bool do_fancy_opens INIT(FALSE);
EXT bool lval_field INIT(FALSE);
EXT bool do_chop INIT(FALSE);
EXT bool need_entire INIT(FALSE);
EXT bool absmaxfld INIT(FALSE);
EXT bool saw_altinput INIT(FALSE);

EXT bool nomemok INIT(FALSE);

EXT char const_FS INIT(0);
EXT char *namelist INIT(Nullch);
EXT char fswitch INIT(0);

EXT int saw_FS INIT(0);
EXT int maxfld INIT(0);
EXT int arymax INIT(0);
char *nameary[100];

EXT STR *opens;

EXT HASH *symtab;
EXT HASH *curarghash;

#define P_MIN		0
#define P_LISTOP	5
#define P_COMMA		10
#define P_ASSIGN	15
#define P_COND		20
#define P_DOTDOT	25
#define P_OROR		30
#define P_ANDAND	35
#define P_OR		40
#define P_AND		45
#define P_EQ		50
#define P_REL		55
#define P_UNI		60
#define P_FILETEST	65
#define P_SHIFT		70
#define P_ADD		75
#define P_MUL		80
#define P_MATCH		85
#define P_UNARY		90
#define P_POW		95
#define P_AUTO		100
#define P_MAX		999
