/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)term.h	8.48 (Berkeley) 7/25/94
 */

/*
 * Fundamental character types.
 *
 * CHAR_T	An integral type that can hold any character.
 * ARG_CHAR_T	The type of a CHAR_T when passed as an argument using
 *		traditional promotion rules.  It should also be able
 *		to be compared against any CHAR_T for equality without
 *		problems.
 * MAX_CHAR_T	The maximum value of any character.
 *
 * If no integral type can hold a character, don't even try the port.
 */
typedef	u_char		CHAR_T;
typedef	u_int		ARG_CHAR_T;
#define	MAX_CHAR_T	0xff

/* The maximum number of columns any character can take up on a screen. */
#define	MAX_CHARACTER_COLUMNS	4

/* Structure to return a character and associated information. */
struct _ch {
	CHAR_T	 ch;		/* Character. */

#define K_NOTUSED	 0
#define	K_CARAT		 1
#define	K_CNTRLD	 2
#define	K_CNTRLR	 3
#define	K_CNTRLT	 4
#define	K_CNTRLZ	 5
#define	K_COLON	 	 6
#define	K_CR		 7
#define	K_ESCAPE	 8
#define	K_FORMFEED	 9
#define	K_HEXCHAR	10
#define	K_NL		11
#define	K_RIGHTBRACE	12
#define	K_RIGHTPAREN	13
#define	K_TAB		14
#define	K_VERASE	15
#define	K_VKILL		16
#define	K_VLNEXT	17
#define	K_VWERASE	18
#define	K_ZERO		19
	u_int8_t value;		/* Special character flag values. */

#define	CH_ABBREVIATED	0x01	/* Character from an abbreviation. */
#define	CH_MAPPED	0x02	/* Character from a map. */
#define	CH_NOMAP	0x04	/* Do not map the character. */
#define	CH_QUOTED	0x08	/* Character is already quoted. */
	u_int8_t flags;
};

typedef struct _keylist {
	u_int8_t value;		/* Special value. */
	CHAR_T	 ch;		/* Key. */
} KEYLIST;

extern KEYLIST keylist[];

/* Structure for the key input buffer. */
struct _ibuf {
	CHAR_T	 *ch;		/* Array of characters. */
	u_int8_t *chf;		/* Array of character flags (CH_*). */

	size_t	 cnt;		/* Count of remaining characters. */
	size_t	 nelem;		/* Numer of array elements. */
	size_t	 next;		/* Offset of next array entry. */
};
				/* Return if more keys in queue. */
#define	KEYS_WAITING(sp)	((sp)->gp->tty->cnt)
#define	MAPPED_KEYS_WAITING(sp)						\
	(KEYS_WAITING(sp) && sp->gp->tty->chf[sp->gp->tty->next] & CH_MAPPED)

/*
 * Routines that return a key as a side-effect return:
 *
 *	INP_OK		Returning a character; must be 0.
 *	INP_EOF		EOF.
 *	INP_ERR		Error.
 *	INP_INTR	Interrupted.
 *
 * The vi structure depends on the key routines being able to return INP_EOF
 * multiple times without failing -- eventually enough things will end due to
 * INP_EOF that vi will reach the command level for the screen, at which point
 * the exit flags will be set and vi will exit.
 */
enum input	{ INP_OK=0, INP_EOF, INP_ERR, INP_INTR };

/*
 * Routines that return a confirmation return:
 *
 *	CONF_NO		User answered no.
 *	CONF_QUIT	User answered quit, eof or an error.
 *	CONF_YES	User answered yes.
 */
enum confirm	{ CONF_NO, CONF_QUIT, CONF_YES };

/*
 * Ex/vi commands are generally separated by whitespace characters.  We
 * can't use the standard isspace(3) macro because it returns true for
 * characters like ^K in the ASCII character set.  The 4.4BSD isblank(3)
 * macro does exactly what we want, but it's not portable yet.
 *
 * XXX
 * Note side effect, ch is evaluated multiple times.
 */
#ifndef isblank
#define	isblank(ch)	((ch) == ' ' || (ch) == '\t')
#endif

