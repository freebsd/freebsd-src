/*
 * awk.h -- Definitions for gawk. 
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* ------------------------------ Includes ------------------------------ */
#include "config.h"

#include <stdio.h>
#ifndef LIMITS_H_MISSING
#include <limits.h>
#endif
#include <ctype.h>
#include <setjmp.h>
#include <varargs.h>
#include <time.h>
#include <errno.h>
#if !defined(errno) && !defined(MSDOS) && !defined(OS2)
extern int errno;
#endif
#ifdef __GNU_LIBRARY__
#ifndef linux
#include <signum.h>
#endif
#endif

/* ----------------- System dependencies (with more includes) -----------*/

#if defined(__FreeBSD__)
# include <floatingpoint.h>
#endif

#if !defined(VMS) || (!defined(VAXC) && !defined(__DECC))
#include <sys/types.h>
#include <sys/stat.h>
#else	/* VMS w/ VAXC or DECC */
#include <types.h>
#include <stat.h>
#include <file.h>	/* avoid <fcntl.h> in io.c */
#endif

#include <signal.h>

#ifdef __STDC__
#define	P(s)	s
#define MALLOC_ARG_T size_t
#else
#define	P(s)	()
#define MALLOC_ARG_T unsigned
#define volatile
#define const
#endif

#ifndef SIGTYPE
#define SIGTYPE	void
#endif

#ifdef SIZE_T_MISSING
typedef unsigned int size_t;
#endif

#ifndef SZTC
#define SZTC
#define INTC
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#ifdef NeXT
#include <libc.h>
#undef atof
#else
#if defined(atarist) || defined(VMS)
#include <unixlib.h>
#else	/* atarist || VMS */
#if !defined(MSDOS) && !defined(_MSC_VER)
#include <unistd.h>
#endif	/* MSDOS */
#endif	/* atarist || VMS */
#endif	/* Next */
#else	/* STDC_HEADERS */
#include "protos.h"
#endif	/* STDC_HEADERS */

#if defined(ultrix) && !defined(Ultrix41)
extern char * getenv P((char *name));
extern double atof P((char *s));
#endif

#ifndef __GNUC__
#ifdef sparc
/* nasty nasty SunOS-ism */
#include <alloca.h>
#ifdef lint
extern char *alloca();
#endif
#else /* not sparc */
#if !defined(alloca) && !defined(ALLOCA_PROTO)
#if defined(_MSC_VER)
#include <malloc.h>
#else
extern char *alloca();
#endif /* _MSC_VER */
#endif
#endif /* sparc */
#endif /* __GNUC__ */

#ifdef HAVE_UNDERSCORE_SETJMP
/* nasty nasty berkelixm */
#define setjmp	_setjmp
#define longjmp	_longjmp
#endif

/*
 * if you don't have vprintf, try this and cross your fingers.
 */
#if defined(VPRINTF_MISSING)
#define vfprintf(fp,fmt,arg)	_doprnt((fmt), (arg), (fp))
#endif

#ifdef VMS
/* some macros to redirect to code in vms/vms_misc.c */
#define exit		vms_exit
#define open		vms_open
#define strerror	vms_strerror
#define strdup		vms_strdup
extern void  exit P((int));
extern int   open P((const char *,int,...));
extern char *strerror P((int));
extern char *strdup P((const char *str));
extern int   vms_devopen P((const char *,int));
# ifndef NO_TTY_FWRITE
#define fwrite		tty_fwrite
#define fclose		tty_fclose
extern size_t fwrite P((const void *,size_t,size_t,FILE *));
extern int    fclose P((FILE *));
# endif
extern FILE *popen P((const char *,const char *));
extern int   pclose P((FILE *));
extern void vms_arg_fixup P((int *,char ***));
/* some things not in STDC_HEADERS */
extern size_t gnu_strftime P((char *,size_t,const char *,const struct tm *));
extern int unlink P((const char *));
extern int getopt P((int,char **,char *));
extern int isatty P((int));
#ifndef fileno
extern int fileno P((FILE *));
#endif
extern int close(), dup(), dup2(), fstat(), read(), stat();
extern int getpgrp P((void));
#endif  /*VMS*/

