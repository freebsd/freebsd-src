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
static const char sccsid[] = "@(#)term.c	8.80 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
/*
 * XXX
 * DON'T INCLUDE <curses.h> HERE, IT BREAKS OSF1 V2.0 WHERE IT
 * CHANGES THE VALUES OF VERASE/VKILL/VWERASE TO INCORRECT ONES.
 */
#include <db.h>
#include <regex.h>

#include "vi.h"

static int	  keycmp __P((const void *, const void *));
static enum input term_key_queue __P((SCR *));
static void	  term_key_set __P((GS *, int, int));

/*
 * If we're reading less than 20 characters, up the size of the tty buffer.
 * This shouldn't ever happen, other than the first time through, but it's
 * possible if a map is large enough.
 */
#define	term_read_grow(sp, tty)					\
	(tty)->nelem - ((tty)->cnt + (tty)->next) >= 20 ?	\
	0 : __term_read_grow(sp, tty, 64)
static int __term_read_grow __P((SCR *, IBUF *, int));

/*
 * !!!
 * Historic vi always used:
 *
 *	^D: autoindent deletion
 *	^H: last character deletion
 *	^W: last word deletion
 *	^Q: quote the next character (if not used in flow control).
 *	^V: quote the next character
 *
 * regardless of the user's choices for these characters.  The user's erase
 * and kill characters worked in addition to these characters.  Nvi wires
 * down the above characters, but in addition permits the VEOF, VERASE, VKILL
 * and VWERASE characters described by the user's termios structure.
 *
 * Ex was not consistent with this scheme, as it historically ran in tty
 * cooked mode.  This meant that the scroll command and autoindent erase
 * characters were mapped to the user's EOF character, and the character
 * and word deletion characters were the user's tty character and word
 * deletion characters.  This implementation makes it all consistent, as
 * described above for vi.
 *
 * XXX
 * THIS REQUIRES THAT ALL SCREENS SHARE A SPECIAL KEY SET.
 */
KEYLIST keylist[] = {
	{K_CARAT,	   '^'},	/*  ^ */
	{K_CNTRLD,	'\004'},	/* ^D */
	{K_CNTRLR,	'\022'},	/* ^R */
	{K_CNTRLT,	'\024'},	/* ^T */
	{K_CNTRLZ,	'\032'},	/* ^Z */
	{K_COLON,	   ':'},	/*  : */
	{K_CR,		  '\r'},	/* \r */
	{K_ESCAPE,	'\033'},	/* ^[ */
	{K_FORMFEED,	  '\f'},	/* \f */
	{K_HEXCHAR,	'\030'},	/* ^X */
	{K_NL,		  '\n'},	/* \n */
	{K_RIGHTBRACE,	   '}'},	/*  } */
	{K_RIGHTPAREN,	   ')'},	/*  ) */
	{K_TAB,		  '\t'},	/* \t */
	{K_VERASE,	  '\b'},	/* \b */
	{K_VKILL,	'\025'},	/* ^U */
	{K_VLNEXT,	'\021'},	/* ^Q */
	{K_VLNEXT,	'\026'},	/* ^V */
	{K_VWERASE,	'\027'},	/* ^W */
	{K_ZERO,	   '0'},	/*  0 */
	{K_NOTUSED, 0},			/* VEOF, VERASE, VKILL, VWERASE */
	{K_NOTUSED, 0},
	{K_NOTUSED, 0},
	{K_NOTUSED, 0},
};
static int nkeylist = (sizeof(keylist) / sizeof(keylist[0])) - 4;

/*
 * term_init --
 *	Initialize the special key lookup table.
 */
