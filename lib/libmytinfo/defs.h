/*
 * defs.h
 *
 * By Ross Ridge
 * Public Domain
 * 92/06/04 11:37:02
 *
 * @(#) mytinfo defs.h 3.3 92/06/04 public domain, By Ross Ridge
 */

#ifndef _DEFS_H_
#define _DEFS_H_

#ifdef TEST
#undef NOTLIB
#define NOTLIB
#endif

#include "config.h"

#ifdef NOTLIB
#undef USE_FAKE_STDIO
#endif

#ifdef USE_STDDEF
#include <stddef.h>
#else
#include <sys/types.h>
#endif

#ifdef USE_STDLIB
#include <stdlib.h>
#else
#ifdef USE_PROTOTYPES
anyptr malloc(mysize_t);
anyptr realloc(anyptr, mysize_t);
char *getenv(char const *);
#else
anyptr malloc();
anyptr realloc();
char *getenv();
#endif
#endif

#ifdef USE_STDARG
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifndef _VA_LIST
#define _VA_LIST
#endif

#ifdef USE_FAKE_STDIO
#include "fake_stdio.h"
#define sprintf _fake_sprintf
#ifdef USE_PROTOTYPES
int sprintf(char *, char *, ...);
#else
int sprintf();
#endif
#else /* USE_FAKE_STDIO */
#if 0
#include <stdio.h>
#else
#undef NULL
#include <stdio.h>
#endif
#endif /* !USE_FAKE_STDIO */

#ifdef USE_STRINGS
#include <strings.h>
#define strchr(s, c) index(s, c)
#define strrchr(s, c) rindex(s, c)
#ifndef USE_MYSTRTOK
#ifdef USE_PROTOTYPES
char *strtok(char *, char *);
#else
char *strtok();
#endif
#endif
#else
#include <string.h>
#endif

#ifdef USE_MEMORY
#include <memory.h>
#else
#define memcpy(b, a, n) bcopy(a, b, n)
#endif

#include <errno.h>

#define MAX_BUF	4096
#define MAX_LINE 640
#define MAX_NAME 128

#define MAX_CHUNK MAX_LINE

#define MAX_DEPTH 32

#define MAX_VARNAME	32
#define MAX_TINFONAME	5
#define MAX_TCAPNAME	2

struct caplist {
	char	type;
	char	flag;
	char	var[MAX_VARNAME + 1];
	char	tinfo[MAX_TINFONAME + 1];
	char	tcap[MAX_TCAPNAME + 1];
};

struct term_path {
	char *file;
	int type;	/* 0 = file, 1 = TERMCAP env, 2 = TERMINFO env */
};

struct _terminal;

#ifdef USE_PROTOTYPES

int _gettcap(char *, struct _terminal *, struct term_path *);
int _gettinfo(char *, struct _terminal *, struct term_path *);
int _fillterm(char *, struct term_path *, char *);
int _findterm(char *, struct term_path *, char *);
int _init_tty(void), _lit_output(void), _check_tty(void);
void _figure_termcap(void);
int _tmatch(char *, char *);
void _norm_output(void);
int readcaps(FILE *, struct caplist *, int);
noreturn void quit(int, char *, ...);
#ifdef lint
extern void (*cleanup)();
#else
extern void (*cleanup)(int);
#endif
struct term_path *_buildpath(char *, int, ...);
void _delpath(struct term_path *);
char *_addstr(char *);
struct strbuf *_endstr(void);
void _del_strs(struct _terminal *);
void _tcapconv(void);
void _tcapdefault(void);
int _getother(char *, struct term_path *, struct _terminal *);
int _gettbin(char *, struct _terminal *);
int _findboolcode(char *), _findnumcode(char *), _findstrcode(char *);
int _findboolname(char *), _findnumname(char *), _findstrname(char *);
int _findboolfname(char *), _findnumfname(char *), _findstrfname(char *);

#ifdef USE_ANSIC
int _compar(void const *, void const *);
typedef int (*compar_fn)(void const *, void const *);
#else
int _compar(anyptr, anyptr);
typedef int (*compar_fn)(anyptr, anyptr);
#endif

#else /* USE_PROTOTYPES */

int _gettcap(), _gettinfo(), _fillterm(), _findterm(), _init_tty();
int _lit_output(), _check_tty();
void _figure_termcap();
int _tmatch();
void _norm_output();
int readcaps();
noreturn void /* GOTO */ quit(/*FORMAT2*/);
extern void (*cleanup)();
struct term_path *_buildpath();
void _delpath();
char *_addstr();
struct strbuf *_endstr();
void _del_strs();
void _tcapconv();
void _tcapdefault();
int _getother();
int _gettbin();
int _findboolcode(), _findnumcode(), _findstrcode();
int _findboolname(), _findnumname(), _findstrname();
int _findboolfname(), _findnumfname(), _findstrfname();
int _compar();
typedef int (*compar_fn)();

#endif /* USE_PROTOTYPES */

extern char _strflags[];

extern char _mytinfo_version[];

/* for quit.c */
extern int sys_nerr;
extern char *prg_name;

#endif /* _DEFS_H_ */
