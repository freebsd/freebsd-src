/* ed.h: type and constant definitions for the ed editor. */
/*
 * Copyright (c) 1993 Andrew Moore
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ed.h,v 1.5 1994/02/01 00:34:39 alm Exp
 *	$FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>		/* for MAXPATHLEN */
#include <errno.h>
#if defined(sun) || defined(__NetBSD__)
# include <limits.h>
#endif
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERR		(-2)
#define EMOD		(-3)
#define FATAL		(-4)

#ifndef MAXPATHLEN
# define MAXPATHLEN 255		/* _POSIX_PATH_MAX */
#endif

#define MINBUFSZ 512		/* minimum buffer size - must be > 0 */
#define SE_MAX 30		/* max subexpressions in a regular expression */
#ifdef INT_MAX
# define LINECHARS INT_MAX	/* max chars per line */
#else
# define LINECHARS MAXINT	/* max chars per line */
#endif

/* gflags */
#define GLB 001		/* global command */
#define GPR 002		/* print after command */
#define GLS 004		/* list after command */
#define GNP 010		/* enumerate after command */
#define GSG 020		/* global substitute */

typedef regex_t pattern_t;

/* Line node */
typedef struct	line {
	struct line	*q_forw;
	struct line	*q_back;
	off_t		seek;		/* address of line in scratch buffer */
	int		len;		/* length of line */
} line_t;


typedef struct undo {

/* type of undo nodes */
#define UADD	0
#define UDEL 	1
#define UMOV	2
#define VMOV	3

	int type;			/* command type */
	line_t	*h;			/* head of list */
	line_t  *t;			/* tail of list */
} undo_t;

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define INC_MOD(l, k)	((l) + 1 > (k) ? 0 : (l) + 1)
#define DEC_MOD(l, k)	((l) - 1 < 0 ? (k) : (l) - 1)

/* SPL1: disable some interrupts (requires reliable signals) */
#define SPL1() mutex++

/* SPL0: enable all interrupts; check sigflags (requires reliable signals) */
#define SPL0() \
if (--mutex == 0) { \
	if (sigflags & (1 << (SIGHUP - 1))) handle_hup(SIGHUP); \
	if (sigflags & (1 << (SIGINT - 1))) handle_int(SIGINT); \
}

/* STRTOL: convert a string to long */
#define STRTOL(i, p) { \
	if (((i = strtol(p, &p, 10)) == LONG_MIN || i == LONG_MAX) && \
	    errno == ERANGE) { \
		sprintf(errmsg, "number out of range"); \
	    	i = 0; \
		return ERR; \
	} \
}

#if defined(sun) || defined(NO_REALLOC_NULL)
/* REALLOC: assure at least a minimum size for buffer b */
#define REALLOC(b,n,i,err) \
if ((i) > (n)) { \
	int ti = (n); \
	char *ts; \
	SPL1(); \
	if ((b) != NULL) { \
		if ((ts = (char *) realloc((b), ti += max((i), MINBUFSZ))) == NULL) { \
			fprintf(stderr, "%s\n", strerror(errno)); \
			sprintf(errmsg, "out of memory"); \
			SPL0(); \
			return err; \
		} \
	} else { \
		if ((ts = (char *) malloc(ti += max((i), MINBUFSZ))) == NULL) { \
			fprintf(stderr, "%s\n", strerror(errno)); \
			sprintf(errmsg, "out of memory"); \
			SPL0(); \
			return err; \
		} \
	} \
	(n) = ti; \
	(b) = ts; \
	SPL0(); \
}
#else /* NO_REALLOC_NULL */
/* REALLOC: assure at least a minimum size for buffer b */
#define REALLOC(b,n,i,err) \
if ((i) > (n)) { \
	int ti = (n); \
	char *ts; \
	SPL1(); \
	if ((ts = (char *) realloc((b), ti += max((i), MINBUFSZ))) == NULL) { \
		fprintf(stderr, "%s\n", strerror(errno)); \
		sprintf(errmsg, "out of memory"); \
		SPL0(); \
		return err; \
	} \
	(n) = ti; \
	(b) = ts; \
	SPL0(); \
}
#endif /* NO_REALLOC_NULL */

/* REQUE: link pred before succ */
#define REQUE(pred, succ) (pred)->q_forw = (succ), (succ)->q_back = (pred)