int
term_init(sp)
	SCR *sp;
{
	GS *gp;
	KEYLIST *kp;
	int cnt;

	/*
	 * XXX
	 * 8-bit only, for now.  Recompilation should get you any
	 * 8-bit character set, as long as nul isn't a character.
	 */
	(void)setlocale(LC_ALL, "");
	key_init(sp);

	gp = sp->gp;
#ifdef VEOF
	term_key_set(gp, VEOF, K_CNTRLD);
#endif
#ifdef VERASE
	term_key_set(gp, VERASE, K_VERASE);
#endif
#ifdef VKILL
	term_key_set(gp, VKILL, K_VKILL);
#endif
#ifdef VWERASE
	term_key_set(gp, VWERASE, K_VWERASE);
#endif

	/* Sort the special key list. */
	qsort(keylist, nkeylist, sizeof(keylist[0]), keycmp);

	/* Initialize the fast lookup table. */
	for (gp->max_special = 0, kp = keylist, cnt = nkeylist; cnt--; ++kp) {
		if (gp->max_special < kp->value)
			gp->max_special = kp->value;
		if (kp->ch <= MAX_FAST_KEY)
			gp->special_key[kp->ch] = kp->value;
	}
	return (0);
}

/*
 * term_key_set --
 *	Set keys found in the termios structure.  VERASE and VKILL are required
 *	by POSIX 1003.1-1990, VWERASE is a 4.4BSD extension.  We've left three
 *	open slots in the keylist table, if these values exist, put them into
 *	place.  Note, they may reset (or duplicate) values already in the table,
 *	so we check for that first.
 */
