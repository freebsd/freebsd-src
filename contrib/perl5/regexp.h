/*    regexp.h
 */

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */


struct regnode {
    U8	flags;
    U8  type;
    U16 next_off;
};

typedef struct regnode regnode;

struct reg_substr_data;

struct reg_data;

typedef struct regexp {
	I32 *startp;
	I32 *endp;
	regnode *regstclass;
        struct reg_substr_data *substrs;
	char *precomp;		/* pre-compilation regular expression */
        struct reg_data *data;	/* Additional data. */
	char *subbeg;		/* saved or original string 
				   so \digit works forever. */
	I32 sublen;		/* Length of string pointed by subbeg */
	I32 refcnt;
	I32 minlen;		/* mininum possible length of $& */
	I32 prelen;		/* length of precomp */
	U32 nparens;		/* number of parentheses */
	U32 lastparen;		/* last paren matched */
	U32 reganch;		/* Internal use only +
				   Tainted information used by regexec? */
	regnode program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

#define ROPT_ANCH		(ROPT_ANCH_BOL|ROPT_ANCH_MBOL|ROPT_ANCH_GPOS|ROPT_ANCH_SBOL)
#define ROPT_ANCH_SINGLE	(ROPT_ANCH_SBOL|ROPT_ANCH_GPOS)
#define ROPT_ANCH_BOL	 	0x00001
#define ROPT_ANCH_MBOL	 	0x00002
#define ROPT_ANCH_SBOL	 	0x00004
#define ROPT_ANCH_GPOS	 	0x00008
#define ROPT_SKIP		0x00010
#define ROPT_IMPLICIT		0x00020	/* Converted .* to ^.* */
#define ROPT_NOSCAN		0x00040	/* Check-string always at start. */
#define ROPT_GPOS_SEEN		0x00080
#define ROPT_CHECK_ALL		0x00100
#define ROPT_LOOKBEHIND_SEEN	0x00200
#define ROPT_EVAL_SEEN		0x00400

/* 0xf800 of reganch is used by PMf_COMPILETIME */

#define ROPT_UTF8		0x10000
#define ROPT_NAUGHTY		0x20000 /* how exponential is this pattern? */
#define ROPT_COPY_DONE		0x40000	/* subbeg is a copy of the string */
#define ROPT_TAINTED_SEEN	0x80000

#define RE_USE_INTUIT_NOML	0x0100000 /* Best to intuit before matching */
#define RE_USE_INTUIT_ML	0x0200000
#define REINT_AUTORITATIVE_NOML	0x0400000 /* Can trust a positive answer */
#define REINT_AUTORITATIVE_ML	0x0800000 
#define REINT_ONCE_NOML		0x1000000 /* Intuit can succed once only. */
#define REINT_ONCE_ML		0x2000000
#define RE_INTUIT_ONECHAR	0x4000000
#define RE_INTUIT_TAIL		0x8000000

#define RE_USE_INTUIT		(RE_USE_INTUIT_NOML|RE_USE_INTUIT_ML)
#define REINT_AUTORITATIVE	(REINT_AUTORITATIVE_NOML|REINT_AUTORITATIVE_ML)
#define REINT_ONCE		(REINT_ONCE_NOML|REINT_ONCE_ML)

#define RX_MATCH_TAINTED(prog)	((prog)->reganch & ROPT_TAINTED_SEEN)
#define RX_MATCH_TAINTED_on(prog) ((prog)->reganch |= ROPT_TAINTED_SEEN)
#define RX_MATCH_TAINTED_off(prog) ((prog)->reganch &= ~ROPT_TAINTED_SEEN)
#define RX_MATCH_TAINTED_set(prog, t) ((t) \
				       ? RX_MATCH_TAINTED_on(prog) \
				       : RX_MATCH_TAINTED_off(prog))

#define RX_MATCH_COPIED(prog)		((prog)->reganch & ROPT_COPY_DONE)
#define RX_MATCH_COPIED_on(prog)	((prog)->reganch |= ROPT_COPY_DONE)
#define RX_MATCH_COPIED_off(prog)	((prog)->reganch &= ~ROPT_COPY_DONE)
#define RX_MATCH_COPIED_set(prog,t)	((t) \
					 ? RX_MATCH_COPIED_on(prog) \
					 : RX_MATCH_COPIED_off(prog))

#define REXEC_COPY_STR	0x01		/* Need to copy the string. */
#define REXEC_CHECKED	0x02		/* check_substr already checked. */
#define REXEC_SCREAM	0x04		/* use scream table. */
#define REXEC_IGNOREPOS	0x08		/* \G matches at start. */
#define REXEC_NOT_FIRST	0x10		/* This is another iteration of //g. */
#define REXEC_ML	0x20		/* $* was set. */

#define ReREFCNT_inc(re) ((void)(re && re->refcnt++), re)
#define ReREFCNT_dec(re) CALLREGFREE(aTHX_ re)

#define FBMcf_TAIL_DOLLAR	1
#define FBMcf_TAIL_DOLLARM	2
#define FBMcf_TAIL_Z		4
#define FBMcf_TAIL_z		8
#define FBMcf_TAIL		(FBMcf_TAIL_DOLLAR|FBMcf_TAIL_DOLLARM|FBMcf_TAIL_Z|FBMcf_TAIL_z)

#define FBMrf_MULTILINE	1

struct re_scream_pos_data_s;