#define	GNU_REGEX
#ifdef GNU_REGEX
#include "regex.h"
#include "dfa.h"
typedef struct Regexp {
	struct re_pattern_buffer pat;
	struct re_registers regs;
	struct dfa dfareg;
	int dfa;
} Regexp;
#define	RESTART(rp,s)	(rp)->regs.start[0]
#define	REEND(rp,s)	(rp)->regs.end[0]
#else	/* GNU_REGEX */
#endif	/* GNU_REGEX */

#ifdef atarist
#define read _text_read /* we do not want all these CR's to mess our input */
extern int _text_read (int, char *, int);
#ifndef __MINT__
#undef NGROUPS_MAX
#endif /* __MINT__ */
#endif

#ifndef DEFPATH
#define DEFPATH	".:/usr/local/lib/awk:/usr/lib/awk"
#endif

#ifndef ENVSEP
#define ENVSEP	':'
#endif

extern double double_to_int P((double d));

/* ------------------ Constants, Structures, Typedefs  ------------------ */
#define AWKNUM	double

typedef enum {
	/* illegal entry == 0 */
	Node_illegal,

	/* binary operators  lnode and rnode are the expressions to work on */
	Node_times,
	Node_quotient,
	Node_mod,
	Node_plus,
	Node_minus,
	Node_cond_pair,		/* conditional pair (see Node_line_range) */
	Node_subscript,
	Node_concat,
	Node_exp,

	/* unary operators   subnode is the expression to work on */
/*10*/	Node_preincrement,
	Node_predecrement,
	Node_postincrement,
	Node_postdecrement,
	Node_unary_minus,
	Node_field_spec,

	/* assignments   lnode is the var to assign to, rnode is the exp */
	Node_assign,
	Node_assign_times,
	Node_assign_quotient,
	Node_assign_mod,
/*20*/	Node_assign_plus,
	Node_assign_minus,
	Node_assign_exp,

	/* boolean binaries   lnode and rnode are expressions */
	Node_and,
	Node_or,

	/* binary relationals   compares lnode and rnode */
	Node_equal,
	Node_notequal,
	Node_less,
	Node_greater,
	Node_leq,
/*30*/	Node_geq,
	Node_match,
	Node_nomatch,

	/* unary relationals   works on subnode */
	Node_not,

	/* program structures */
	Node_rule_list,		/* lnode is a rule, rnode is rest of list */
	Node_rule_node,		/* lnode is pattern, rnode is statement */
	Node_statement_list,	/* lnode is statement, rnode is more list */
	Node_if_branches,	/* lnode is to run on true, rnode on false */
	Node_expression_list,	/* lnode is an exp, rnode is more list */
	Node_param_list,	/* lnode is a variable, rnode is more list */

	/* keywords */
/*40*/	Node_K_if,		/* lnode is conditonal, rnode is if_branches */
	Node_K_while,		/* lnode is condtional, rnode is stuff to run */
	Node_K_for,		/* lnode is for_struct, rnode is stuff to run */
	Node_K_arrayfor,	/* lnode is for_struct, rnode is stuff to run */
	Node_K_break,		/* no subs */
	Node_K_continue,	/* no stuff */
	Node_K_print,		/* lnode is exp_list, rnode is redirect */
	Node_K_printf,		/* lnode is exp_list, rnode is redirect */
	Node_K_next,		/* no subs */
	Node_K_exit,		/* subnode is return value, or NULL */
/*50*/	Node_K_do,		/* lnode is conditional, rnode stuff to run */
	Node_K_return,
	Node_K_delete,
	Node_K_getline,
	Node_K_function,	/* lnode is statement list, rnode is params */

	/* I/O redirection for print statements */
	Node_redirect_output,	/* subnode is where to redirect */
	Node_redirect_append,	/* subnode is where to redirect */
	Node_redirect_pipe,	/* subnode is where to redirect */
	Node_redirect_pipein,	/* subnode is where to redirect */
	Node_redirect_input,	/* subnode is where to redirect */

	/* Variables */
/*60*/	Node_var,		/* rnode is value, lnode is array stuff */
	Node_var_array,		/* array is ptr to elements, asize num of
				 * eles */
	Node_val,		/* node is a value - type in flags */

	/* Builtins   subnode is explist to work on, proc is func to call */
	Node_builtin,

	/*
	 * pattern: conditional ',' conditional ;  lnode of Node_line_range
	 * is the two conditionals (Node_cond_pair), other word (rnode place)
	 * is a flag indicating whether or not this range has been entered.
	 */
	Node_line_range,

	/*
	 * boolean test of membership in array lnode is string-valued
	 * expression rnode is array name 
	 */
	Node_in_array,

	Node_func,		/* lnode is param. list, rnode is body */
	Node_func_call,		/* lnode is name, rnode is argument list */

	Node_cond_exp,		/* lnode is conditonal, rnode is if_branches */
	Node_regex,
/*70*/	Node_hashnode,
	Node_ahash,
	Node_NF,
	Node_NR,
	Node_FNR,
	Node_FS,
	Node_RS,
	Node_FIELDWIDTHS,
	Node_IGNORECASE,
	Node_OFS,
	Node_ORS,
	Node_OFMT,
	Node_CONVFMT,
	Node_K_nextfile
} NODETYPE;

