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

struct reg_data {
    U32 count;
    U8 *what;
    void* data[1];
};

struct reg_substr_datum {
    I32 min_offset;
    I32 max_offset;
    SV *substr;
};

struct reg_substr_data {
    struct reg_substr_datum data[3];	/* Actual array */
};

typedef struct regexp {
	I32 refcnt;
	char **startp;
	char **endp;
	regnode *regstclass;
	I32 minlen;		/* mininum possible length of $& */
	I32 prelen;		/* length of precomp */
	U32 nparens;		/* number of parentheses */
	U32 lastparen;		/* last paren matched */
	char *precomp;		/* pre-compilation regular expression */
	char *subbase;		/* saved string so \digit works forever */
	char *subbeg;		/* same, but not responsible for allocation */
	char *subend;		/* end of subbase */
	U16 naughty;		/* how exponential is this pattern? */
	U16 reganch;		/* Internal use only +
				   Tainted information used by regexec? */
#if 0
        SV *anchored_substr;	/* Substring at fixed position wrt start. */
	I32 anchored_offset;	/* Position of it. */
        SV *float_substr;	/* Substring at variable position wrt start. */
	I32 float_min_offset;	/* Minimal position of it. */
	I32 float_max_offset;	/* Maximal position of it. */
        SV *check_substr;	/* Substring to check before matching. */
        I32 check_offset_min;	/* Offset of the above. */
        I32 check_offset_max;	/* Offset of the above. */
#else
        struct reg_substr_data *substrs;
#endif
        struct reg_data *data;	/* Additional data. */
	regnode program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

#define anchored_substr substrs->data[0].substr
#define anchored_offset substrs->data[0].min_offset
#define float_substr substrs->data[1].substr
#define float_min_offset substrs->data[1].min_offset
#define float_max_offset substrs->data[1].max_offset
#define check_substr substrs->data[2].substr
#define check_offset_min substrs->data[2].min_offset
#define check_offset_max substrs->data[2].max_offset

#define ROPT_ANCH		(ROPT_ANCH_BOL|ROPT_ANCH_MBOL|ROPT_ANCH_GPOS)
#define ROPT_ANCH_SINGLE	(ROPT_ANCH_BOL|ROPT_ANCH_GPOS)
#define ROPT_ANCH_BOL	 	1
#define ROPT_ANCH_MBOL	 	2
#define ROPT_ANCH_GPOS	 	4
#define ROPT_SKIP		8
#define ROPT_IMPLICIT		0x10	/* Converted .* to ^.* */
#define ROPT_NOSCAN		0x20	/* Check-string always at start. */
#define ROPT_GPOS_SEEN		0x40
#define ROPT_CHECK_ALL		0x80
#define ROPT_LOOKBEHIND_SEEN	0x100
#define ROPT_EVAL_SEEN		0x200
#define ROPT_TAINTED_SEEN	0x400
/* 0xf800 of reganch is used by PMf_COMPILETIME */

#define RX_MATCH_TAINTED(prog)	((prog)->reganch & ROPT_TAINTED_SEEN)
#define RX_MATCH_TAINTED_on(prog) ((prog)->reganch |= ROPT_TAINTED_SEEN)
#define RX_MATCH_TAINTED_off(prog) ((prog)->reganch &= ~ROPT_TAINTED_SEEN)
#define RX_MATCH_TAINTED_set(prog, t) ((t) \
				       ? RX_MATCH_TAINTED_on(prog) \
				       : RX_MATCH_TAINTED_off(prog))

#define REXEC_COPY_STR	1		/* Need to copy the string. */
#define REXEC_CHECKED	2		/* check_substr already checked. */

#define ReREFCNT_inc(re) ((re && re->refcnt++), re)
#define ReREFCNT_dec(re) pregfree(re)
