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
 */

#ifndef lint
static char sccsid[] = "@(#)term.c	8.56 (Berkeley) 3/23/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "seq.h"

static int	keycmp __P((const void *, const void *));
static void	termkeyset __P((GS *, int, int));

/*
 * If we're reading less than 20 characters, up the size of the tty buffer.
 * This shouldn't ever happen, other than the first time through, but it's
 * possible if a map is large enough.
 */
#define	term_read_grow(sp, tty)					\
	(tty)->nelem - (tty)->cnt >= 20 ? 0 : __term_read_grow(sp, tty, 64)
static int __term_read_grow __P((SCR *, IBUF *, int));

/*
 * XXX
 * THIS REQUIRES THAT ALL SCREENS SHARE A TERMINAL TYPE.
 */
typedef struct _tklist {
	char	*ts;			/* Key's termcap string. */
	char	*output;		/* Corresponding vi command. */
	char	*name;			/* Name. */
	u_char	 value;			/* Special value (for lookup). */
} TKLIST;
static TKLIST const c_tklist[] = {	/* Command mappings. */
	{"kA",    "O",	"insert line"},
	{"kD",    "x",	"delete character"},
	{"kd",    "j",	"cursor down"},
	{"kE",    "D",	"delete to eol"},
	{"kF", "\004",	"scroll down"},
	{"kH",    "$",	"go to eol"},
	{"kh",    "^",	"go to sol"},
	{"kI",    "i",	"insert at cursor"},
	{"kL",   "dd",	"delete line"},
	{"kl",    "h",	"cursor left"},
	{"kN", "\006",	"page down"},
	{"kP", "\002",	"page up"},
	{"kR", "\025",	"scroll up"},
	{"kS",	 "dG",	"delete to end of screen"},
	{"kr",    "l",	"cursor right"},
	{"ku",    "k",	"cursor up"},
	{NULL},
};
static TKLIST const m1_tklist[] = {	/* Input mappings (lookup). */
	{"kl",   NULL,	"cursor erase", K_VERASE},
	{NULL},
};
static TKLIST const m2_tklist[] = {	/* Input mappings (set or delete). */
	{"kd",   NULL,	"cursor down"},
	{"ku",   NULL,	"cursor up"},
	{"kr",    " ",	"cursor space"},
	{NULL},
};

/*
 * !!!
 * Historic ex/vi always used:
 *
 *	^D: autoindent deletion
 *	^H: last character deletion
 *	^W: last word deletion
 *	^Q: quote the next character (if not used in flow control).
 *	^V: quote the next character
 *
 * regardless of the user's choices for these characters.  The user's erase
 * and kill characters worked in addition to these characters.  Ex was not
 * completely consistent with this scheme, as it did map the scroll command
 * to the user's current EOF character.  This implementation wires down the
 * above characters, but in addition uses the VERASE, VINTR, VKILL and VWERASE
 * characters described by the user's termios structure.  We don't do the EOF
 * mapping for ex, but I think I'm unlikely to get caught on that one.
 *
 * XXX
 * THIS REQUIRES THAT ALL SCREENS SHARE A SPECIAL KEY SET.
 */
typedef struct _keylist {
	u_char	value;			/* Special value. */
	CHAR_T	ch;			/* Key. */
} KEYLIST;
static KEYLIST keylist[] = {
	{K_CARAT,	   '^'},	/*  ^ */
	{K_CNTRLD,	'\004'},	/* ^D */
	{K_CNTRLR,	'\022'},	/* ^R */
	{K_CNTRLT,	'\024'},	/* ^T */
	{K_CNTRLZ,	'\032'},	/* ^Z */
	{K_COLON,	   ':'},	/*  : */
	{K_CR,		  '\r'},	/* \r */
	{K_ESCAPE,	'\033'},	/* ^[ */
	{K_FORMFEED,	  '\f'},	/* \f */
	{K_NL,		  '\n'},	/* \n */
	{K_RIGHTBRACE,	   '}'},	/*  } */
	{K_RIGHTPAREN,	   ')'},	/*  ) */
	{K_TAB,		  '\t'},	/* \t */
	{K_VERASE,	  '\b'},	/* \b */
	{K_VINTR,	'\003'},	/* ^C */
	{K_VKILL,	'\025'},	/* ^U */
	{K_VLNEXT,	'\021'},	/* ^Q */
	{K_VLNEXT,	'\026'},	/* ^V */
	{K_VWERASE,	'\027'},	/* ^W */
	{K_ZERO,	   '0'},	/*  0 */
	{K_NOTUSED, 0},			/* VERASE, VINTR, VKILL, VWERASE */
	{K_NOTUSED, 0},
	{K_NOTUSED, 0},
	{K_NOTUSED, 0},
};
static int nkeylist = (sizeof(keylist) / sizeof(keylist[0])) - 4;

