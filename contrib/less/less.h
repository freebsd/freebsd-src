/*
 * Copyright (C) 1984-2002  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Standard include file for "less".
 */

/*
 * Defines for MSDOS_COMPILER.
 */
#define	MSOFTC		1	/* Microsoft C */
#define	BORLANDC	2	/* Borland C */
#define	WIN32C		3	/* Windows (Borland C or Microsoft C) */
#define	DJGPPC		4	/* DJGPP C */

/*
 * Include the file of compile-time options.
 * The <> make cc search for it in -I., not srcdir.
 */
#include <defines.h>

#ifdef _SEQUENT_
/*
 * Kludge for Sequent Dynix systems that have sigsetmask, but
 * it's not compatible with the way less calls it.
 * {{ Do other systems need this? }}
 */
#undef HAVE_SIGSETMASK
#endif

/*
 * Language details.
 */
#if HAVE_VOID
#define	VOID_POINTER	void *
#else
#define	VOID_POINTER	char *
#define	void  int
#endif
#if HAVE_CONST
#define	constant	const
#else
#define	constant
#endif

#define	public		/* PUBLIC FUNCTION */

/* Library function declarations */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#ifdef _OSK
#include <modes.h>
#include <strings.h>
#endif
#if MSDOS_COMPILER==WIN32C || OS2
#include <io.h>
#endif
#if MSDOS_COMPILER==DJGPPC
#include <io.h>
#include <sys/exceptn.h>
#include <conio.h>
#include <pc.h>
#endif

#if !HAVE_STDLIB_H
char *getenv();
off_t lseek();
VOID_POINTER calloc();
void free();
#endif

/*
 * Simple lowercase test which can be used during option processing
 * (before options are parsed which might tell us what charset to use).
 */
#define SIMPLE_IS_UPPER(c)	((c) >= 'A' && (c) <= 'Z')
#define SIMPLE_IS_LOWER(c)	((c) >= 'a' && (c) <= 'z')
#define	SIMPLE_TO_UPPER(c)	((c) - 'a' + 'A')
#define	SIMPLE_TO_LOWER(c)	((c) - 'A' + 'a')

#if !HAVE_UPPER_LOWER
#define	isupper(c)	SIMPLE_IS_UPPER(c)
#define	islower(c)	SIMPLE_IS_LOWER(c)
#define	toupper(c)	SIMPLE_TO_UPPER(c)
#define	tolower(c)	SIMPLE_TO_LOWER(c)
#endif

#ifndef NULL
#define	NULL	0
#endif

#ifndef TRUE
#define	TRUE		1
#endif
#ifndef FALSE
#define	FALSE		0
#endif

#define	OPT_OFF		0
#define	OPT_ON		1
#define	OPT_ONPLUS	2

#if !HAVE_MEMCPY
#ifndef memcpy
#define	memcpy(to,from,len)	bcopy((from),(to),(len))
#endif
#endif

#define	BAD_LSEEK	((off_t)-1)

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

/*
 * Upper bound on the string length of an integer converted to string.
 * 302 / 1000 is ceil (log10 (2.0)).  Subtract 1 for the sign bit;
 * add 1 for integer division truncation; add 1 more for a minus sign.
 */
#define INT_STRLEN_BOUND(t) ((sizeof(t) * CHAR_BIT - 1) * 302 / 1000 + 1 + 1)

/*
 * Special types and constants.
 */
typedef off_t		POSITION;
typedef off_t		LINENUM;
#define MIN_LINENUM_WIDTH  7	/* Min printing width of a line number */

#define	NULL_POSITION	((POSITION)(-1))

/*
 * Flags for open()
 */
#if MSDOS_COMPILER || OS2
#define	OPEN_READ	(O_RDONLY|O_BINARY)
#else
#ifdef _OSK
#define	OPEN_READ	(S_IREAD)
#else
#ifdef O_RDONLY
#define	OPEN_READ	(O_RDONLY)
#else
#define	OPEN_READ	(0)
#endif
#endif
#endif