/*
 * NOTE - this struct is a rather kludgey -- it is packed to minimize
 * space usage, at the expense of cleanliness.  Alter at own risk.
 */
typedef struct exp_node {
	union {
		struct {
			union {
				struct exp_node *lptr;
				char *param_name;
				long ll;
			} l;
			union {
				struct exp_node *rptr;
				struct exp_node *(*pptr) ();
				Regexp *preg;
				struct for_loop_header *hd;
				struct exp_node **av;
				int r_ent;	/* range entered */
			} r;
			union {
				char *name;
				struct exp_node *extra;
				long xl;
			} x;
			short number;
			unsigned char reflags;
#			define	CASE	1
#			define	CONST	2
#			define	FS_DFLT	4
		} nodep;
		struct {
			AWKNUM fltnum;	/* this is here for optimal packing of
					 * the structure on many machines
					 */
			char *sp;
			size_t slen;
			unsigned char sref;
			char idx;
		} val;
		struct {
			struct exp_node *next;
			char *name;
			size_t length;
			struct exp_node *value;
		} hash;
#define	hnext	sub.hash.next
#define	hname	sub.hash.name
#define	hlength	sub.hash.length
#define	hvalue	sub.hash.value
		struct {
			struct exp_node *next;
			struct exp_node *name;
			struct exp_node *value;
		} ahash;
#define	ahnext	sub.ahash.next
#define	ahname	sub.ahash.name
#define	ahvalue	sub.ahash.value
	} sub;
	NODETYPE type;
	unsigned short flags;
#			define	MALLOC	1	/* can be free'd */
#			define	TEMP	2	/* should be free'd */
#			define	PERM	4	/* can't be free'd */
#			define	STRING	8	/* assigned as string */
#			define	STR	16	/* string value is current */
#			define	NUM	32	/* numeric value is current */
#			define	NUMBER	64	/* assigned as number */
#			define	MAYBE_NUM 128	/* user input:  if NUMERIC then
						 * a NUMBER */
#			define	ARRAYMAXED 256	/* array is at max size */
	char *vname;	/* variable's name */
} NODE;

#define lnode	sub.nodep.l.lptr
#define nextp	sub.nodep.l.lptr
#define rnode	sub.nodep.r.rptr
#define source_file	sub.nodep.x.name
#define	source_line	sub.nodep.number
#define	param_cnt	sub.nodep.number
#define param	sub.nodep.l.param_name

#define subnode	lnode
#define proc	sub.nodep.r.pptr

#define re_reg	sub.nodep.r.preg
#define re_flags sub.nodep.reflags
#define re_text lnode
#define re_exp	sub.nodep.x.extra
#define	re_cnt	sub.nodep.number

#define forsub	lnode
#define forloop	rnode->sub.nodep.r.hd

#define stptr	sub.val.sp
#define stlen	sub.val.slen
#define stref	sub.val.sref
#define	stfmt	sub.val.idx

#define numbr	sub.val.fltnum

#define var_value lnode
#define var_array sub.nodep.r.av
#define array_size sub.nodep.l.ll
#define table_size sub.nodep.x.xl

#define condpair lnode
#define triggered sub.nodep.r.r_ent

#ifdef DONTDEF
int primes[] = {31, 61, 127, 257, 509, 1021, 2053, 4099, 8191, 16381};
#endif

typedef struct for_loop_header {
	NODE *init;
	NODE *cond;
	NODE *incr;
} FOR_LOOP_HEADER;

/* for "for(iggy in foo) {" */
struct search {
	NODE *sym;
	size_t idx;
	NODE *bucket;
	NODE *retval;
};