/*
 * term_init --
 *	Initialize the special key lookup table, and the special keys
 *	defined by the terminal's termcap entry.
 */
int
term_init(sp)
	SCR *sp;
{
	extern CHNAME const asciiname[];	/* XXX */
	GS *gp;
	KEYLIST *kp;
	TKLIST const *tkp;
	int cnt;
	char *sbp, *t, buf[2 * 1024], sbuf[128];

	/*
	 * XXX
	 * 8-bit, ASCII only, for now.  Recompilation should get you
	 * any 8-bit character set, as long as nul isn't a character.
	 */
	gp = sp->gp;
	gp->cname = asciiname;			/* XXX */

#ifdef VERASE
	termkeyset(gp, VERASE, K_VERASE);
#endif
#ifdef VINTR
	termkeyset(gp, VINTR, K_VINTR);
#endif
#ifdef VKILL
	termkeyset(gp, VKILL, K_VKILL);
#endif
#ifdef VWERASE
	termkeyset(gp, VWERASE, K_VWERASE);
#endif
	/* Sort the special key list. */
	qsort(keylist, nkeylist, sizeof(keylist[0]), keycmp);

	/* Initialize the fast lookup table. */
	CALLOC_RET(sp,
	    gp->special_key, u_char *, MAX_FAST_KEY + 1, sizeof(u_char));
	for (gp->max_special = 0, kp = keylist, cnt = nkeylist; cnt--; ++kp) {
		if (gp->max_special < kp->value)
			gp->max_special = kp->value;
		if (kp->ch <= MAX_FAST_KEY)
			gp->special_key[kp->ch] = kp->value;
	}

	/* Set key sequences found in the termcap entry. */
	switch (tgetent(buf, O_STR(sp, O_TERM))) {
	case -1:
		msgq(sp, M_ERR,
		    "tgetent: %s: %s.", O_STR(sp, O_TERM), strerror(errno));
		return (0);
	case 0:
		msgq(sp, M_ERR,
		    "%s: unknown terminal type.", O_STR(sp, O_TERM));
		return (0);
	}

	/* Command mappings. */
	for (tkp = c_tklist; tkp->name != NULL; ++tkp) {
		sbp = sbuf;
		if ((t = tgetstr(tkp->ts, &sbp)) == NULL)
			continue;
		if (seq_set(sp, tkp->name, strlen(tkp->name), t, strlen(t),
		    tkp->output, strlen(tkp->output), SEQ_COMMAND, 0))
			return (1);
	}
	/* Input mappings needing to be looked up. */
	for (tkp = m1_tklist; tkp->name != NULL; ++tkp) {
		sbp = sbuf;
		if ((t = tgetstr(tkp->ts, &sbp)) == NULL)
			continue;
		for (kp = keylist;; ++kp)
			if (kp->value == tkp->value)
				break;
		if (kp == NULL)
			continue;
		if (seq_set(sp, tkp->name, strlen(tkp->name),
		    t, strlen(t), &kp->ch, 1, SEQ_INPUT, 0))
			return (1);
	}
	/* Input mappings that are already set or are text deletions. */
	for (tkp = m2_tklist; tkp->name != NULL; ++tkp) {
		sbp = sbuf;
		if ((t = tgetstr(tkp->ts, &sbp)) == NULL)
			continue;
		if (tkp->output == NULL) {
			if (seq_set(sp, tkp->name, strlen(tkp->name),
			    t, strlen(t), NULL, 0, SEQ_INPUT, 0))
				return (1);
		} else
			if (seq_set(sp, tkp->name, strlen(tkp->name),
			    t, strlen(t), tkp->output, strlen(tkp->output),
			    SEQ_INPUT, 0))
				return (1);
	}
	return (0);
}