static void
term_key_set(gp, name, val)
	GS *gp;
	int name, val;
{
	KEYLIST *kp;
	cc_t ch;

	if (!F_ISSET(gp, G_TERMIOS_SET))
		return;
	if ((ch = gp->original_termios.c_cc[name]) == _POSIX_VDISABLE)
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
 * key_init --
 *	Build the fast-lookup key display array.
 */
void
key_init(sp)
	SCR *sp;
{
	CHAR_T ch;

	for (ch = 0; ch <= MAX_FAST_KEY; ++ch) {
		(void)__key_name(sp, ch);
		(void)memmove(sp->gp->cname[ch].name, sp->cname, sp->clen);
		sp->gp->cname[ch].len = sp->clen;
	}
}

/*
 * __key_len --
 *	Return the length of the string that will display the key.
 *	This routine is the backup for the KEY_LEN() macro.
 */
size_t
__key_len(sp, ch)
	SCR *sp;
	ARG_CHAR_T ch;
{
	(void)__key_name(sp, ch);
	return (sp->clen);
}

/*
 * __key_name --
 *	Return the string that will display the key.  This routine
 *	is the backup for the KEY_NAME() macro.
 */
CHAR_T *
__key_name(sp, ach)
	SCR *sp;
	ARG_CHAR_T ach;
{
	static const CHAR_T hexdigit[] = "0123456789abcdef";
	static const CHAR_T octdigit[] = "01234567";
	CHAR_T ch, *chp, mask;
	size_t len;
	int cnt, shift;

	/*
	 * Historical (ARPA standard) mappings.  Printable characters are left
	 * alone.  Control characters less than '\177' are represented as '^'
	 * followed by the character offset from the '@' character in the ASCII
	 * map.  '\177' is represented as '^' followed by '?'.
	 *
	 * XXX
	 * The following code depends on the current locale being identical to
	 * the ASCII map from '\100' to '\076' (\076 since that's the largest
	 * character for which we can offset from '@' and get something that's
	 * a printable character in ASCII.  I'm told that this is a reasonable
	 * assumption...
	 *
	 * XXX
	 * This code will only work with CHAR_T's that are multiples of 8-bit
	 * bytes.
	 *
	 * XXX
	 * NB: There's an assumption here that all printable characters take
	 * up a single column on the screen.  This is not always correct.
	 */
	ch = ach;
	if (isprint(ch)) {
		sp->cname[0] = ch;
		len = 1;
	} else if (ch <= '\076' && iscntrl(ch)) {
		sp->cname[0] = '^';
		sp->cname[1] = ch == '\177' ? '?' : '@' + ch;
		len = 2;
	} else if (O_ISSET(sp, O_OCTAL)) {
#define	BITS	(sizeof(CHAR_T) * 8)
#define	SHIFT	(BITS - BITS % 3)
#define	TOPMASK	(BITS % 3 == 2 ? 3 : 1) << (BITS - BITS % 3)
		sp->cname[0] = '\\';
		sp->cname[1] = octdigit[(ch & TOPMASK) >> SHIFT];
		shift = SHIFT - 3;
		for (len = 2, mask = 7 << (SHIFT - 3),
		    cnt = BITS / 3; cnt-- > 0; mask >>= 3, shift -= 3)
			sp->cname[len++] = octdigit[(ch & mask) >> shift];
	} else {
		sp->cname[0] = '0';
		sp->cname[1] = 'x';
		for (len = 2, chp = (u_int8_t *)&ch,
		    cnt = sizeof(CHAR_T); cnt-- > 0; ++chp) {
			sp->cname[len++] = hexdigit[(*chp & 0xf0) >> 4];
			sp->cname[len++] = hexdigit[*chp & 0x0f];
		}
	}
	sp->cname[sp->clen = len] = '\0';
	return (sp->cname);
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
term_push(sp, s, nchars, flags)
	SCR *sp;
	CHAR_T *s;			/* Characters. */
	size_t nchars;			/* Number of chars. */
	u_int flags;			/* CH_* flags. */
{
	IBUF *tty;
	size_t total;
	
	/* If we have room, stuff the keys into the buffer. */
	tty = sp->gp->tty;
	if (nchars <= tty->next ||
	    (tty->ch != NULL && tty->cnt == 0 && nchars <= tty->nelem)) {
		if (tty->cnt != 0)
			tty->next -= nchars;
		tty->cnt += nchars;
		MEMMOVE(tty->ch + tty->next, s, nchars);
		MEMSET(tty->chf + tty->next, flags, nchars);
		return (0);
	}

	/*
	 * If there are currently characters in the queue, shift them up,
	 * leaving some extra room.  Get enough space plus a little extra.
	 */
#define	TERM_PUSH_SHIFT	30
	total = tty->cnt + tty->next + nchars + TERM_PUSH_SHIFT;
	if (total >= tty->nelem && __term_read_grow(sp, tty, MAX(total, 64)))
		return (1);
	if (tty->cnt) {
		MEMMOVE(tty->ch + TERM_PUSH_SHIFT + nchars,
		    tty->ch + tty->next, tty->cnt);
		MEMMOVE(tty->chf + TERM_PUSH_SHIFT + nchars,
		    tty->chf + tty->next, tty->cnt);
	}

	/* Put the new characters into the queue. */
	tty->next = TERM_PUSH_SHIFT;
	tty->cnt += nchars;
	MEMMOVE(tty->ch + TERM_PUSH_SHIFT, s, nchars);
	MEMSET(tty->chf + TERM_PUSH_SHIFT, flags, nchars);
	return (0);
}

/*
 * Remove characters from the queue, simultaneously clearing the flag
 * and map counts.
 */
#define	QREM_HEAD(q, len) {						\
	size_t __off = (q)->next;					\
	if (len == 1)							\
		tty->chf[__off] = 0;					\
	else								\
		MEMSET(tty->chf + __off, 0, len);			\
	if (((q)->cnt -= len) == 0)					\
		(q)->next = 0;						\
	else								\
		(q)->next += len;					\
}
#define	QREM_TAIL(q, len) {						\
	size_t __off = (q)->next + (q)->cnt - 1;			\
	if (len == 1)							\
		tty->chf[__off] = 0;					\
	else								\
		MEMSET(tty->chf + __off, 0, len);			\
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
 * place.  We recover gracefully, but the only recourse the user has in an
 * infinite macro loop is to interrupt.
 *
 * !!!
 * It is historic practice that mapping characters to themselves as the first
 * part of the mapped string was legal, and did not cause infinite loops, i.e.
 * ":map! { {^M^T" and ":map n nz." were known to work.  The initial, matching
 * characters were returned instead of being remapped.
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
	int init_nomap, ispartial, nr;

	/* If we've been interrupted, return an error. */
	if (INTERRUPTED(sp))
		return (INP_INTR);

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
		if ((rval = sp->s_key_read(sp, &nr, NULL)) != INP_OK)
			return (rval);
		/*
		 * If there's something on the mode line that we wanted
		 * the user to see, they just entered a character so we
		 * can presume they saw it.
		 */
		if (F_ISSET(sp, S_UPDATE_MODE))
			F_CLR(sp, S_UPDATE_MODE);
	}

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

		/* If we've been interrupted, return an error. */
		if (INTERRUPTED(sp))
			return (INP_INTR);

		/*
		 * If get a partial match, read more characters and retry
		 * the map.  If no characters read, return the characters
		 * unmapped.
		 */
		if (ispartial) {
			if (term_read_grow(sp, tty))
				return (INP_ERR);
			if ((rval = sp->s_key_read(sp, &nr, tp)) != INP_OK)
				return (rval);
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

		/* Find out if the initial segments are identical. */
		init_nomap = !memcmp(&tty->ch[tty->next], qp->output, qp->ilen);

		/* Delete the mapped characters from the queue. */
		QREM_HEAD(tty, qp->ilen);

		/* If keys mapped to nothing, go get more. */
		if (qp->output == NULL)
			goto loop;

		/* If remapping characters, push the character on the queue. */
		if (O_ISSET(sp, O_REMAP)) { 
			if (init_nomap) {
				if (term_push(sp, qp->output + qp->ilen,
				    qp->olen - qp->ilen, CH_MAPPED))
					return (INP_ERR);
				if (term_push(sp,
				    qp->output, qp->ilen, CH_NOMAP | CH_MAPPED))
					return (INP_ERR);
				goto nomap;
			} else
				if (term_push(sp,
				    qp->output, qp->olen, CH_MAPPED))
					return (INP_ERR);
			goto newmap;
		}

		/* Else, push the characters on the queue and return one. */
		if (term_push(sp, qp->output, qp->olen, CH_MAPPED | CH_NOMAP))
			return (INP_ERR);
	}