#if defined(O_WRONLY) && defined(O_APPEND)
#define	OPEN_APPEND	(O_APPEND|O_WRONLY)
#else
#ifdef _OSK
#define OPEN_APPEND	(S_IWRITE)
#else
#define	OPEN_APPEND	(1)
#endif
#endif

/*
 * Set a file descriptor to binary mode.
 */
#if MSDOS_COMPILER==MSOFTC
#define	SET_BINARY(f)	_setmode(f, _O_BINARY);
#else
#if MSDOS_COMPILER || OS2
#define	SET_BINARY(f)	setmode(f, O_BINARY)
#else
#define	SET_BINARY(f)
#endif
#endif

/*
 * Does the shell treat "?" as a metacharacter?
 */
#if MSDOS_COMPILER || OS2 || _OSK
#define	SHELL_META_QUEST 0
#else
#define	SHELL_META_QUEST 1
#endif

#define	SPACES_IN_FILENAMES 1

/*
 * An IFILE represents an input file.
 */
#define	IFILE		VOID_POINTER
#define	NULL_IFILE	((IFILE)NULL)

/*
 * The structure used to represent a "screen position".
 * This consists of a file position, and a screen line number.
 * The meaning is that the line starting at the given file
 * position is displayed on the ln-th line of the screen.
 * (Screen lines before ln are empty.)
 */
struct scrpos
{
	POSITION pos;
	int ln;
};

typedef union parg
{
	char *p_string;
	int p_int;
	LINENUM p_linenum;
} PARG;

#define	NULL_PARG	((PARG *)NULL)

struct textlist
{
	char *string;
	char *endstring;
};

#define	EOI		(-1)

#define	READ_INTR	(-2)

/* How quiet should we be? */
#define	NOT_QUIET	0	/* Ring bell at eof and for errors */
#define	LITTLE_QUIET	1	/* Ring bell only for errors */
#define	VERY_QUIET	2	/* Never ring bell */

/* How should we prompt? */
#define	PR_SHORT	0	/* Prompt with colon */
#define	PR_MEDIUM	1	/* Prompt with message */
#define	PR_LONG		2	/* Prompt with longer message */

/* How should we handle backspaces? */
#define	BS_SPECIAL	0	/* Do special things for underlining and bold */
#define	BS_NORMAL	1	/* \b treated as normal char; actually output */
#define	BS_CONTROL	2	/* \b treated as control char; prints as ^H */

/* How should we search? */
#define	SRCH_FORW	000001	/* Search forward from current position */
#define	SRCH_BACK	000002	/* Search backward from current position */
#define	SRCH_NO_MOVE	000004	/* Highlight, but don't move */
#define	SRCH_FIND_ALL	000010	/* Find and highlight all matches */
#define	SRCH_NO_MATCH	000100	/* Search for non-matching lines */
#define	SRCH_PAST_EOF	000200	/* Search past end-of-file, into next file */
#define	SRCH_FIRST_FILE	000400	/* Search starting at the first file */
#define	SRCH_NO_REGEX	001000	/* Don't use regular expressions */

#define	SRCH_REVERSE(t)	(((t) & SRCH_FORW) ? \
				(((t) & ~SRCH_FORW) | SRCH_BACK) : \
				(((t) & ~SRCH_BACK) | SRCH_FORW))

/* */
#define	NO_MCA		0
#define	MCA_DONE	1
#define	MCA_MORE	2

#define	CC_OK		0	/* Char was accepted & processed */
#define	CC_QUIT		1	/* Char was a request to abort current cmd */
#define	CC_ERROR	2	/* Char could not be accepted due to error */
#define	CC_PASS		3	/* Char was rejected (internal) */

#define CF_QUIT_ON_ERASE 0001   /* Abort cmd if its entirely erased */

/* Special chars used to tell put_line() to do something special */
#define	AT_NORMAL	(0)
#define	AT_UNDERLINE	(1)
#define	AT_BOLD		(2)
#define	AT_BLINK	(3)
#define	AT_INVIS	(4)
#define	AT_STANDOUT	(5)

#if '0' == 240
#define IS_EBCDIC_HOST 1
#endif