/*
 * termkeyset --
 *	Set keys found in the termios structure.  VERASE, VINTR and VKILL are
 *	required by POSIX 1003.1-1990, VWERASE is a 4.4BSD extension.  We've
 *	left four open slots in the keylist table, if these values exist, put
 *	them into place.  Note, they may reset (or duplicate) values already
 *	in the table, so we check for that first.
 */
static void
termkeyset(gp, name, val)
	GS *gp;
	int name, val;
{
	KEYLIST *kp;
	cc_t ch;

	if (!F_ISSET(gp, G_TERMIOS_SET))
		return;
	if ((ch = gp->original_termios.c_cc[(name)]) == _POSIX_VDISABLE)
		return;

	/* Check for duplication. */
	for (kp = keylist; kp->value != K_NOTUSED; ++kp)
		if (kp->ch == ch) {
			kp->value = val;
			return;
		}
	/* Add a new entry. */
	if (kp->value == K_NOTUSED) {
		keylist[nkeylist].ch = ch;
		keylist[nkeylist].value = val;
		++nkeylist;
	}
}

/*
 * term_push --
 *	Push keys onto the front of a buffer.
 *
 * There is a single input buffer in ex/vi.  Characters are read onto the
 * end of the buffer by the terminal input routines, and pushed onto the
 * front of the buffer by various other functions in ex/vi.  Each key has
 * an associated flag value, which indicates if it has already been quoted,
 * if it is the result of a mapping or an abbreviation, as well as a count
 * of the number of times it has been mapped.
 */
int
term_push(sp, s, nchars, cmap, flags)
	SCR *sp;
	CHAR_T *s;			/* Characters. */
	size_t nchars;			/* Number of chars. */
	u_int cmap;			/* Map count. */
	u_int flags;			/* CH_* flags. */
{
	IBUF *tty;

	/* If we have room, stuff the keys into the buffer. */
	tty = sp->gp->tty;
	if (nchars <= tty->next ||
	    (tty->ch != NULL && tty->cnt == 0 && nchars <= tty->nelem)) {
		if (tty->cnt != 0)
			tty->next -= nchars;
		tty->cnt += nchars;
		MEMMOVE(tty->ch + tty->next, s, nchars);
		MEMSET(tty->chf + tty->next, flags, nchars);
		MEMSET(tty->cmap + tty->next, cmap, nchars);
		return (0);
	}

	/* Get enough space plus a little extra. */
	if (tty->cnt + nchars >= tty->nelem &&
	    __term_read_grow(sp, tty, MAX(nchars, 64)))
		return (1);

	/*
	 * If there are currently characters in the queue, shift them up,
	 * leaving some extra room.
	 */
#define	TERM_PUSH_SHIFT	30
	if (tty->cnt) {
		MEMMOVE(tty->ch + TERM_PUSH_SHIFT + nchars,
		    tty->ch + tty->next, tty->cnt);
		MEMMOVE(tty->chf + TERM_PUSH_SHIFT + nchars,
		    tty->chf + tty->next, tty->cnt);
		MEMMOVE(tty->cmap + TERM_PUSH_SHIFT + nchars,
		    tty->cmap + tty->next, tty->cnt);
	}

	/* Put the new characters into the queue. */
	tty->next = TERM_PUSH_SHIFT;
	tty->cnt += nchars;
	MEMMOVE(tty->ch + TERM_PUSH_SHIFT, s, nchars);
	MEMSET(tty->chf + TERM_PUSH_SHIFT, flags, nchars);
	MEMSET(tty->cmap + TERM_PUSH_SHIFT, cmap, nchars);
	return (0);
}

/*
 * Remove characters from the queue, simultaneously clearing the flag
 * and map counts.
 */
