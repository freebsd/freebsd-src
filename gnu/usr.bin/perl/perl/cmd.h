/* $RCSfile: cmd.h,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:35 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: cmd.h,v $
 * Revision 1.1.1.1  1993/08/23  21:29:35  nate
 * PERL!
 *
 * Revision 4.0.1.2  92/06/08  12:01:02  lwall
 * patch20: removed implicit int declarations on funcions
 * 
 * Revision 4.0.1.1  91/06/07  10:28:50  lwall
 * patch4: new copyright notice
 * patch4: length($`), length($&), length($') now optimized to avoid string copy
 * 
 * Revision 4.0  91/03/20  01:04:34  lwall
 * 4.0 baseline.
 * 
 */

#define C_NULL 0
#define C_IF 1
#define C_ELSE 2
#define C_WHILE 3
#define C_BLOCK 4
#define C_EXPR 5
#define C_NEXT 6
#define C_ELSIF 7	/* temporary--turns into an IF + ELSE */
#define C_CSWITCH 8	/* created by switch optimization in block_head() */
#define C_NSWITCH 9	/* likewise */

#ifdef DEBUGGING
#ifndef DOINIT
extern char *cmdname[];
#else
char *cmdname[] = {
    "NULL",
    "IF",
    "ELSE",
    "WHILE",
    "BLOCK",
    "EXPR",
    "NEXT",
    "ELSIF",
    "CSWITCH",
    "NSWITCH",
    "10"
};
#endif
#endif /* DEBUGGING */

#define CF_OPTIMIZE 077	/* type of optimization */
#define CF_FIRSTNEG 0100/* conditional is ($register NE 'string') */
#define CF_NESURE 0200	/* if short doesn't match we're sure */
#define CF_EQSURE 0400	/* if short does match we're sure */
#define CF_COND	01000	/* test c_expr as conditional first, if not null. */
			/* Set for everything except do {} while currently */
#define CF_LOOP 02000	/* loop on the c_expr conditional (loop modifiers) */
#define CF_INVERT 04000	/* it's an "unless" or an "until" */
#define CF_ONCE 010000	/* we've already pushed the label on the stack */
#define CF_FLIP 020000	/* on a match do flipflop */
#define CF_TERM 040000	/* value of this cmd might be returned */
#define CF_DBSUB 0100000 /* this is an inserted cmd for debugging */

#define CFT_FALSE 0	/* c_expr is always false */
#define CFT_TRUE 1	/* c_expr is always true */
#define CFT_REG 2	/* c_expr is a simple register */
#define CFT_ANCHOR 3	/* c_expr is an anchored search /^.../ */
#define CFT_STROP 4	/* c_expr is a string comparison */
#define CFT_SCAN 5	/* c_expr is an unanchored search /.../ */
#define CFT_GETS 6	/* c_expr is <filehandle> */
#define CFT_EVAL 7	/* c_expr is not optimized, so call eval() */
#define CFT_UNFLIP 8	/* 2nd half of range not optimized */
#define CFT_CHOP 9	/* c_expr is a chop on a register */
#define CFT_ARRAY 10	/* this is a foreach loop */
#define CFT_INDGETS 11	/* c_expr is <$variable> */
#define CFT_NUMOP 12	/* c_expr is a numeric comparison */
#define CFT_CCLASS 13	/* c_expr must start with one of these characters */
#define CFT_D0 14	/* no special breakpoint at this line */
#define CFT_D1 15	/* possible special breakpoint at this line */

#ifdef DEBUGGING
#ifndef DOINIT
extern char *cmdopt[];
#else
char *cmdopt[] = {
    "FALSE",
    "TRUE",
    "REG",
    "ANCHOR",
    "STROP",
    "SCAN",
    "GETS",
    "EVAL",
    "UNFLIP",
    "CHOP",
    "ARRAY",
    "INDGETS",
    "NUMOP",
    "CCLASS",
    "14"
};
#endif
#endif /* DEBUGGING */

struct acmd {
    STAB	*ac_stab;	/* a symbol table entry */
    ARG		*ac_expr;	/* any associated expression */
};

struct ccmd {
    CMD		*cc_true;	/* normal code to do on if and while */
    CMD		*cc_alt;	/* else cmd ptr or continue code */
};

struct scmd {
    CMD		**sc_next;	/* array of pointers to commands */
    short	sc_offset;	/* first value - 1 */
    short	sc_max;		/* last value + 1 */
};

struct cmd {
    CMD		*c_next;	/* the next command at this level */
    ARG		*c_expr;	/* conditional expression */
    CMD		*c_head;	/* head of this command list */
    STR		*c_short;	/* string to match as shortcut */
    STAB	*c_stab;	/* a symbol table entry, mostly for fp */
    SPAT	*c_spat;	/* pattern used by optimization */
    char	*c_label;	/* label for this construct */
    union ucmd {
	struct acmd acmd;	/* normal command */
	struct ccmd ccmd;	/* compound command */
	struct scmd scmd;	/* switch command */
    } ucmd;
    short	c_slen;		/* len of c_short, if not null */
    VOLATILE short c_flags;	/* optimization flags--see above */
    HASH	*c_stash;	/* package line was compiled in */
    STAB	*c_filestab;	/* file the following line # is from */
    line_t      c_line;         /* line # of this command */
    char	c_type;		/* what this command does */
};

#define Nullcmd Null(CMD*)
#define Nullcsv Null(CSV*)

EXT CMD * VOLATILE main_root INIT(Nullcmd);
EXT CMD * VOLATILE eval_root INIT(Nullcmd);

EXT CMD compiling;
EXT CMD * VOLATILE curcmd INIT(&compiling);
EXT CSV * VOLATILE curcsv INIT(Nullcsv);

struct callsave {
    SUBR *sub;
    STAB *stab;
    CSV *curcsv;
    CMD *curcmd;
    ARRAY *savearray;
    ARRAY *argarray;
    long depth;
    int wantarray;
    char hasargs;
};

struct compcmd {
    CMD *comp_true;
    CMD *comp_alt;
};

void opt_arg();
ARG* evalstatic();
int cmd_exec();
#ifdef DEBUGGING
void deb();
#endif
int copyopt();