/* for faster input, bypass stdio */
typedef struct iobuf {
	int fd;
	char *buf;
	char *off;
	char *end;
	size_t size;	/* this will be determined by an fstat() call */
	int cnt;
	long secsiz;
	int flag;
#	define		IOP_IS_TTY	1
#	define		IOP_IS_INTERNAL	2
#	define		IOP_NO_FREE	4
} IOBUF;

typedef void (*Func_ptr)();

/*
 * structure used to dynamically maintain a linked-list of open files/pipes
 */
struct redirect {
	unsigned int flag;
#		define		RED_FILE	1
#		define		RED_PIPE	2
#		define		RED_READ	4
#		define		RED_WRITE	8
#		define		RED_APPEND	16
#		define		RED_NOBUF	32
#		define		RED_USED	64
#		define		RED_EOF		128
	char *value;
	FILE *fp;
	IOBUF *iop;
	int pid;
	int status;
	struct redirect *prev;
	struct redirect *next;
};

/* structure for our source, either a command line string or a source file */
struct src {
	enum srctype { CMDLINE = 1, SOURCEFILE } stype;
	char *val;
};

/* longjmp return codes, must be nonzero */
/* Continue means either for loop/while continue, or next input record */
#define TAG_CONTINUE 1
/* Break means either for/while break, or stop reading input */
#define TAG_BREAK 2
/* Return means return from a function call; leave value in ret_node */
#define	TAG_RETURN 3

#ifndef INT_MAX
#define INT_MAX (~(1 << (sizeof (int) * 8 - 1)))
#endif
#ifndef LONG_MAX
#define LONG_MAX (~(1 << (sizeof (long) * 8 - 1)))
#endif
#ifndef ULONG_MAX
#define ULONG_MAX (~(unsigned long)0)
#endif
#ifndef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1)
#endif
#define HUGE    INT_MAX 

/* -------------------------- External variables -------------------------- */
/* gawk builtin variables */
extern long NF;
extern long NR;
extern long FNR;
extern int IGNORECASE;
extern char *RS;
extern char *OFS;
extern int OFSlen;
extern char *ORS;
extern int ORSlen;
extern char *OFMT;
extern char *CONVFMT;
extern int CONVFMTidx;
extern int OFMTidx;
extern NODE *FS_node, *NF_node, *RS_node, *NR_node;
extern NODE *FILENAME_node, *OFS_node, *ORS_node, *OFMT_node;
extern NODE *CONVFMT_node;
extern NODE *FNR_node, *RLENGTH_node, *RSTART_node, *SUBSEP_node;
extern NODE *IGNORECASE_node;
extern NODE *FIELDWIDTHS_node;

extern NODE **stack_ptr;
extern NODE *Nnull_string;
extern NODE **fields_arr;
extern int sourceline;
extern char *source;
extern NODE *expression_value;

extern NODE *_t;	/* used as temporary in tree_eval */

extern const char *myname;

extern NODE *nextfree;
extern int field0_valid;
extern int do_unix;
extern int do_posix;
extern int do_lint;
extern int in_begin_rule;
extern int in_end_rule;

/* ------------------------- Pseudo-functions ------------------------- */

#define is_identchar(c) (isalnum(c) || (c) == '_')


#ifndef MPROF
#define	getnode(n)	if (nextfree) n = nextfree, nextfree = nextfree->nextp;\
			else n = more_nodes()
#define	freenode(n)	((n)->nextp = nextfree, nextfree = (n))
#else
#define	getnode(n)	emalloc(n, NODE *, sizeof(NODE), "getnode")
#define	freenode(n)	free(n)
#endif

#ifdef DEBUG
#define	tree_eval(t)	r_tree_eval(t)
#define	get_lhs(p, a)	r_get_lhs((p), (a))
#undef freenode
#else
#define	get_lhs(p, a)	((p)->type == Node_var ? (&(p)->var_value) : \
			r_get_lhs((p), (a)))
#define	tree_eval(t)	(_t = (t),_t == NULL ? Nnull_string : \
			(_t->type == Node_param_list ? r_tree_eval(_t) : \
			(_t->type == Node_val ? _t : \
			(_t->type == Node_var ? _t->var_value : \
			r_tree_eval(_t)))))
#endif