#define	QREM_HEAD(q, len) {						\
	size_t __off = (q)->next;					\
	if (len == 1) {							\
		tty->chf[__off] = 0;					\
		tty->cmap[__off] = 0;					\
	} else {							\
		MEMSET(tty->chf + __off, 0, len);			\
		MEMSET(tty->cmap + __off, 0, len);			\
	}								\
	if (((q)->cnt -= len) == 0)					\
		(q)->next = 0;						\
	else								\
		(q)->next += len;					\
}
#define	QREM_TAIL(q, len) {						\
	size_t __off = (q)->next + (q)->cnt - 1;			\
	if (len == 1) {							\
		tty->chf[__off] = 0;					\
		tty->cmap[__off] = 0;					\
	} else {							\
		MEMSET(tty->chf + __off, 0, len);			\
		MEMSET(tty->cmap + __off, 0, len);			\
	}								\
	if (((q)->cnt -= len) == 0)					\
		(q)->next = 0;						\
}

/*
 * term_key --
 *	Get the next key.
 *
 * !!!
 * The flag TXT_MAPNODIGIT probably needs some explanation.  First, the idea
 * of mapping keys is that one or more keystrokes act like a function key.
 * What's going on is that vi is reading a number, and the character following
 * the number may or may not be mapped (TXT_MAPCOMMAND).  For example, if the
 * user is entering the z command, a valid command is "z40+", and we don't want
 * to map the '+', i.e. if '+' is mapped to "xxx", we don't want to change it
 * into "z40xxx".  However, if the user enters "35x", we want to put all of the
 * characters through the mapping code.
 *
 * Historical practice is a bit muddled here.  (Surprise!)  It always permitted
 * mapping digits as long as they weren't the first character of the map, e.g.
 * ":map ^A1 xxx" was okay.  It also permitted the mapping of the digits 1-9
 * (the digit 0 was a special case as it doesn't indicate the start of a count)
 * as the first character of the map, but then ignored those mappings.  While
 * it's probably stupid to map digits, vi isn't your mother.
 *
 * The way this works is that the TXT_MAPNODIGIT causes term_key to return the
 * end-of-digit without "looking" at the next character, i.e. leaving it as the
 * user entered it.  Presumably, the next term_key call will tell us how the
 * user wants it handled.
 *
 * There is one more complication.  Users might map keys to digits, and, as
 * it's described above, the commands "map g 1G|d2g" would return the keys
 * "d2<end-of-digits>1G", when the user probably wanted "d21<end-of-digits>G".
 * So, if a map starts off with a digit we continue as before, otherwise, we
 * pretend that we haven't mapped the character and return <end-of-digits>.
 *
 * Now that that's out of the way, let's talk about Energizer Bunny macros.
 * It's easy to create macros that expand to a loop, e.g. map x 3x.  It's
 * fairly easy to detect this example, because it's all internal to term_key.
 * If we're expanding a macro and it gets big enough, at some point we can
 * assume it's looping and kill it.  The examples that are tough are the ones
 * where the parser is involved, e.g. map x "ayyx"byy.  We do an expansion
 * on 'x', and get "ayyx"byy.  We then return the first 4 characters, and then
 * find the looping macro again.  There is no way that we can detect this
 * without doing a full parse of the command, because the character that might
 * cause the loop (in this case 'x') may be a literal character, e.g. the map
 * map x "ayy"xyy"byy is perfectly legal and won't cause a loop.
 *
 * Historic vi tried to detect looping macros by disallowing obvious cases in
 * the map command, maps that that ended with the same letter as they started
 * (which wrongly disallowed "map x 'x"), and detecting macros that expanded
 * too many times before keys were returned to the command parser.  It didn't
 * get many (most?) of the tricky cases right, however, and it was certainly
 * possible to create macros that ran forever.  And, even if it did figure out
 * what was going on, the user was usually tossed into ex mode.  Finally, any
 * changes made before vi realized that the macro was recursing were left in
 * place.  This implementation counts how many times each input character has
 * been mapped.  If it reaches some arbitrary value, we flush all mapped keys
 * and return an error.
 *
 * XXX
 * The final issue is recovery.  It would be possible to undo all of the work
 * that was done by the macro if we entered a record into the log so that we
 * knew when the macro started, and, in fact, this might be worth doing at some
 * point.  Given that this might make the log grow unacceptably (consider that
 * cursor keys are done with maps), for now we leave any changes made in place.
 */