/* INSQUE: insert elem in circular queue after pred */
#define INSQUE(elem, pred) \
{ \
	REQUE((elem), (pred)->q_forw); \
	REQUE((pred), elem); \
}

/* REMQUE: remove_lines elem from circular queue */
#define REMQUE(elem) REQUE((elem)->q_back, (elem)->q_forw);

/* NUL_TO_NEWLINE: overwrite ASCII NULs with newlines */
#define NUL_TO_NEWLINE(s, l) translit_text(s, l, '\0', '\n')

/* NEWLINE_TO_NUL: overwrite newlines with ASCII NULs */
#define NEWLINE_TO_NUL(s, l) translit_text(s, l, '\n', '\0')

#ifdef sun
# define strerror(n) sys_errlist[n]
#endif

#ifndef __P
# ifndef __STDC__
#  define __P(proto) ()
# else
#  define __P(proto) proto
# endif
#endif

/* Local Function Declarations */
void add_line_node __P((line_t *));
int append_lines __P((long));
int apply_subst_template __P((char *, regmatch_t *, int, int));
int build_active_list __P((int));
int cbc_decode __P((char *, FILE *));
int cbc_encode __P((char *, int, FILE *));
int check_addr_range __P((long, long));
void clear_active_list __P((void));
void clear_undo_stack __P((void));
int close_sbuf __P((void));
int copy_lines __P((long));
int delete_lines __P((long, long));
void des_error __P((char *));
int display_lines __P((long, long, int));
line_t *dup_line_node __P((line_t *));
int exec_command __P((void));
long exec_global __P((int, int));
void expand_des_key __P((char *, char *));
int extract_addr_range __P((void));
char *extract_pattern __P((int));
int extract_subst_tail __P((int *, int *));
char *extract_subst_template __P((void));
int filter_lines __P((long, long, char *));
int flush_des_file __P((FILE *));
line_t *get_addressed_line_node __P((long));
pattern_t *get_compiled_pattern __P((void));
int get_des_char __P((FILE *));
char *get_extended_line __P((int *, int));
char *get_filename __P((void));
int get_keyword __P((void));
long get_line_node_addr __P((line_t *));
long get_matching_node_addr __P((pattern_t *, int));
long get_marked_node_addr __P((int));
char *get_sbuf_line __P((line_t *));
int get_shell_command __P((void));
int get_stream_line __P((FILE *));
int get_tty_line __P((void));
void handle_hup __P((int));
void handle_int __P((int));
void handle_winch __P((int));
int has_trailing_escape __P((char *, char *));
int hex_to_binary __P((int, int));
void init_buffers __P((void));
void init_des_cipher __P((void));
int is_legal_filename __P((char *));
int join_lines __P((long, long));
int mark_line_node __P((line_t *, int));
int move_lines __P((long));
line_t *next_active_node __P(());
long next_addr __P((void));
int open_sbuf __P((void));
char *parse_char_class __P((char *));
int pop_undo_stack __P((void));
undo_t *push_undo_stack __P((int, long, long));
int put_des_char __P((int, FILE *));
char *put_sbuf_line __P((char *));
int put_stream_line __P((FILE *, char *, int));
int put_tty_line __P((char *, int, long, int));
void quit __P((int));
long read_file __P((char *, long));
long read_stream __P((FILE *, long));
int search_and_replace __P((pattern_t *, int, int));
int set_active_node __P((line_t *));
void set_des_key __P((char *));
void signal_hup __P((int));
void signal_int __P((int));
char *strip_escapes __P((char *));
int substitute_matching_text __P((pattern_t *, line_t *, int, int));
char *translit_text __P((char *, int, int, int));
void unmark_line_node __P((line_t *));
void unset_active_nodes __P((line_t *, line_t *));
long write_file __P((char *, char *, long, long));
long write_stream __P((FILE *, long, long));

/* global buffers */
extern char stdinbuf[];
extern char *ibuf;
extern char *ibufp;
extern int ibufsz;

/* global flags */
extern int isbinary;
extern int isglobal;
extern int modified;
extern int mutex;
extern int sigflags;

/* global vars */
extern long addr_last;
extern long current_addr;
extern char errmsg[];
extern long first_addr;
extern int lineno;
extern long second_addr;
#ifdef sun
extern char *sys_errlist[];
#endif