#define	make_number(x)	mk_number((x), (unsigned int)(MALLOC|NUM|NUMBER))
#define	tmp_number(x)	mk_number((x), (unsigned int)(MALLOC|TEMP|NUM|NUMBER))

#define	free_temp(n)	do {if ((n)->flags&TEMP) { unref(n); }} while (0)
#define	make_string(s,l)	make_str_node((s), SZTC (l),0)
#define		SCAN			1
#define		ALREADY_MALLOCED	2

#define	cant_happen()	fatal("internal error line %d, file: %s", \
				__LINE__, __FILE__);

#if defined(__STDC__) && !defined(NO_TOKEN_PASTING)
#define	emalloc(var,ty,x,str)	(void)((var=(ty)malloc((MALLOC_ARG_T)(x))) ||\
				 (fatal("%s: %s: can't allocate memory (%s)",\
					(str), #var, strerror(errno)),0))
#define	erealloc(var,ty,x,str)	(void)((var=(ty)realloc((char *)var,\
						  (MALLOC_ARG_T)(x))) ||\
				 (fatal("%s: %s: can't allocate memory (%s)",\
					(str), #var, strerror(errno)),0))
#else /* __STDC__ */
#define	emalloc(var,ty,x,str)	(void)((var=(ty)malloc((MALLOC_ARG_T)(x))) ||\
				 (fatal("%s: %s: can't allocate memory (%s)",\
					(str), "var", strerror(errno)),0))
#define	erealloc(var,ty,x,str)	(void)((var=(ty)realloc((char *)var,\
						  (MALLOC_ARG_T)(x))) ||\
				 (fatal("%s: %s: can't allocate memory (%s)",\
					(str), "var", strerror(errno)),0))
#endif /* __STDC__ */

#ifdef DEBUG
#define	force_number	r_force_number
#define	force_string	r_force_string
#else /* not DEBUG */
#ifdef lint
extern AWKNUM force_number();
#endif
#ifdef MSDOS
extern double _msc51bug;
#define	force_number(n)	(_msc51bug=(_t = (n),(_t->flags & NUM) ? _t->numbr : r_force_number(_t)))
#else /* not MSDOS */
#define	force_number(n)	(_t = (n),(_t->flags & NUM) ? _t->numbr : r_force_number(_t))
#endif /* MSDOS */
#define	force_string(s)	(_t = (s),(_t->flags & STR) ? _t : r_force_string(_t))
#endif /* not DEBUG */

#define	STREQ(a,b)	(*(a) == *(b) && strcmp((a), (b)) == 0)
#define	STREQN(a,b,n)	((n)&& *(a)== *(b) && strncmp((a), (b), SZTC (n)) == 0)

/* ------------- Function prototypes or defs (as appropriate) ------------- */