enum input
term_key(sp, chp, flags)
	SCR *sp;
	CH *chp;
	u_int flags;
{
	enum input rval;
	struct timeval t, *tp;
	CHAR_T ch;
	GS *gp;
	IBUF *tty;
	SEQ *qp;
	int cmap, ispartial, nr, itear;

	gp = sp->gp;
	tty = gp->tty;

	/*
	 * If the queue is empty, read more keys in.  Since no timeout is
	 * requested, s_key_read will either return an error or will read
	 * some number of characters.
	 */
loop:	if (tty->cnt == 0) {
		if (term_read_grow(sp, tty))
			return (INP_ERR);
		if (rval = sp->s_key_read(sp, &nr, NULL))
			return (rval);
		/*
		 * If there's something on the mode line that we wanted
		 * the user to see, they just entered a character so we
		 * can presume they saw it.
		 */
		if (F_ISSET(sp, S_UPDATE_MODE))
			F_CLR(sp, S_UPDATE_MODE);
	}

	/* If no limit on remaps, set it up so the user can interrupt. */
	itear = O_ISSET(sp, O_REMAPMAX) ? 0 : !intr_init(sp);

	/* If the key is mappable and should be mapped, look it up. */
	if (!(tty->chf[tty->next] & CH_NOMAP) &&
	    LF_ISSET(TXT_MAPCOMMAND | TXT_MAPINPUT)) {
		/* Set up timeout value. */
		if (O_ISSET(sp, O_TIMEOUT)) {
			tp = &t;
			t.tv_sec = O_VAL(sp, O_KEYTIME) / 10;
			t.tv_usec = (O_VAL(sp, O_KEYTIME) % 10) * 100000L;
		} else
			tp = NULL;

		/* Get the next key. */
newmap:		ch = tty->ch[tty->next];
		if (ch < MAX_BIT_SEQ && !bit_test(gp->seqb, ch))
			goto nomap;

		/* Search the map. */
remap:		qp = seq_find(sp, NULL, &tty->ch[tty->next], tty->cnt,
		    LF_ISSET(TXT_MAPCOMMAND) ? SEQ_COMMAND : SEQ_INPUT,
		    &ispartial);

		/*
		 * If get a partial match, read more characters and retry
		 * the map.  If no characters read, return the characters
		 * unmapped.
		 */
		if (ispartial) {
			if (term_read_grow(sp, tty)) {
				rval = INP_ERR;
				goto ret;
			}
			if (rval = sp->s_key_read(sp, &nr, tp))
				goto ret;
			if (nr)
				goto remap;
			goto nomap;
		}

		/* If no map, return the character. */
		if (qp == NULL)
			goto nomap;

		/*
		 * If looking for the end of a digit string, and the first
		 * character of the map is it, pretend we haven't seen the
		 * character.
		 */
		if (LF_ISSET(TXT_MAPNODIGIT) &&
		    qp->output != NULL && !isdigit(qp->output[0]))
			goto not_digit_ch;

		/*
		 * Only permit a character to be remapped a certain number
		 * of times before we figure that it's not going to finish.
		 */
		if (O_ISSET(sp, O_REMAPMAX)) {
			if ((cmap = tty->cmap[tty->next]) > MAX_MAP_COUNT)
				goto flush;
		} else if (F_ISSET(sp, S_INTERRUPTED)) {
flush:			term_map_flush(sp, "Character remapped too many times");
			rval = INP_ERR;
			goto ret;
		} else
			cmap = 0;

		/* Delete the mapped characters from the queue. */
		QREM_HEAD(tty, qp->ilen);

		/* If keys mapped to nothing, go get more. */
		if (qp->output == NULL)
			goto loop;

		/* If remapping characters, push the character on the queue. */
		if (O_ISSET(sp, O_REMAP)) {
			if (term_push(sp, qp->output, qp->olen, ++cmap, 0)) {
				rval = INP_ERR;
				goto ret;
			}
			goto newmap;
		}

		/* Else, push the characters on the queue and return one. */
		if (term_push(sp, qp->output, qp->olen, 0, CH_NOMAP)) {
			rval = INP_ERR;
			goto ret;
		}
	}

nomap:	ch = tty->ch[tty->next];
	if (LF_ISSET(TXT_MAPNODIGIT) && !isdigit(ch)) {
not_digit_ch:	chp->ch = NOT_DIGIT_CH;
		chp->value = 0;
		chp->flags = 0;
		rval = INP_OK;
		goto ret;
	}

	/* Fill in the return information. */
	chp->ch = ch;
	chp->flags = tty->chf[tty->next];
	chp->value = term_key_val(sp, ch);

	/* Delete the character from the queue. */
	QREM_HEAD(tty, 1);
	rval = INP_OK;

ret:	if (itear)
		intr_end(sp);
	return (rval);
}