#if IS_EBCDIC_HOST
/*
 * Long definition for EBCDIC.
 * Since the argument is usually a constant, this macro normally compiles
 * into a constant.
 */
#define CONTROL(c) ( \
	(c)=='[' ? '\047' : \
	(c)=='a' ? '\001' : \
	(c)=='b' ? '\002' : \
	(c)=='c' ? '\003' : \
	(c)=='d' ? '\067' : \
	(c)=='e' ? '\055' : \
	(c)=='f' ? '\056' : \
	(c)=='g' ? '\057' : \
	(c)=='h' ? '\026' : \
	(c)=='i' ? '\005' : \
	(c)=='j' ? '\025' : \
	(c)=='k' ? '\013' : \
	(c)=='l' ? '\014' : \
	(c)=='m' ? '\015' : \
	(c)=='n' ? '\016' : \
	(c)=='o' ? '\017' : \
	(c)=='p' ? '\020' : \
	(c)=='q' ? '\021' : \
	(c)=='r' ? '\022' : \
	(c)=='s' ? '\023' : \
	(c)=='t' ? '\074' : \
	(c)=='u' ? '\075' : \
	(c)=='v' ? '\062' : \
	(c)=='w' ? '\046' : \
	(c)=='x' ? '\030' : \
	(c)=='y' ? '\031' : \
	(c)=='z' ? '\077' : \
	(c)=='A' ? '\001' : \
	(c)=='B' ? '\002' : \
	(c)=='C' ? '\003' : \
	(c)=='D' ? '\067' : \
	(c)=='E' ? '\055' : \
	(c)=='F' ? '\056' : \
	(c)=='G' ? '\057' : \
	(c)=='H' ? '\026' : \
	(c)=='I' ? '\005' : \
	(c)=='J' ? '\025' : \
	(c)=='K' ? '\013' : \
	(c)=='L' ? '\014' : \
	(c)=='M' ? '\015' : \
	(c)=='N' ? '\016' : \
	(c)=='O' ? '\017' : \
	(c)=='P' ? '\020' : \
	(c)=='Q' ? '\021' : \
	(c)=='R' ? '\022' : \
	(c)=='S' ? '\023' : \
	(c)=='T' ? '\074' : \
	(c)=='U' ? '\075' : \
	(c)=='V' ? '\062' : \
	(c)=='W' ? '\046' : \
	(c)=='X' ? '\030' : \
	(c)=='Y' ? '\031' : \
	(c)=='Z' ? '\077' : \
	(c)=='|' ? '\031' : \
	(c)=='\\' ? '\034' : \
	(c)=='^' ? '\036' : \
	(c)&077)
#else
#define	CONTROL(c)	((c)&037)
#endif /* IS_EBCDIC_HOST */

#define	ESC		CONTROL('[')

#if _OSK_MWC32
#define	LSIGNAL(sig,func)	os9_signal(sig,func)
#else
#define	LSIGNAL(sig,func)	signal(sig,func)
#endif

#if HAVE_SIGPROCMASK
#if HAVE_SIGSET_T
#else
#undef HAVE_SIGPROCMASK
#endif
#endif
#if HAVE_SIGPROCMASK
#if HAVE_SIGEMPTYSET
#else
#undef  sigemptyset
#define sigemptyset(mp) *(mp) = 0
#endif
#endif

#define	S_INTERRUPT	01
#define	S_STOP		02
#define S_WINCH		04
#define	ABORT_SIGS()	(sigs & (S_INTERRUPT|S_STOP))

#define	QUIT_OK		0
#define	QUIT_ERROR	1
#define	QUIT_SAVED_STATUS (-1)

/* filestate flags */
#define	CH_CANSEEK	001
#define	CH_KEEPOPEN	002
#define	CH_POPENED	004
#define	CH_HELPFILE	010

#define	ch_zero()	((POSITION)0)

#define	FAKE_HELPFILE	"@/\\less/\\help/\\file/\\@"

#include "funcs.h"

/* Functions not included in funcs.h */
void postoa();
void linenumtoa();
void inttoa();