/* array.c */
extern NODE *concat_exp P((NODE *tree));
extern void assoc_clear P((NODE *symbol));
extern unsigned int hash P((const char *s, size_t len, unsigned long hsize));
extern int in_array P((NODE *symbol, NODE *subs));
extern NODE **assoc_lookup P((NODE *symbol, NODE *subs));
extern void do_delete P((NODE *symbol, NODE *tree));
extern void assoc_scan P((NODE *symbol, struct search *lookat));
extern void assoc_next P((struct search *lookat));
/* awk.tab.c */
extern char *tokexpand P((void));
extern char nextc P((void));
extern NODE *node P((NODE *left, NODETYPE op, NODE *right));
extern NODE *install P((char *name, NODE *value));
extern NODE *lookup P((const char *name));
extern NODE *variable P((char *name, int can_free));
extern int yyparse P((void));
/* builtin.c */
extern NODE *do_exp P((NODE *tree));
extern NODE *do_index P((NODE *tree));
extern NODE *do_int P((NODE *tree));
extern NODE *do_length P((NODE *tree));
extern NODE *do_log P((NODE *tree));
extern NODE *do_sprintf P((NODE *tree));
extern void do_printf P((NODE *tree));
extern void print_simple P((NODE *tree, FILE *fp));
extern NODE *do_sqrt P((NODE *tree));
extern NODE *do_substr P((NODE *tree));
extern NODE *do_strftime P((NODE *tree));
extern NODE *do_systime P((NODE *tree));
extern NODE *do_system P((NODE *tree));
extern void do_print P((NODE *tree));
extern NODE *do_tolower P((NODE *tree));
extern NODE *do_toupper P((NODE *tree));
extern NODE *do_atan2 P((NODE *tree));
extern NODE *do_sin P((NODE *tree));
extern NODE *do_cos P((NODE *tree));
extern NODE *do_rand P((NODE *tree));
extern NODE *do_srand P((NODE *tree));
extern NODE *do_match P((NODE *tree));
extern NODE *do_gsub P((NODE *tree));
extern NODE *do_sub P((NODE *tree));
/* eval.c */
extern int interpret P((NODE *volatile tree));
extern NODE *r_tree_eval P((NODE *tree));
extern int cmp_nodes P((NODE *t1, NODE *t2));
extern NODE **r_get_lhs P((NODE *ptr, Func_ptr *assign));
extern void set_IGNORECASE P((void));
void set_OFS P((void));
void set_ORS P((void));
void set_OFMT P((void));
void set_CONVFMT P((void));
/* field.c */
extern void init_fields P((void));
extern void set_record P((char *buf, int cnt, int freeold));
extern void reset_record P((void));
extern void set_NF P((void));
extern NODE **get_field P((int num, Func_ptr *assign));
extern NODE *do_split P((NODE *tree));
extern void set_FS P((void));
extern void set_RS P((void));
extern void set_FIELDWIDTHS P((void));
/* io.c */
extern void set_FNR P((void));
extern void set_NR P((void));
extern void do_input P((void));
extern struct redirect *redirect P((NODE *tree, int *errflg));
extern NODE *do_close P((NODE *tree));
extern int flush_io P((void));
extern int close_io P((void));
extern int devopen P((const char *name, const char *mode));
extern int pathopen P((const char *file));
extern NODE *do_getline P((NODE *tree));
extern void do_nextfile P((void));
/* iop.c */
extern int optimal_bufsize P((int fd));
extern IOBUF *iop_alloc P((int fd));
extern int get_a_record P((char **out, IOBUF *iop, int rs, int *errcode));
/* main.c */
extern int main P((int argc, char **argv));
extern Regexp *mk_re_parse P((char *s, int ignorecase));
extern void load_environ P((void));
extern char *arg_assign P((char *arg));
extern SIGTYPE catchsig P((int sig, int code));
/* msg.c */
extern void err P((const char *s, const char *emsg, va_list argp));
#if _MSC_VER == 510
extern void msg P((va_list va_alist, ...));
extern void warning P((va_list va_alist, ...));
extern void fatal P((va_list va_alist, ...));
#else
extern void msg ();
extern void warning ();
extern void fatal ();
#endif
/* node.c */
extern AWKNUM r_force_number P((NODE *n));
extern NODE *r_force_string P((NODE *s));
extern NODE *dupnode P((NODE *n));
extern NODE *mk_number P((AWKNUM x, unsigned int flags));
extern NODE *make_str_node P((char *s, size_t len, int scan ));
extern NODE *tmp_string P((char *s, size_t len ));
extern NODE *more_nodes P((void));
#ifdef DEBUG
extern void freenode P((NODE *it));
#endif
extern void unref P((NODE *tmp));
extern int parse_escape P((char **string_ptr));
/* re.c */
extern Regexp *make_regexp P((char *s, size_t len, int ignorecase, int dfa));
extern int research P((Regexp *rp, char *str, int start,
		       size_t len, int need_start));
extern void refree P((Regexp *rp));
extern void reg_error P((const char *s));
extern Regexp *re_update P((NODE *t));
extern void resyntax P((int syntax));
extern void resetup P((void));

/* strcase.c */
extern int strcasecmp P((const char *s1, const char *s2));
extern int strncasecmp P((const char *s1, const char *s2, register size_t n));

#ifdef atarist
/* atari/tmpnam.c */
extern char *tmpnam P((char *buf));
extern char *tempnam P((const char *path, const char *base));
#endif

/* Figure out what '\a' really is. */
#ifdef __STDC__
#define BELL	'\a'		/* sure makes life easy, don't it? */
#else
#	if 'z' - 'a' == 25	/* ascii */
#		if 'a' != 97	/* machine is dumb enough to use mark parity */
#			define BELL	'\207'
#		else
#			define BELL	'\07'
#		endif
#	else
#		define BELL	'\057'
#	endif
#endif

extern char casetable[];	/* for case-independent regexp matching */