/*
 * term_ab_flush --
 *	Flush any abbreviated keys.
 */
void
term_ab_flush(sp, msg)
	SCR *sp;
	char *msg;
{
	IBUF *tty;

	tty = sp->gp->tty;
	if (!tty->cnt || !(tty->chf[tty->next] & CH_ABBREVIATED))
		return;
	do {
		QREM_HEAD(tty, 1);
	} while (tty->cnt && tty->chf[tty->next] & CH_ABBREVIATED);
	msgq(sp, M_ERR, "%s: keys flushed.", msg);
}

/*
 * term_map_flush --
 *	Flush any mapped keys.
 */
void
term_map_flush(sp, msg)
	SCR *sp;
	char *msg;
{
	IBUF *tty;

	tty = sp->gp->tty;
	if (!tty->cnt || !tty->cmap[tty->next])
		return;
	do {
		QREM_HEAD(tty, 1);
	} while (tty->cnt && tty->cmap[tty->next]);
	msgq(sp, M_ERR, "%s: keys flushed.", msg);
}

/*
 * term_user_key --
 *	Get the next key, but require the user enter one.
 */
enum input
term_user_key(sp, chp)
	SCR *sp;
	CH *chp;
{
	enum input rval;
	IBUF *tty;
	int nr;

	/*
	 * Read any keys the user has waiting.  Make the race
	 * condition as short as possible.
	 */
	if (rval = term_key_queue(sp))
		return (rval);

	/* Wait and read another key. */
	if (rval = sp->s_key_read(sp, &nr, NULL))
		return (rval);

	/* Fill in the return information. */
	tty = sp->gp->tty;
	chp->ch = tty->ch[tty->next + (tty->cnt - 1)];
	chp->flags = 0;
	chp->value = term_key_val(sp, chp->ch);

	QREM_TAIL(tty, 1);
	return (INP_OK);
}

/*
 * term_key_queue --
 *	Read the keys off of the terminal queue until it's empty.
 */
int
term_key_queue(sp)
	SCR *sp;
{
	enum input rval;
	struct timeval t;
	IBUF *tty;
	int nr;

	t.tv_sec = 0;
	t.tv_usec = 0;
	for (tty = sp->gp->tty;;) {
		if (term_read_grow(sp, tty))
			return (INP_ERR);
		if (rval = sp->s_key_read(sp, &nr, &t))
			return (rval);
		if (nr == 0)
			break;
	}
	return (INP_OK);
}

/*
 * __term_key_val --
 *	Fill in the value for a key.  This routine is the backup
 *	for the term_key_val() macro.
 */
int
__term_key_val(sp, ch)
	SCR *sp;
	ARG_CHAR_T ch;
{
	KEYLIST k, *kp;

	k.ch = ch;
	kp = bsearch(&k, keylist, nkeylist, sizeof(keylist[0]), keycmp);
	return (kp == NULL ? K_NOTUSED : kp->value);
}

/*
 * __term_read_grow --
 *	Grow the terminal queue.  This routine is the backup for
 *	the term_read_grow() macro.
 */
static int
__term_read_grow(sp, tty, add)
	SCR *sp;
	IBUF *tty;
	int add;
{
	size_t new_nelem, olen;

	new_nelem = tty->nelem + add;
	olen = tty->nelem * sizeof(tty->ch[0]);
	BINC_RET(sp, tty->ch, olen, new_nelem * sizeof(tty->ch[0]));

	olen = tty->nelem * sizeof(tty->chf[0]);
	BINC_RET(sp, tty->chf, olen, new_nelem * sizeof(tty->chf[0]));

	olen = tty->nelem * sizeof(tty->cmap[0]);
	BINC_RET(sp, tty->cmap, olen, new_nelem * sizeof(tty->cmap[0]));

	tty->nelem = new_nelem;
	return (0);
}

static int
keycmp(ap, bp)
	const void *ap, *bp;
{
	return (((KEYLIST *)ap)->ch - ((KEYLIST *)bp)->ch);
}