/* The "standard" tab width, for displaying things to users. */
#define	STANDARD_TAB	6

/* Various special characters, messages. */
#define	CH_BSEARCH	'?'			/* Backward search prompt. */
#define	CH_CURSOR	' '			/* Cursor character. */
#define	CH_ENDMARK	'$'			/* End of a range. */
#define	CH_EXPROMPT	':'			/* Ex prompt. */
#define	CH_FSEARCH	'/'			/* Forward search prompt. */
#define	CH_HEX		'\030'			/* Leading hex character. */
#define	CH_LITERAL	'\026'			/* ASCII ^V. */
#define	CH_NO		'n'			/* No. */
#define	CH_NOT_DIGIT	'a'			/* A non-isdigit() character. */
#define	CH_QUIT		'q'			/* Quit. */
#define	CH_YES		'y'			/* Yes. */

#define	STR_CONFIRM	"confirm? [ynq]"
#define	STR_CMSG	"Enter return to continue: "
#define	STR_QMSG	"Enter return to continue [q to quit]: "

/* Flags describing how input is handled. */
#define	TXT_AICHARS	0x00000001	/* Leading autoindent chars. */
#define	TXT_ALTWERASE	0x00000002	/* Option: altwerase. */
#define	TXT_APPENDEOL	0x00000004	/* Appending after EOL. */
#define	TXT_AUTOINDENT	0x00000008	/* Autoindent set this line. */
#define	TXT_BACKSLASH	0x00000010	/* Backslashes escape characters. */
#define	TXT_BEAUTIFY	0x00000020	/* Only printable characters. */
#define	TXT_BS		0x00000040	/* Backspace returns the buffer. */
#define	TXT_CNTRLD	0x00000080	/* Control-D is a special command. */
#define	TXT_CNTRLT	0x00000100	/* Control-T is an indent special. */
#define	TXT_CR		0x00000200	/* CR returns the buffer. */
#define	TXT_DOTTERM	0x00000400	/* Leading '.' terminates the input. */
#define	TXT_EMARK	0x00000800	/* End of replacement mark. */
#define	TXT_ESCAPE	0x00001000	/* Escape returns the buffer. */
#define	TXT_EXSUSPEND	0x00002000	/* ^Z should suspend the session. */
#define	TXT_INFOLINE	0x00004000	/* Editing the info line. */
#define	TXT_MAPCOMMAND	0x00008000	/* Apply the command map. */
#define	TXT_MAPINPUT	0x00010000	/* Apply the input map. */
#define	TXT_MAPNODIGIT	0x00020000	/* Return to a digit. */
#define	TXT_NLECHO	0x00040000	/* Echo the newline. */
#define	TXT_OVERWRITE	0x00080000	/* Overwrite characters. */
#define	TXT_PROMPT	0x00100000	/* Display a prompt. */
#define	TXT_RECORD	0x00200000	/* Record for replay. */
#define	TXT_REPLACE	0x00400000	/* Replace; don't delete overwrite. */
#define	TXT_REPLAY	0x00800000	/* Replay the last input. */
#define	TXT_RESOLVE	0x01000000	/* Resolve the text into the file. */
#define	TXT_SHOWMATCH	0x02000000	/* Option: showmatch. */
#define	TXT_TTYWERASE	0x04000000	/* Option: ttywerase. */
#define	TXT_WRAPMARGIN	0x08000000	/* Option: wrapmargin. */

/* Support keyboard routines. */
size_t		 __key_len __P((SCR *, ARG_CHAR_T));
CHAR_T		*__key_name __P((SCR *, ARG_CHAR_T));
int		 __key_val __P((SCR *, ARG_CHAR_T));
void		 key_init __P((SCR *));
void		 term_flush __P((SCR *, char *, u_int));
enum input	 term_key __P((SCR *, CH *, u_int));
enum input	 term_user_key __P((SCR *, CH *));
int		 term_init __P((SCR *));
int		 term_push __P((SCR *, CHAR_T *, size_t, u_int));