nomap:	ch = tty->ch[tty->next];
	if (LF_ISSET(TXT_MAPNODIGIT) && !isdigit(ch)) {
not_digit_ch:	chp->ch = CH_NOT_DIGIT;
		chp->value = 0;
		chp->flags = 0;
		return (INP_OK);
	}

	/* Fill in the return information. */
	chp->ch = ch;
	chp->flags = tty->chf[tty->next];
	chp->value = KEY_VAL(sp, ch);

	/* Delete the character from the queue. */
	QREM_HEAD(tty, 1);
	return (INP_OK);
}

/*
 * term_flush --
 *	Flush any flagged keys.
 */
void
term_flush(sp, msg, flags)
	SCR *sp;
	char *msg;
	u_int flags;
{
	IBUF *tty;

	tty = sp->gp->tty;
	if (!tty->cnt || !(tty->chf[tty->next] & flags))
		return;
	do {
		QREM_HEAD(tty, 1);
	} while (tty->cnt && tty->chf[tty->next] & flags);
	msgq(sp, M_ERR, "%s: keys flushed", msg);
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
	if ((rval = term_key_queue(sp)) != INP_OK)
		return (rval);

	/* Wait and read another key. */
	if ((rval = sp->s_key_read(sp, &nr, NULL)) != INP_OK)
		return (rval);

	/* Fill in the return information. */
	tty = sp->gp->tty;
	chp->ch = tty->ch[tty->next + (tty->cnt - 1)];
	chp->flags = 0;
	chp->value = KEY_VAL(sp, chp->ch);

	QREM_TAIL(tty, 1);
	return (INP_OK);
}

/*
 * term_key_queue --
 *	Read the keys off of the terminal queue until it's empty.
 */
static enum input
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
		if ((rval = sp->s_key_read(sp, &nr, &t)) != INP_OK)
			return (rval);
		if (nr == 0)
			break;
	}
	return (INP_OK);
}

/*
 * __key_val --
 *	Fill in the value for a key.  This routine is the backup
 *	for the KEY_VAL() macro.
 */
int
__key_val(sp, ch)
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

	tty->nelem = olen / sizeof(tty->chf[0]);
	return (0);
}

static int
keycmp(ap, bp)
	const void *ap, *bp;
{
	return (((KEYLIST *)ap)->ch - ((KEYLIST *)bp)->ch);
}
