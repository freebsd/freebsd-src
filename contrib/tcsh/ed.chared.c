/* $Header: /src/pub/tcsh/ed.chared.c,v 3.71 2002/03/08 17:36:45 christos Exp $ */
/*
 * ed.chared.c: Character editing functions.
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
/*
  Bjorn Knutsson @ Thu Jun 24 19:02:17 1999

  e_dabbrev_expand() did not do proper completion if quoted spaces were present
  in the string being completed. Exemple:

  # echo hello\ world
  hello world
  # echo h<press key bound to dabbrev-expande>
  # echo hello\<cursor>

  Correct behavior is:
  # echo h<press key bound to dabbrev-expande>
  # echo hello\ world<cursor>

  The same problem occured if spaces were present in a string withing quotation
  marks. Example:

  # echo "hello world"
  hello world
  # echo "h<press key bound to dabbrev-expande>
  # echo "hello<cursor>
  
  The former problem could be solved with minor modifications of c_preword()
  and c_endword(). The latter, however, required a significant rewrite of
  c_preword(), since quoted strings must be parsed from start to end to
  determine if a given character is inside or outside the quotation marks.

  Compare the following two strings:

  # echo \"" 'foo \' bar\"
  " 'foo \' bar\
  # echo '\"" 'foo \' bar\"
  \"" foo ' bar"

  The only difference between the two echo lines is in the first character
  after the echo command. The result is either one or three arguments.

 */

#include "sh.h"

RCSID("$Id: ed.chared.c,v 3.71 2002/03/08 17:36:45 christos Exp $")

#include "ed.h"
#include "tw.h"
#include "ed.defns.h"

/* #define SDEBUG */

#define TCSHOP_NOP    	  0x00
#define TCSHOP_DELETE 	  0x01
#define TCSHOP_INSERT 	  0x02
#define TCSHOP_CHANGE 	  0x04

#define CHAR_FWD	0
#define CHAR_BACK	1

/*
 * vi word treatment
 * from: Gert-Jan Vons <vons@cesar.crbca1.sinet.slb.com>
 */
#define C_CLASS_WHITE	1
#define C_CLASS_ALNUM	2
#define C_CLASS_OTHER	3

static Char *InsertPos = InputBuf; /* Where insertion starts */
static Char *ActionPos = 0;	   /* Where action begins  */
static int  ActionFlag = TCSHOP_NOP;	   /* What delayed action to take */
/*
 * Word search state
 */
static int  searchdir = F_UP_SEARCH_HIST; 	/* Direction of last search */
static Char patbuf[INBUFSIZE];			/* Search target */
static int patlen = 0;
/*
 * Char search state
 */
static int  srch_dir = CHAR_FWD;		/* Direction of last search */
static Char srch_char = 0;			/* Search target */

/* all routines that start with c_ are private to this set of routines */
static	void	 c_alternativ_key_map	__P((int));
void	 c_insert		__P((int));
void	 c_delafter		__P((int));
void	 c_delbefore		__P((int));
static 	int	 c_to_class		__P((int));
static	Char	*c_prev_word		__P((Char *, Char *, int));
static	Char	*c_next_word		__P((Char *, Char *, int));
static	Char	*c_number		__P((Char *, int *, int));
static	Char	*c_expand		__P((Char *));
static	void	 c_excl			__P((Char *));
static	void	 c_substitute		__P((void));
static	void	 c_delfini		__P((void));
static	int	 c_hmatch		__P((Char *));
static	void	 c_hsetpat		__P((void));
#ifdef COMMENT
static	void	 c_get_word		__P((Char **, Char **));
#endif
static	Char	*c_preword		__P((Char *, Char *, int, Char *));
static	Char	*c_nexword		__P((Char *, Char *, int));
static	Char	*c_endword		__P((Char *, Char *, int, Char *));
static	Char	*c_eword		__P((Char *, Char *, int));
static	void	 c_push_kill		__P((Char *, Char *));
static  CCRETVAL c_get_histline		__P((void));
static  CCRETVAL c_search_line		__P((Char *, int));
static  CCRETVAL v_repeat_srch		__P((int));
static	CCRETVAL e_inc_search		__P((int));
static	CCRETVAL v_search		__P((int));
static	CCRETVAL v_csearch_fwd		__P((int, int, int));
static	CCRETVAL v_action		__P((int));
static	CCRETVAL v_csearch_back		__P((int, int, int));

#if defined(DSPMBYTE)
static	void 	 e_charfwd_mbyte	__P((int));
static	void 	 e_charback_mbyte	__P((int));
static  int extdel;
static  int extins = 0;
#endif

static void
c_alternativ_key_map(state)
    int     state;
{
    switch (state) {
    case 0:
	CurrentKeyMap = CcKeyMap;
	break;
    case 1:
	CurrentKeyMap = CcAltMap;
	break;
    default:
	return;
    }

    AltKeyMap = (Char) state;
}

void
c_insert(num)
    int num;
{
    Char *cp;

    if (LastChar + num >= InputLim)
	return;			/* can't go past end of buffer */

    if (Cursor < LastChar) {	/* if I must move chars */
	for (cp = LastChar; cp >= Cursor; cp--)
	    cp[num] = *cp;
    }
    LastChar += num;
}

void
c_delafter(num)	
    int num;
{
    Char *cp, *kp = NULL;

#if defined(DSPMBYTE)
    Char *wkcp;

    extdel = 0;
#endif

    if (num > LastChar - Cursor)
	num = (int) (LastChar - Cursor);	/* bounds check */

    if (num > 0) {			/* if I can delete anything */
#if defined(DSPMBYTE)
	/* check for code of deleted character */
	if (_enable_mbdisp) {
	    for (wkcp = Cursor ; wkcp < Cursor + num; wkcp++) {
		if (extdel == 0)
		    extdel = Ismbyte1(*wkcp); /* check to 1st. byte */
		else
		    extdel = 0;	/* if 2nd. byte, force set to 0 */
	    }
	}
#endif
	if (VImode) {
	    kp = UndoBuf;		/* Set Up for VI undo command */
	    UndoAction = TCSHOP_INSERT;
	    UndoSize = num;
	    UndoPtr  = Cursor;
	    for (cp = Cursor; cp <= LastChar; cp++) {
		*kp++ = *cp;	/* Save deleted chars into undobuf */
		*cp = cp[num];
	    }
	}
	else
	    for (cp = Cursor; cp <= LastChar; cp++)
		*cp = cp[num];
	LastChar -= num;
#if defined(DSPMBYTE)
	if (_enable_mbdisp && extdel && Ismbyte2(*Cursor)) {
	    if( VImode ) {
		UndoSize++;
		*kp++ = *Cursor; /* Save deleted chars into undobuf */
	    }
	    for (cp = Cursor; cp <= LastChar; cp++)
		*cp = cp[1];
	    LastChar--;
	    e_redisp( 1 );
	}
	else
	    extdel = 0;
#endif
    }
#ifdef notdef
    else {
	/* 
	 * XXX: We don't want to do that. In emacs mode overwrite should be
	 * sticky. I am not sure how that affects vi mode 
	 */
	inputmode = MODE_INSERT;
    }
#endif /* notdef */
}

void
c_delbefore(num)		/* delete before dot, with bounds checking */
    int num;
{
    Char *cp, *kp = NULL;

#if defined(DSPMBYTE)
    Char *nowcur, *wkcp;
    Char delc;

    extdel = 0;
#endif

    if (num > Cursor - InputBuf)
	num = (int) (Cursor - InputBuf);	/* bounds check */

    if (num > 0) {			/* if I can delete anything */
#if defined(DSPMBYTE)
	nowcur = Cursor - num;
	delc = *nowcur;
#endif
	if (VImode) {
	    kp = UndoBuf;		/* Set Up for VI undo command */
	    UndoAction = TCSHOP_INSERT;
	    UndoSize = num;
	    UndoPtr  = Cursor - num;
	    for (cp = Cursor - num; cp <= LastChar; cp++) {
		*kp++ = *cp;
		*cp = cp[num];
	    }
	}
	else
	    for (cp = Cursor - num; cp <= LastChar; cp++)
		*cp = cp[num];
	LastChar -= num;
#if defined(DSPMBYTE)
	if (_enable_mbdisp) {
	    for (wkcp = InputBuf; wkcp < nowcur; wkcp++) {
		if(extdel == 0)
		    extdel = Ismbyte1(*wkcp); /* check to 1st. byte */
		else
		    extdel = 0;	/* if 2nd. byte, force set to 0 */
	    }
	    if (extdel && Ismbyte2(delc)) {
		if( VImode ) {
		    UndoSize++;
		    UndoPtr--;
		    *kp++ = *(nowcur-1);
				/* Save deleted chars into undobuf */
		}
		for (cp = nowcur - 1; cp <= LastChar; cp++)
		    *cp = cp[1];
		LastChar--;
	    }
	}
	else
	    extdel = 0;
#endif
    }
}

static Char *
c_preword(p, low, n, delim)
    Char *p, *low, *delim;
    int n;
{
  while (n--) {
    Char *prev = low;
    Char *new;

    while (prev < p) {		/* Skip initial non-word chars */
      if (!Strchr(delim, *prev) || *(prev-1) == (Char)'\\')
	break;
      prev++;
    }

    new = prev;

    while (new < p) {
      prev = new;
      new = c_endword(prev-1, p, 1, delim); /* Skip to next non-word char */
      new++;			/* Step away from end of word */
      while (new <= p) {	/* Skip trailing non-word chars */
	if (!Strchr(delim, *new) || *(new-1) == (Char)'\\')
	  break;
	new++;
      }
    }

    p = prev;			/* Set to previous word start */

  }
  if (p < low)
    p = low;
  return (p);
}

/*
 * c_to_class() returns the class of the given character.
 *
 * This is used to make the c_prev_word() and c_next_word() functions
 * work like vi's, which classify characters. A word is a sequence of
 * characters belonging to the same class, classes being defined as
 * follows:
 *
 *	1/ whitespace
 *	2/ alphanumeric chars, + underscore
 *	3/ others
 */
static int
c_to_class(ch)
int  ch;
{
    if (Isspace(ch))
        return C_CLASS_WHITE;

    if (Isdigit(ch) || Isalpha(ch) || ch == '_')
        return C_CLASS_ALNUM;

    return C_CLASS_OTHER;
}

static Char *
c_prev_word(p, low, n)
    Char *p, *low;
    int n;
{
    p--;

    if (!VImode) {
	while (n--) {
	    while ((p >= low) && !isword(*p)) 
		p--;
	    while ((p >= low) && isword(*p)) 
		p--;
	}
      
	/* cp now points to one character before the word */
	p++;
	if (p < low)
	    p = low;
	/* cp now points where we want it */
	return(p);
    }
  
    while (n--) {
        int  c_class;

        if (p < low)
            break;

        /* scan until beginning of current word (may be all whitespace!) */
        c_class = c_to_class(*p);
        while ((p >= low) && c_class == c_to_class(*p))
            p--;

        /* if this was a non_whitespace word, we're ready */
        if (c_class != C_CLASS_WHITE)
            continue;

        /* otherwise, move back to beginning of the word just found */
        c_class = c_to_class(*p);
        while ((p >= low) && c_class == c_to_class(*p))
            p--;
    }

    p++;                        /* correct overshoot */

    return (p);
}

static Char *
c_next_word(p, high, n)
    Char *p, *high;
    int n;
{
    if (!VImode) {
	while (n--) {
	    while ((p < high) && !isword(*p)) 
		p++;
	    while ((p < high) && isword(*p)) 
		p++;
	}
	if (p > high)
	    p = high;
	/* p now points where we want it */
	return(p);
    }

    while (n--) {
        int  c_class;

        if (p >= high)
            break;

        /* scan until end of current word (may be all whitespace!) */
        c_class = c_to_class(*p);
        while ((p < high) && c_class == c_to_class(*p))
            p++;

        /* if this was all whitespace, we're ready */
        if (c_class == C_CLASS_WHITE)
            continue;

	/* if we've found white-space at the end of the word, skip it */
        while ((p < high) && c_to_class(*p) == C_CLASS_WHITE)
            p++;
    }

    p--;                        /* correct overshoot */

    return (p);
}

static Char *
c_nexword(p, high, n)
    Char *p, *high;
    int n;
{
    while (n--) {
	while ((p < high) && !Isspace(*p)) 
	    p++;
	while ((p < high) && Isspace(*p)) 
	    p++;
    }

    if (p > high)
	p = high;
    /* p now points where we want it */
    return(p);
}

/*
 * Expand-History (originally "Magic-Space") code added by
 * Ray Moody <ray@gibbs.physics.purdue.edu>
 * this is a neat, but odd, addition.
 */

/*
 * c_number: Ignore character p points to, return number appearing after that.
 * A '$' by itself means a big number; "$-" is for negative; '^' means 1.
 * Return p pointing to last char used.
 */

/*
 * dval is the number to subtract from for things like $-3
 */

static Char *
c_number(p, num, dval)
    Char *p;
    int *num;
    int dval;
{
    int i;
    int sign = 1;

    if (*++p == '^') {
	*num = 1;
	return(p);
    }
    if (*p == '$') {
	if (*++p != '-') {
	    *num = NCARGS;	/* Handle $ */
	    return(--p);
	}
	sign = -1;		/* Handle $- */
	++p;
    }
    for (i = 0; *p >= '0' && *p <= '9'; i = 10 * i + *p++ - '0')
	continue;
    *num = (sign < 0 ? dval - i : i);
    return(--p);
}

/*
 * excl_expand: There is an excl to be expanded to p -- do the right thing
 * with it and return a version of p advanced over the expanded stuff.  Also,
 * update tsh_cur and related things as appropriate...
 */

static Char *
c_expand(p)
    Char *p;
{
    Char *q;
    struct Hist *h = Histlist.Hnext;
    struct wordent *l;
    int     i, from, to, dval;
    bool    all_dig;
    bool    been_once = 0;
    Char   *op = p;
    Char    buf[INBUFSIZE];
    Char   *bend = buf;
    Char   *modbuf, *omodbuf;

    if (!h)
	goto excl_err;
excl_sw:
    switch (*(q = p + 1)) {

    case '^':
	bend = expand_lex(buf, INBUFSIZE, &h->Hlex, 1, 1);
	break;

    case '$':
	if ((l = (h->Hlex).prev) != 0)
	    bend = expand_lex(buf, INBUFSIZE, l->prev->prev, 0, 0);
	break;

    case '*':
	bend = expand_lex(buf, INBUFSIZE, &h->Hlex, 1, NCARGS);
	break;

    default:
	if (been_once) {	/* unknown argument */
	    /* assume it's a modifier, e.g. !foo:h, and get whole cmd */
	    bend = expand_lex(buf, INBUFSIZE, &h->Hlex, 0, NCARGS);
	    q -= 2;
	    break;
	}
	been_once = 1;

	if (*q == ':')		/* short form: !:arg */
	    --q;

	if (*q != HIST) {
	    /*
	     * Search for a space, tab, or colon.  See if we have a number (as
	     * in !1234:xyz).  Remember the number.
	     */
	    for (i = 0, all_dig = 1; 
		 *q != ' ' && *q != '\t' && *q != ':' && q < Cursor; q++) {
		/*
		 * PWP: !-4 is a valid history argument too, therefore the test
		 * is if not a digit, or not a - as the first character.
		 */
		if ((*q < '0' || *q > '9') && (*q != '-' || q != p + 1))
		    all_dig = 0;
		else if (*q == '-')
		    all_dig = 2;/* we are sneeky about this */
		else
		    i = 10 * i + *q - '0';
	    }
	    --q;

	    /*
	     * If we have a number, search for event i.  Otherwise, search for
	     * a named event (as in !foo).  (In this case, I is the length of
	     * the named event).
	     */
	    if (all_dig) {
		if (all_dig == 2)
		    i = -i;	/* make it negitive */
		if (i < 0)	/* if !-4 (for example) */
		    i = eventno + 1 + i;	/* remember: i is < 0 */
		for (; h; h = h->Hnext) {
		    if (h->Hnum == i)
			break;
		}
	    }
	    else {
		for (i = (int) (q - p); h; h = h->Hnext) {
		    if ((l = &h->Hlex) != 0) {
			if (!Strncmp(p + 1, l->next->word, (size_t) i))
			    break;
		    }
		}
	    }
	}
	if (!h)
	    goto excl_err;
	if (q[1] == ':' || q[1] == '-' || q[1] == '*' ||
	    q[1] == '$' || q[1] == '^') {	/* get some args */
	    p = q[1] == ':' ? ++q : q;
	    /*
	     * Go handle !foo:*
	     */
	    if ((q[1] < '0' || q[1] > '9') &&
		q[1] != '-' && q[1] != '$' && q[1] != '^')
		goto excl_sw;
	    /*
	     * Go handle !foo:$
	     */
	    if (q[1] == '$' && (q[2] != '-' || q[3] < '0' || q[3] > '9'))
		goto excl_sw;
	    /*
	     * Count up the number of words in this event.  Store it in dval.
	     * Dval will be fed to number.
	     */
	    dval = 0;
	    if ((l = h->Hlex.prev) != 0) {
		for (l = l->prev; l != h->Hlex.next; l = l->prev, dval++)
		    continue;
	    }
	    if (!dval)
		goto excl_err;
	    if (q[1] == '-')
		from = 0;
	    else
		q = c_number(q, &from, dval);
	    if (q[1] == '-') {
		++q;
		if ((q[1] < '0' || q[1] > '9') && q[1] != '$')
		    to = dval - 1;
		else
		    q = c_number(q, &to, dval);
	    }
	    else if (q[1] == '*') {
		++q;
		to = NCARGS;
	    }
	    else {
		to = from;
	    }
	    if (from < 0 || to < from)
		goto excl_err;
	    bend = expand_lex(buf, INBUFSIZE, &h->Hlex, from, to);
	}
	else {			/* get whole cmd */
	    bend = expand_lex(buf, INBUFSIZE, &h->Hlex, 0, NCARGS);
	}
	break;
    }

    /*
     * Apply modifiers, if any.
     */
    if (q[1] == ':') {
	*bend = '\0';
	modbuf = omodbuf = buf;
	while (q[1] == ':' && modbuf != NULL) {
	    switch (q[2]) {
	    case 'r':
	    case 'e':
	    case 'h':
	    case 't':
	    case 'q':
	    case 'x':
	    case 'u':
	    case 'l':
		if ((modbuf = domod(omodbuf, (int) q[2])) != NULL) {
		    if (omodbuf != buf)
			xfree((ptr_t) omodbuf);
		    omodbuf = modbuf;
		}
		++q;
		break;

	    case 'a':
	    case 'g':
		/* Not implemented; this needs to be done before expanding
		 * lex. We don't have the words available to us anymore.
		 */
		++q;
		break;

	    case 'p':
		/* Ok */
		++q;
		break;

	    case '\0':
		break;

	    default:
		++q;
		break;
	    }
	    if (q[1])
		++q;
	}
	if (omodbuf != buf) {
	    (void) Strcpy(buf, omodbuf);
	    xfree((ptr_t) omodbuf);
	    bend = Strend(buf);
	}
    }

    /*
     * Now replace the text from op to q inclusive with the text from buf to
     * bend.
     */
    q++;

    /*
     * Now replace text non-inclusively like a real CS major!
     */
    if (LastChar + (bend - buf) - (q - op) >= InputLim)
	goto excl_err;
    (void) memmove((ptr_t) (q + (bend - buf) - (q - op)), (ptr_t) q, 
		   (size_t) ((LastChar - q) * sizeof(Char)));
    LastChar += (bend - buf) - (q - op);
    Cursor += (bend - buf) - (q - op);
    (void) memmove((ptr_t) op, (ptr_t) buf, 
		   (size_t) ((bend - buf) * sizeof(Char)));
    *LastChar = '\0';
    return(op + (bend - buf));
excl_err:
    SoundBeep();
    return(op + 1);
}

/*
 * c_excl: An excl has been found at point p -- back up and find some white
 * space (or the beginning of the buffer) and properly expand all the excl's
 * from there up to the current cursor position. We also avoid (trying to)
 * expanding '>!'
 */

static void
c_excl(p)
    Char *p;
{
    int i;
    Char *q;

    /*
     * if />[SPC TAB]*![SPC TAB]/, back up p to just after the >. otherwise,
     * back p up to just before the current word.
     */
    if ((p[1] == ' ' || p[1] == '\t') &&
	(p[-1] == ' ' || p[-1] == '\t' || p[-1] == '>')) {
	for (q = p - 1; q > InputBuf && (*q == ' ' || *q == '\t'); --q)
	    continue;
	if (*q == '>')
	    ++p;
    }
    else {
	while (*p != ' ' && *p != '\t' && p > InputBuf)
	    --p;
    }

    /*
     * Forever: Look for history char.  (Stop looking when we find the cursor.)
     * Count backslashes.  Of odd, skip history char. Return if all done.
     * Expand if even number of backslashes.
     */
    for (;;) {
	while (*p != HIST && p < Cursor)
	    ++p;
	for (i = 1; (p - i) >= InputBuf && p[-i] == '\\'; i++)
	    continue;
	if (i % 2 == 0)
	    ++p;
	if (p >= Cursor)
	    return;
	if (i % 2 == 1)
	    p = c_expand(p);
    }
}


static void
c_substitute()
{
    Char *p;

    /*
     * Start p out one character before the cursor.  Move it backwards looking
     * for white space, the beginning of the line, or a history character.
     */
    for (p = Cursor - 1; 
	 p > InputBuf && *p != ' ' && *p != '\t' && *p != HIST; --p)
	continue;

    /*
     * If we found a history character, go expand it.
     */
    if (*p == HIST)
	c_excl(p);
    Refresh();
}

static void
c_delfini()		/* Finish up delete action */
{
    int Size;

    if (ActionFlag & TCSHOP_INSERT)
	c_alternativ_key_map(0);

    ActionFlag = TCSHOP_NOP;

    if (ActionPos == 0) 
	return;

    UndoAction = TCSHOP_INSERT;

    if (Cursor > ActionPos) {
	Size = (int) (Cursor-ActionPos);
	c_delbefore(Size); 
	Cursor = ActionPos;
#if defined(DSPMBYTE)
	if (_enable_mbdisp && extdel) {
	    Cursor--;
	    e_redisp(1);
	}
#endif
	RefCursor();
    }
    else if (Cursor < ActionPos) {
	Size = (int)(ActionPos-Cursor);
	c_delafter(Size);
    }
    else  {
	Size = 1;
	c_delafter(Size);
    }
    UndoPtr = Cursor;
    UndoSize = Size;
}

static Char *
c_endword(p, high, n, delim)
    Char *p, *high, *delim;
    int n;
{
    int inquote = 0;
    p++;

    while (n--) {
        while (p < high) {	/* Skip non-word chars */
	  if (!Strchr(delim, *p) || *(p-1) == (Char)'\\')
	    break;
	  p++;
        }
	while (p < high) {	/* Skip string */
	  if ((*p == (Char)'\'' || *p == (Char)'"')) { /* Quotation marks? */
	    if (inquote || *(p-1) != (Char)'\\') { /* Should it be honored? */
	      if (inquote == 0) inquote = *p;
	      else if (inquote == *p) inquote = 0;
	    }
	  }
	  /* Break if unquoted non-word char */
	  if (!inquote && Strchr(delim, *p) && *(p-1) != (Char)'\\')
	    break;
	  p++;
	}
    }

    p--;
    return(p);
}


static Char *
c_eword(p, high, n)
    Char *p, *high;
    int n;
{
    p++;

    while (n--) {
	while ((p < high) && Isspace(*p)) 
	    p++;

	if (Isalnum(*p))
	    while ((p < high) && Isalnum(*p)) 
		p++;
	else
	    while ((p < high) && !(Isspace(*p) || Isalnum(*p)))
		p++;
    }

    p--;
    return(p);
}

/* Set the max length of the kill ring */
void
SetKillRing(max)
    int max;
{
    CStr *new;
    int count, i, j;

    if (max < 1)
	max = 1;		/* no ring, but always one buffer */
    if (max == KillRingMax)
	return;
    new = (CStr *)xcalloc((size_t) max, sizeof(CStr));
    if (KillRing != NULL) {
	if (KillRingLen != 0) {
	    if (max >= KillRingLen) {
		count = KillRingLen;
		j = KillPos;
	    } else {
		count = max;
		j = (KillPos - count + KillRingLen) % KillRingLen;
	    }
	    for (i = 0; i < KillRingLen; i++) {
		if (i < count)	/* copy latest */
		    new[i] = KillRing[j];
		else		/* free the others */
		    xfree(KillRing[j].buf);
		j = (j + 1) % KillRingLen;
	    }
	    KillRingLen = count;
	    KillPos = count % max;
	    YankPos = count - 1;
	}
	xfree(KillRing);
    }
    KillRing = new;
    KillRingMax = max;
}

/* Push string from start upto (but not including) end onto kill ring */
static void
c_push_kill(start, end)
    Char *start, *end;
{
    CStr save, *pos;
    Char *dp, *cp, *kp;
    int len = end - start, i, j, k;

    /* Check for duplicates? */
    if (KillRingLen > 0 && (dp = varval(STRkilldup)) != STRNULL) {
	YankPos = (KillPos - 1 + KillRingLen) % KillRingLen;
	if (eq(dp, STRerase)) {	/* erase earlier one (actually move up) */
	    j = YankPos;
	    for (i = 0; i < KillRingLen; i++) {
		if (Strncmp(KillRing[j].buf, start, (size_t) len) == 0 &&
		    KillRing[j].buf[len] == '\0') {
		    save = KillRing[j];
		    for ( ; i > 0; i--) {
			k = j;
			j = (j + 1) % KillRingLen;
			KillRing[k] = KillRing[j];
		    }
		    KillRing[j] = save;
		    return;
		}
		j = (j - 1 + KillRingLen) % KillRingLen;
	    }
	} else if (eq(dp, STRall)) { /* skip if any earlier */
	    for (i = 0; i < KillRingLen; i++)
		if (Strncmp(KillRing[i].buf, start, (size_t) len) == 0 &&
		    KillRing[i].buf[len] == '\0')
		    return;
	} else if (eq(dp, STRprev)) { /* skip if immediately previous */
	    j = YankPos;
	    if (Strncmp(KillRing[j].buf, start, (size_t) len) == 0 &&
		KillRing[j].buf[len] == '\0')
		return;
	}
    }

    /* No duplicate, go ahead and push */
    len++;			/* need space for '\0' */
    YankPos = KillPos;
    if (KillRingLen < KillRingMax)
	KillRingLen++;
    pos = &KillRing[KillPos];
    KillPos = (KillPos + 1) % KillRingMax;
    if (pos->len < len) {
	if (pos->buf == NULL)
	    pos->buf = (Char *) xmalloc(len * sizeof(Char));
	else
	    pos->buf = (Char *) xrealloc((ptr_t) pos->buf, len * sizeof(Char));
	pos->len = len;
    }
    cp = start;
    kp = pos->buf;
    while (cp < end)
	*kp++ = *cp++;
    *kp = '\0';
}

static CCRETVAL
c_get_histline()
{
    struct Hist *hp;
    int     h;

    if (Hist_num == 0) {	/* if really the current line */
	copyn(InputBuf, HistBuf, INBUFSIZE);
	LastChar = InputBuf + (LastHist - HistBuf);

#ifdef KSHVI
    if (VImode)
	Cursor = InputBuf;
    else
#endif /* KSHVI */
	Cursor = LastChar;

	return(CC_REFRESH);
    }

    hp = Histlist.Hnext;
    if (hp == NULL)
	return(CC_ERROR);

    for (h = 1; h < Hist_num; h++) {
	if ((hp->Hnext) == NULL) {
	    Hist_num = h;
	    return(CC_ERROR);
	}
	hp = hp->Hnext;
    }

    if (HistLit && hp->histline) {
	copyn(InputBuf, hp->histline, INBUFSIZE);
	CurrentHistLit = 1;
    }
    else {
	(void) sprlex(InputBuf, sizeof(InputBuf) / sizeof(Char), &hp->Hlex);
	CurrentHistLit = 0;
    }
    LastChar = InputBuf + Strlen(InputBuf);

    if (LastChar > InputBuf) {
	if (LastChar[-1] == '\n')
	    LastChar--;
#if 0
	if (LastChar[-1] == ' ')
	    LastChar--;
#endif
	if (LastChar < InputBuf)
	    LastChar = InputBuf;
    }

#ifdef KSHVI
    if (VImode)
	Cursor = InputBuf;
    else
#endif /* KSHVI */
	Cursor = LastChar;

    return(CC_REFRESH);
}

static CCRETVAL
c_search_line(pattern, dir)
Char *pattern;
int dir;
{
    Char *cp;
    int len;

    len = (int) Strlen(pattern);

    if (dir == F_UP_SEARCH_HIST) {
	for (cp = Cursor; cp >= InputBuf; cp--)
	    if (Strncmp(cp, pattern, (size_t) len) == 0 ||
		Gmatch(cp, pattern)) {
		Cursor = cp;
		return(CC_NORM);
	    }
	return(CC_ERROR);
    } else {
	for (cp = Cursor; *cp != '\0' && cp < InputLim; cp++)
	    if (Strncmp(cp, pattern, (size_t) len) == 0 ||
		Gmatch(cp, pattern)) {
		Cursor = cp;
		return(CC_NORM);
	    }
	return(CC_ERROR);
    }
}

static CCRETVAL
e_inc_search(dir)
    int dir;
{
    static Char STRfwd[] = { 'f', 'w', 'd', '\0' },
		STRbck[] = { 'b', 'c', 'k', '\0' };
    static Char pchar = ':';	/* ':' = normal, '?' = failed */
    static Char endcmd[2];
    Char ch, *cp,
	*oldCursor = Cursor,
	oldpchar = pchar;
    CCRETVAL ret = CC_NORM;
    int oldHist_num = Hist_num,
	oldpatlen = patlen,
	newdir = dir,
        done, redo;

    if (LastChar + sizeof(STRfwd)/sizeof(Char) + 2 + patlen >= InputLim)
	return(CC_ERROR);

    for (;;) {

	if (patlen == 0) {	/* first round */
	    pchar = ':';
	    patbuf[patlen++] = '*';
	}
	done = redo = 0;
	*LastChar++ = '\n';
	for (cp = newdir == F_UP_SEARCH_HIST ? STRbck : STRfwd; 
	     *cp; *LastChar++ = *cp++)
	    continue;
	*LastChar++ = pchar;
	for (cp = &patbuf[1]; cp < &patbuf[patlen]; *LastChar++ = *cp++)
	    continue;
	*LastChar = '\0';
	Refresh();

	if (GetNextChar(&ch) != 1)
	    return(e_send_eof(0));

	switch (CurrentKeyMap[(unsigned char) ch]) {
	case F_INSERT:
	case F_DIGIT:
	case F_MAGIC_SPACE:
	    if (patlen > INBUFSIZE - 3)
		SoundBeep();
	    else {
		patbuf[patlen++] = ch;
		*LastChar++ = ch;
		*LastChar = '\0';
		Refresh();
	    }
	    break;

	case F_INC_FWD:
	    newdir = F_DOWN_SEARCH_HIST;
	    redo++;
	    break;

	case F_INC_BACK:
	    newdir = F_UP_SEARCH_HIST;
	    redo++;
	    break;

	case F_DELPREV:
	    if (patlen > 1)
		done++;
	    else 
		SoundBeep();
	    break;

	default:
	    switch (ch) {
	    case 0007:		/* ^G: Abort */
		ret = CC_ERROR;
		done++;
		break;

	    case 0027:		/* ^W: Append word */
		/* No can do if globbing characters in pattern */
		for (cp = &patbuf[1]; ; cp++)
		    if (cp >= &patbuf[patlen]) {
			Cursor += patlen - 1;
			cp = c_next_word(Cursor, LastChar, 1);
			while (Cursor < cp && *Cursor != '\n') {
			    if (patlen > INBUFSIZE - 3) {
				SoundBeep();
				break;
			    }
			    patbuf[patlen++] = *Cursor;
			    *LastChar++ = *Cursor++;
			}
			Cursor = oldCursor;
			*LastChar = '\0';
			Refresh();
			break;
		    } else if (isglob(*cp)) {
			SoundBeep();
			break;
		    }
		break;
	    
	    default:		/* Terminate and execute cmd */
		endcmd[0] = ch;
		PushMacro(endcmd);
		/*FALLTHROUGH*/

	    case 0033:		/* ESC: Terminate */
		ret = CC_REFRESH;
		done++;
		break;
	    }
	    break;
	}

	while (LastChar > InputBuf && *LastChar != '\n')
	    *LastChar-- = '\0';
	*LastChar = '\0';

	if (!done) {

	    /* Can't search if unmatched '[' */
	    for (cp = &patbuf[patlen - 1], ch = ']'; cp > patbuf; cp--)
		if (*cp == '[' || *cp == ']') {
		    ch = *cp;
		    break;
		}

	    if (patlen > 1 && ch != '[') {
		if (redo && newdir == dir) {
		    if (pchar == '?') {	/* wrap around */
			Hist_num = newdir == F_UP_SEARCH_HIST ? 0 : 0x7fffffff;
			if (c_get_histline() == CC_ERROR)
			    /* Hist_num was fixed by first call */
			    (void) c_get_histline();
			Cursor = newdir == F_UP_SEARCH_HIST ?
			    LastChar : InputBuf;
		    } else
			Cursor += newdir == F_UP_SEARCH_HIST ? -1 : 1;
		}
		patbuf[patlen++] = '*';
		patbuf[patlen] = '\0';
		if (Cursor < InputBuf || Cursor > LastChar ||
		    (ret = c_search_line(&patbuf[1], newdir)) == CC_ERROR) {
		    LastCmd = (KEYCMD) newdir; /* avoid c_hsetpat */
		    ret = newdir == F_UP_SEARCH_HIST ?
			e_up_search_hist(0) : e_down_search_hist(0);
		    if (ret != CC_ERROR) {
			Cursor = newdir == F_UP_SEARCH_HIST ?
			    LastChar : InputBuf;
			(void) c_search_line(&patbuf[1], newdir);
		    }
		}
		patbuf[--patlen] = '\0';
		if (ret == CC_ERROR) {
		    SoundBeep();
		    if (Hist_num != oldHist_num) {
			Hist_num = oldHist_num;
			if (c_get_histline() == CC_ERROR)
			    return(CC_ERROR);
		    }
		    Cursor = oldCursor;
		    pchar = '?';
		} else {
		    pchar = ':';
		}
	    }

	    ret = e_inc_search(newdir);

	    if (ret == CC_ERROR && pchar == '?' && oldpchar == ':') {
		/* break abort of failed search at last non-failed */
		ret = CC_NORM;
	    }

	}

	if (ret == CC_NORM || (ret == CC_ERROR && oldpatlen == 0)) {
	    /* restore on normal return or error exit */
	    pchar = oldpchar;
	    patlen = oldpatlen;
	    if (Hist_num != oldHist_num) {
		Hist_num = oldHist_num;
		if (c_get_histline() == CC_ERROR)
		    return(CC_ERROR);
	    }
	    Cursor = oldCursor;
	    if (ret == CC_ERROR)
		Refresh();
	}
	if (done || ret != CC_NORM)
	    return(ret);
	    
    }

}

static CCRETVAL
v_search(dir)
    int dir;
{
    Char ch;
    Char tmpbuf[INBUFSIZE];
    Char oldbuf[INBUFSIZE];
    Char *oldlc, *oldc;
    int tmplen;

    copyn(oldbuf, InputBuf, INBUFSIZE);
    oldlc = LastChar;
    oldc = Cursor;
    tmplen = 0;
    tmpbuf[tmplen++] = '*';

    InputBuf[0] = '\0';
    LastChar = InputBuf;
    Cursor = InputBuf;
    searchdir = dir;

    c_insert(2);	/* prompt + '\n' */
    *Cursor++ = '\n';
    *Cursor++ = dir == F_UP_SEARCH_HIST ? '?' : '/';
    Refresh();
    for (ch = 0;ch == 0;) {
	if (GetNextChar(&ch) != 1)
	    return(e_send_eof(0));
	switch (ASC(ch)) {
	case 0010:	/* Delete and backspace */
	case 0177:
	    if (tmplen > 1) {
		*Cursor-- = '\0';
		LastChar = Cursor;
		tmpbuf[tmplen--] = '\0';
	    }
	    else {
		copyn(InputBuf, oldbuf, INBUFSIZE);
		LastChar = oldlc;
		Cursor = oldc;
		return(CC_REFRESH);
	    }
	    Refresh();
	    ch = 0;
	    break;

	case 0033:	/* ESC */
#ifdef IS_ASCII
	case '\r':	/* Newline */
	case '\n':
#else
	case '\012':    /* ASCII Line feed */
	case '\015':    /* ASCII (or EBCDIC) Return */
#endif
	    break;

	default:
	    if (tmplen >= INBUFSIZE)
		SoundBeep();
	    else {
		tmpbuf[tmplen++] = ch;
		*Cursor++ = ch;
		LastChar = Cursor;
	    }
	    Refresh();
	    ch = 0;
	    break;
	}
    }

    if (tmplen == 1) {
	/*
	 * Use the old pattern, but wild-card it.
	 */
	if (patlen == 0) {
	    InputBuf[0] = '\0';
	    LastChar = InputBuf;
	    Cursor = InputBuf;
	    Refresh();
	    return(CC_ERROR);
	}
	if (patbuf[0] != '*') {
	    (void) Strcpy(tmpbuf, patbuf);
	    patbuf[0] = '*';
	    (void) Strcpy(&patbuf[1], tmpbuf);
	    patlen++;
	    patbuf[patlen++] = '*';
	    patbuf[patlen] = '\0';
	}
    }
    else {
	tmpbuf[tmplen++] = '*';
	tmpbuf[tmplen] = '\0';
	(void) Strcpy(patbuf, tmpbuf);
	patlen = tmplen;
    }
    LastCmd = (KEYCMD) dir; /* avoid c_hsetpat */
    Cursor = LastChar = InputBuf;
    if ((dir == F_UP_SEARCH_HIST ? e_up_search_hist(0) : 
				   e_down_search_hist(0)) == CC_ERROR) {
	Refresh();
	return(CC_ERROR);
    }
    else {
	if (ch == 0033) {
	    Refresh();
	    *LastChar++ = '\n';
	    *LastChar = '\0';
	    PastBottom();
	    return(CC_NEWLINE);
	}
	else
	    return(CC_REFRESH);
    }
}

/*
 * semi-PUBLIC routines.  Any routine that is of type CCRETVAL is an
 * entry point, called from the CcKeyMap indirected into the
 * CcFuncTbl array.
 */

/*ARGSUSED*/
CCRETVAL
v_cmd_mode(c)
    int c;
{
    USE(c);
    InsertPos = 0;
    ActionFlag = TCSHOP_NOP;	/* [Esc] cancels pending action */
    ActionPos = 0;
    DoingArg = 0;
    if (UndoPtr > Cursor)
	UndoSize = (int)(UndoPtr - Cursor);
    else
	UndoSize = (int)(Cursor - UndoPtr);

    inputmode = MODE_INSERT;
    c_alternativ_key_map(1);
#ifdef notdef
    /*
     * We don't want to move the cursor, because all the editing
     * commands don't include the character under the cursor.
     */
    if (Cursor > InputBuf)
	Cursor--;
#endif
    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_unassigned(c)
    int c;
{				/* bound to keys that arn't really assigned */
    USE(c);
    SoundBeep();
    flush();
    return(CC_NORM);
}

CCRETVAL
e_insert(c)
    int c;
{
    int i;
#if defined(DSPMBYTE)
    CCRETVAL ret;
    static Char savec;
    static int exterr = 0;
#endif
#ifndef SHORT_STRINGS
    c &= ASCII;			/* no meta chars ever */
#endif
#if defined(DSPMBYTE)
    ret = (CCRETVAL) CC_NORM;
#endif

    if (!c)
	return(CC_ERROR);	/* no NULs in the input ever!! */

    if (LastChar + Argument >= InputLim)
	return(CC_ERROR);	/* end of buffer space */

    if (Argument == 1) {  	/* How was this optimized ???? */

#if defined(DSPMBYTE)
	if(_enable_mbdisp && extins && exterr && Ismbyte2(c)) {
	    extins = 0;
	    exterr = 0;
	    return(CC_ERROR);
	}
#endif
	if (inputmode != MODE_INSERT) {
	    UndoBuf[UndoSize++] = *Cursor;
	    UndoBuf[UndoSize] = '\0';
	    c_delafter(1);   /* Do NOT use the saving ONE */
    	}

        c_insert(1);

#if defined(DSPMBYTE)
	/* 1st. byte is store to special buffer, and replace space */
	if(_enable_mbdisp && extins == 0 && Ismbyte1(c)) {
	    extins++;
	    savec = (Char) c;
	    *Cursor++ = (Char) ' ';
	}
	else if (_enable_mbdisp && extins && Ismbyte2(c)) {
	    *(Cursor-1) = savec;
	    *Cursor++ = (Char) c;
	    extins = 0;
	    e_redisp(1);
	    Refresh();
	    ret = CC_REFRESH;
	}
	else
	    *Cursor++ = (Char) c;
	DoingArg = 0;		/* just in case */
	if (ret != CC_REFRESH)
	    RefPlusOne();	/* fast refresh for one char. */
#else
	*Cursor++ = (Char) c;
	DoingArg = 0;		/* just in case */
	RefPlusOne();		/* fast refresh for one char. */
#endif
    }
    else {
#if defined(DSPMBYTE)
	/* Cannot use ESC-(number) for multi-byte */
	if (_enable_mbdisp && extins == 0 && Ismbyte1(c)) {
	    extins++;
	    exterr++;
	    return(CC_ERROR);
	}
	else if (_enable_mbdisp && extins && exterr && Ismbyte2(c))
	{
	    extins = 0;
	    exterr = 0;
	    return(CC_ERROR);
	}
#endif
	if (inputmode != MODE_INSERT) {

	    for(i=0;i<Argument;i++) 
		UndoBuf[UndoSize++] = *(Cursor+i);

	    UndoBuf[UndoSize] = '\0';
	    c_delafter(Argument);   /* Do NOT use the saving ONE */
    	}

        c_insert(Argument);

	while (Argument--)
	    *Cursor++ = (Char) c;
	Refresh();
    }

    if (inputmode == MODE_REPLACE_1)
	(void) v_cmd_mode(0);

#if defined(DSPMBYTE)
    return(ret);
#else
    return(CC_NORM);
#endif
}

int
InsertStr(s)			/* insert ASCIZ s at cursor (for complete) */
    Char   *s;
{
    int len;

    if ((len = (int) Strlen(s)) <= 0)
	return -1;
    if (LastChar + len >= InputLim)
	return -1;		/* end of buffer space */

    c_insert(len);
    while (len--)
	*Cursor++ = *s++;
    return 0;
}

void
DeleteBack(n)			/* delete the n characters before . */
    int     n;
{
    if (n <= 0)
	return;
    if (Cursor >= &InputBuf[n]) {
	c_delbefore(n);		/* delete before dot */
	if (n > Cursor - InputBuf)
	    Cursor = InputBuf;	/* bounds check */
	else
	    Cursor -= n;
#if defined(DSPMBYTE)
	if(_enable_mbdisp && extdel && Cursor > InputBuf) {
	    Cursor--;
	    e_redisp(1);
	}
#endif
    }
}

CCRETVAL
e_digit(c)			/* gray magic here */
    int c;
{
    if (!Isdigit(c))
	return(CC_ERROR);	/* no NULs in the input ever!! */

    if (DoingArg) {		/* if doing an arg, add this in... */
	if (LastCmd == F_ARGFOUR)	/* if last command was ^U */
	    Argument = c - '0';
	else {
	    if (Argument > 1000000)
		return CC_ERROR;
	    Argument = (Argument * 10) + (c - '0');
	}
	return(CC_ARGHACK);
    }
    else {
	if (LastChar + 1 >= InputLim)
	    return CC_ERROR;	/* end of buffer space */

	if (inputmode != MODE_INSERT) {
	    UndoBuf[UndoSize++] = *Cursor;
	    UndoBuf[UndoSize] = '\0';
	    c_delafter(1);   /* Do NOT use the saving ONE */
    	}
	c_insert(1);
	*Cursor++ = (Char) c;
	DoingArg = 0;		/* just in case */
	RefPlusOne();		/* fast refresh for one char. */
    }
    return(CC_NORM);
}

CCRETVAL
e_argdigit(c)			/* for ESC-n */
    int c;
{
    c &= ASCII;

    if (!Isdigit(c))
	return(CC_ERROR);	/* no NULs in the input ever!! */

    if (DoingArg) {		/* if doing an arg, add this in... */
	if (Argument > 1000000)
	    return CC_ERROR;
	Argument = (Argument * 10) + (c - '0');
    }
    else {			/* else starting an argument */
	Argument = c - '0';
	DoingArg = 1;
    }
    return(CC_ARGHACK);
}

CCRETVAL
v_zero(c)			/* command mode 0 for vi */
    int c;
{
    if (DoingArg) {		/* if doing an arg, add this in... */
	if (Argument > 1000000)
	    return CC_ERROR;
	Argument = (Argument * 10) + (c - '0');
	return(CC_ARGHACK);
    }
    else {			/* else starting an argument */
	Cursor = InputBuf;
	if (ActionFlag & TCSHOP_DELETE) {
	   c_delfini();
	   return(CC_REFRESH);
        }
	RefCursor();		/* move the cursor */
	return(CC_NORM);
    }
}

/*ARGSUSED*/
CCRETVAL
e_newline(c)
    int c;
{				/* always ignore argument */
    USE(c);
  /*  PastBottom();  NOW done in ed.inputl.c */
    *LastChar++ = '\n';		/* for the benefit of CSH */
    *LastChar = '\0';		/* just in case */
    if (VImode)
	InsertPos = InputBuf;	/* Reset editing position */
    return(CC_NEWLINE);
}

/*ARGSUSED*/
CCRETVAL
e_send_eof(c)
    int c;
{				/* for when ^D is ONLY send-eof */
    USE(c);
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return(CC_EOF);
}

/*ARGSUSED*/
CCRETVAL
e_complete(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_COMPLETE);
}

/*ARGSUSED*/
CCRETVAL
e_complete_back(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_COMPLETE_BACK);
}

/*ARGSUSED*/
CCRETVAL
e_complete_fwd(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_COMPLETE_FWD);
}

/*ARGSUSED*/
CCRETVAL
e_complete_all(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_COMPLETE_ALL);
}

/*ARGSUSED*/
CCRETVAL
v_cm_complete(c)
    int c;
{
    USE(c);
    if (Cursor < LastChar)
	Cursor++;
    *LastChar = '\0';		/* just in case */
    return(CC_COMPLETE);
}

/*ARGSUSED*/
CCRETVAL
e_toggle_hist(c)
    int c;
{
    struct Hist *hp;
    int     h;

    USE(c);
    *LastChar = '\0';		/* just in case */

    if (Hist_num <= 0) {
	return CC_ERROR;
    }

    hp = Histlist.Hnext;
    if (hp == NULL) {	/* this is only if no history */
	return(CC_ERROR);
    }

    for (h = 1; h < Hist_num; h++)
	hp = hp->Hnext;

    if (!CurrentHistLit) {
	if (hp->histline) {
	    copyn(InputBuf, hp->histline, INBUFSIZE);
	    CurrentHistLit = 1;
	}
	else {
	    return CC_ERROR;
	}
    }
    else {
	(void) sprlex(InputBuf, sizeof(InputBuf) / sizeof(Char), &hp->Hlex);
	CurrentHistLit = 0;
    }

    LastChar = InputBuf + Strlen(InputBuf);
    if (LastChar > InputBuf) {
	if (LastChar[-1] == '\n')
	    LastChar--;
	if (LastChar[-1] == ' ')
	    LastChar--;
	if (LastChar < InputBuf)
	    LastChar = InputBuf;
    }

#ifdef KSHVI
    if (VImode)
	Cursor = InputBuf;
    else
#endif /* KSHVI */
	Cursor = LastChar;

    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_up_hist(c)
    int c;
{
    Char    beep = 0;

    USE(c);
    UndoAction = TCSHOP_NOP;
    *LastChar = '\0';		/* just in case */

    if (Hist_num == 0) {	/* save the current buffer away */
	copyn(HistBuf, InputBuf, INBUFSIZE);
	LastHist = HistBuf + (LastChar - InputBuf);
    }

    Hist_num += Argument;

    if (c_get_histline() == CC_ERROR) {
	beep = 1;
	(void) c_get_histline(); /* Hist_num was fixed by first call */
    }

    Refresh();
    if (beep)
	return(CC_ERROR);
    else
	return(CC_NORM);	/* was CC_UP_HIST */
}

/*ARGSUSED*/
CCRETVAL
e_down_hist(c)
    int c;
{
    USE(c);
    UndoAction = TCSHOP_NOP;
    *LastChar = '\0';		/* just in case */

    Hist_num -= Argument;

    if (Hist_num < 0) {
	Hist_num = 0;
	return(CC_ERROR);	/* make it beep */
    }

    return(c_get_histline());
}



/*
 * c_hmatch() return True if the pattern matches the prefix
 */
static int
c_hmatch(str)
Char *str;
{
    if (Strncmp(patbuf, str, (size_t) patlen) == 0)
	return 1;
    return Gmatch(str, patbuf);
}

/*
 * c_hsetpat(): Set the history seatch pattern
 */
static void
c_hsetpat()
{
    if (LastCmd != F_UP_SEARCH_HIST && LastCmd != F_DOWN_SEARCH_HIST) {
	patlen = (int) (Cursor - InputBuf);
	if (patlen >= INBUFSIZE) patlen = INBUFSIZE -1;
	if (patlen >= 0)  {
	    (void) Strncpy(patbuf, InputBuf, (size_t) patlen);
	    patbuf[patlen] = '\0';
	}
	else
	    patlen = (int) Strlen(patbuf);
    }
#ifdef SDEBUG
    xprintf("\nHist_num = %d\n", Hist_num);
    xprintf("patlen = %d\n", patlen);
    xprintf("patbuf = \"%S\"\n", patbuf);
    xprintf("Cursor %d LastChar %d\n", Cursor - InputBuf, LastChar - InputBuf);
#endif
}

/*ARGSUSED*/
CCRETVAL
e_up_search_hist(c)
    int c;
{
    struct Hist *hp;
    int h;
    bool    found = 0;

    USE(c);
    ActionFlag = TCSHOP_NOP;
    UndoAction = TCSHOP_NOP;
    *LastChar = '\0';		/* just in case */
    if (Hist_num < 0) {
#ifdef DEBUG_EDIT
	xprintf("%s: e_up_search_hist(): Hist_num < 0; resetting.\n", progname);
#endif
	Hist_num = 0;
	return(CC_ERROR);
    }

    if (Hist_num == 0)
    {
	copyn(HistBuf, InputBuf, INBUFSIZE);
	LastHist = HistBuf + (LastChar - InputBuf);
    }


    hp = Histlist.Hnext;
    if (hp == NULL)
	return(CC_ERROR);

    c_hsetpat();		/* Set search pattern !! */

    for (h = 1; h <= Hist_num; h++)
	hp = hp->Hnext;

    while (hp != NULL) {
	Char sbuf[INBUFSIZE], *hl;
	if (hp->histline == NULL) {
	    hp->histline = Strsave(sprlex(sbuf, sizeof(sbuf) / sizeof(Char),
				   &hp->Hlex));
	}
	hl = HistLit ? hp->histline : sprlex(sbuf, sizeof(sbuf) / sizeof(Char),
					     &hp->Hlex);
#ifdef SDEBUG
	xprintf("Comparing with \"%S\"\n", hl);
#endif
	if ((Strncmp(hl, InputBuf, (size_t) (LastChar - InputBuf)) || 
	     hl[LastChar-InputBuf]) && c_hmatch(hl)) {
	    found++;
	    break;
	}
	h++;
	hp = hp->Hnext;
    }

    if (!found) {
#ifdef SDEBUG
	xprintf("not found\n"); 
#endif
	return(CC_ERROR);
    }

    Hist_num = h;

    return(c_get_histline());
}

/*ARGSUSED*/
CCRETVAL
e_down_search_hist(c)
    int c;
{
    struct Hist *hp;
    int h;
    bool    found = 0;

    USE(c);
    ActionFlag = TCSHOP_NOP;
    UndoAction = TCSHOP_NOP;
    *LastChar = '\0';		/* just in case */

    if (Hist_num == 0)
	return(CC_ERROR);

    hp = Histlist.Hnext;
    if (hp == 0)
	return(CC_ERROR);

    c_hsetpat();		/* Set search pattern !! */

    for (h = 1; h < Hist_num && hp; h++) {
	Char sbuf[INBUFSIZE], *hl;
	if (hp->histline == NULL) {
	    hp->histline = Strsave(sprlex(sbuf, sizeof(sbuf) / sizeof(Char),
				   &hp->Hlex));
	}
	hl = HistLit ? hp->histline : sprlex(sbuf, sizeof(sbuf) / sizeof(Char),
					     &hp->Hlex);
#ifdef SDEBUG
	xprintf("Comparing with \"%S\"\n", hl);
#endif
	if ((Strncmp(hl, InputBuf, (size_t) (LastChar - InputBuf)) || 
	     hl[LastChar-InputBuf]) && c_hmatch(hl))
	    found = h;
	hp = hp->Hnext;
    }

    if (!found) {		/* is it the current history number? */
	if (!c_hmatch(HistBuf)) {
#ifdef SDEBUG
	    xprintf("not found\n"); 
#endif
	    return(CC_ERROR);
	}
    }

    Hist_num = found;

    return(c_get_histline());
}

/*ARGSUSED*/
CCRETVAL
e_helpme(c)
    int c;
{
    USE(c);
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return(CC_HELPME);
}

/*ARGSUSED*/
CCRETVAL
e_correct(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_CORRECT);
}

/*ARGSUSED*/
CCRETVAL
e_correctl(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_CORRECT_L);
}

/*ARGSUSED*/
CCRETVAL
e_run_fg_editor(c)
    int c;
{
    struct process *pp;
    extern bool tellwhat;

    USE(c);
    if ((pp = find_stop_ed()) != NULL) {
	/* save our editor state so we can restore it */
	tellwhat = 1;
	copyn(WhichBuf, InputBuf, INBUFSIZE);
	LastWhich = WhichBuf + (LastChar - InputBuf);
	CursWhich = WhichBuf + (Cursor - InputBuf);
	HistWhich = Hist_num;
	Hist_num = 0;		/* for the history commands */

	/* put the tty in a sane mode */
	PastBottom();
	(void) Cookedmode();	/* make sure the tty is set up correctly */

	/* do it! */
	fg_proc_entry(pp);

	(void) Rawmode();	/* go on */
	Refresh();
	tellwhat = 0;
    }
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_list_choices(c)
    int c;
{
    USE(c);
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return(CC_LIST_CHOICES);
}

/*ARGSUSED*/
CCRETVAL
e_list_all(c)
    int c;
{
    USE(c);
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return(CC_LIST_ALL);
}

/*ARGSUSED*/
CCRETVAL
e_list_glob(c)
    int c;
{
    USE(c);
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return(CC_LIST_GLOB);
}

/*ARGSUSED*/
CCRETVAL
e_expand_glob(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_EXPAND_GLOB);
}

/*ARGSUSED*/
CCRETVAL
e_normalize_path(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_NORMALIZE_PATH);
}

/*ARGSUSED*/
CCRETVAL
e_normalize_command(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_NORMALIZE_COMMAND);
}

/*ARGSUSED*/
CCRETVAL
e_expand_vars(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    return(CC_EXPAND_VARS);
}

/*ARGSUSED*/
CCRETVAL
e_which(c)
    int c;
{				/* do a fast command line which(1) */
    USE(c);
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return(CC_WHICH);
}

/*ARGSUSED*/
CCRETVAL
e_last_item(c)
    int c;
{				/* insert the last element of the prev. cmd */
    Char *cp;
    struct Hist *hp;
    struct wordent *wp, *firstp;
    int i;
    Char buf[INBUFSIZE];

    USE(c);
    if (Argument <= 0)
	return(CC_ERROR);

    hp = Histlist.Hnext;
    if (hp == NULL) {	/* this is only if no history */
	return(CC_ERROR);
    }

    wp = (hp->Hlex).prev;

    if (wp->prev == (struct wordent *) NULL)
	return(CC_ERROR);	/* an empty history entry */

    firstp = (hp->Hlex).next;

    /* back up arg words in lex */
    for (i = 0; i < Argument && wp != firstp; i++) {
	wp = wp->prev;
    }

    cp = expand_lex(buf, INBUFSIZE, wp->prev, 0, i - 1);
    *cp = '\0';
    if (InsertStr(buf))
	return(CC_ERROR);

    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_dabbrev_expand(c)
    int c;
{				/* expand to preceding word matching prefix */
    Char *cp, *ncp, *bp;
    struct Hist *hp;
    int arg = 0, len = 0, i; /* len = 0 to shut up gcc -Wall */
    bool found = 0;
    Char hbuf[INBUFSIZE];
    static int oldevent, hist, word;
    static Char *start, *oldcursor;

    USE(c);
    if (Argument <= 0)
	return(CC_ERROR);

    cp = c_preword(Cursor, InputBuf, 1, STRshwordsep);
    if (cp == Cursor || Isspace(*cp))
	return(CC_ERROR);

    hp = Histlist.Hnext;
    bp = InputBuf;
    if (Argument == 1 && eventno == oldevent && cp == start &&
	Cursor == oldcursor && patlen > 0 && Strncmp(patbuf, cp, patlen) == 0){
	/* continue previous search - go to last match (hist/word) */
	if (hist != 0) {		/* need to move up history */
	    for (i = 1; i < hist && hp != NULL; i++)
		hp = hp->Hnext;
	    if (hp == NULL)	/* "can't happen" */
		return(CC_ERROR);
	    cp = expand_lex(hbuf, INBUFSIZE, &hp->Hlex, 0, NCARGS);
	    *cp = '\0';
	    bp = hbuf;
	    hp = hp->Hnext;
	}
	cp = c_preword(cp, bp, word, STRshwordsep);
    } else {			/* starting new search */
	oldevent = eventno;
	start = cp;
	patlen = (int) (Cursor - cp);
	(void) Strncpy(patbuf, cp, patlen);
	hist = 0;
	word = 0;
    }

    while (!found) {
	ncp = c_preword(cp, bp, 1, STRshwordsep);
	if (ncp == cp || Isspace(*ncp)) { /* beginning of line */
	    hist++;
	    word = 0;
	    if (hp == NULL)
		return(CC_ERROR);
	    cp = expand_lex(hbuf, INBUFSIZE, &hp->Hlex, 0, NCARGS);
	    *cp = '\0';
	    bp = hbuf;
	    hp = hp->Hnext;
	    continue;
	} else {
	    word++;
	    len = (int) (c_endword(ncp-1, cp, 1, STRshwordsep) - ncp + 1);
	    cp = ncp;
	}
	if (len > patlen && Strncmp(cp, patbuf, patlen) == 0) {
	    /* We don't fully check distinct matches as Gnuemacs does: */
	    if (Argument > 1) {	/* just count matches */
		if (++arg >= Argument)
		    found++;
	    } else {		/* match if distinct from previous */
		if (len != Cursor - start || Strncmp(cp, start, len) != 0)
		    found++;
	    }
	}
    }

    if (LastChar + len - (Cursor - start) >= InputLim)
	return(CC_ERROR);	/* no room */
    DeleteBack(Cursor - start);
    c_insert(len);
    while (len--)
	*Cursor++ = *cp++;
    oldcursor = Cursor;
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_yank_kill(c)
    int c;
{				/* almost like GnuEmacs */
    int len;
    Char *kp, *cp;

    USE(c);
    if (KillRingLen == 0)	/* nothing killed */
	return(CC_ERROR);
    len = Strlen(KillRing[YankPos].buf);
    if (LastChar + len >= InputLim)
	return(CC_ERROR);	/* end of buffer space */

    /* else */
    cp = Cursor;		/* for speed */

    c_insert(len);		/* open the space, */
    for (kp = KillRing[YankPos].buf; *kp; kp++)	/* copy the chars */
	*cp++ = *kp;

    if (Argument == 1) {	/* if no arg */
	Mark = Cursor;		/* mark at beginning, cursor at end */
	Cursor = cp;
    } else {
	Mark = cp;		/* else cursor at beginning, mark at end */
    }

    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_yank_pop(c)
    int c;
{				/* almost like GnuEmacs */
    int m_bef_c, del_len, ins_len;
    Char *kp, *cp;

    USE(c);

#if 0
    /* XXX This "should" be here, but doesn't work, since LastCmd
       gets set on CC_ERROR and CC_ARGHACK, which it shouldn't(?).
       (But what about F_ARGFOUR?) I.e. if you hit M-y twice the
       second one will "succeed" even if the first one wasn't preceded
       by a yank, and giving an argument is impossible. Now we "succeed"
       regardless of previous command, which is wrong too of course. */
    if (LastCmd != F_YANK_KILL && LastCmd != F_YANK_POP)
	return(CC_ERROR);
#endif

    if (KillRingLen == 0)	/* nothing killed */
	return(CC_ERROR);
    YankPos -= Argument;
    while (YankPos < 0)
	YankPos += KillRingLen;
    YankPos %= KillRingLen;

    if (Cursor > Mark) {
	del_len = Cursor - Mark;
	m_bef_c = 1;
    } else {
	del_len = Mark - Cursor;
	m_bef_c = 0;
    }
    ins_len = Strlen(KillRing[YankPos].buf);
    if (LastChar + ins_len - del_len >= InputLim)
	return(CC_ERROR);	/* end of buffer space */

    if (m_bef_c) {
	c_delbefore(del_len);
	Cursor = Mark;
    } else {
	c_delafter(del_len);
    }
    cp = Cursor;		/* for speed */

    c_insert(ins_len);		/* open the space, */
    for (kp = KillRing[YankPos].buf; *kp; kp++)	/* copy the chars */
	*cp++ = *kp;

    if (m_bef_c) {
	Mark = Cursor;		/* mark at beginning, cursor at end */
	Cursor = cp;
    } else {
	Mark = cp;		/* else cursor at beginning, mark at end */
    }

    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_delprev(c) 		/* Backspace key in insert mode */
    int c;
{
    int rc;

    USE(c);
    rc = CC_ERROR;

    if (InsertPos != 0) {
	if (Argument <= Cursor - InsertPos) {
	    c_delbefore(Argument);	/* delete before */
	    Cursor -= Argument;
#if defined(DSPMBYTE)
	if (_enable_mbdisp && extdel) {
	    Cursor--;
	    e_redisp(c);
	}
#endif
	    rc = CC_REFRESH;
	}
    }
    return(rc);
}   /* v_delprev  */

/*ARGSUSED*/
CCRETVAL
e_delprev(c)
    int c;
{
    USE(c);
    if (Cursor > InputBuf) {
	c_delbefore(Argument);	/* delete before dot */
	if (Argument > Cursor - InputBuf)
	    Cursor = InputBuf;	/* bounds check */
	else
	    Cursor -= Argument;
#if defined(DSPMBYTE)
	if (_enable_mbdisp && extdel && Cursor > InputBuf) {
	    Cursor--;
	    e_redisp(c);
	}
#endif
	return(CC_REFRESH);
    }
    else {
	return(CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
e_delwordprev(c)
    int c;
{
    Char *cp;

    USE(c);
    if (Cursor == InputBuf)
	return(CC_ERROR);
    /* else */

    cp = c_prev_word(Cursor, InputBuf, Argument);

    c_push_kill(cp, Cursor);	/* save the text */

    c_delbefore((int)(Cursor - cp));	/* delete before dot */
    Cursor = cp;
    if (Cursor < InputBuf)
	Cursor = InputBuf;	/* bounds check */
    return(CC_REFRESH);
}

/* DCS <dcs@neutron.chem.yale.edu>, 9 Oct 93
 *
 * Changed the names of some of the ^D family of editor functions to
 * correspond to what they actually do and created new e_delnext_list
 * for completeness.
 *   
 *   Old names:			New names:
 *   
 *   delete-char		delete-char-or-eof
 *     F_DELNEXT		  F_DELNEXT_EOF
 *     e_delnext		  e_delnext_eof
 *     edelnxt			  edelnxteof
 *   delete-char-or-eof		delete-char			
 *     F_DELNEXT_EOF		  F_DELNEXT
 *     e_delnext_eof		  e_delnext
 *     edelnxteof		  edelnxt
 *   delete-char-or-list	delete-char-or-list-or-eof
 *     F_LIST_DELNEXT		  F_DELNEXT_LIST_EOF
 *     e_list_delnext		  e_delnext_list_eof
 *   				  edellsteof
 *   (no old equivalent)	delete-char-or-list
 *   				  F_DELNEXT_LIST
 *   				  e_delnext_list
 *   				  e_delnxtlst
 */

/* added by mtk@ari.ncl.omron.co.jp (920818) */
/* rename e_delnext() -> e_delnext_eof() */
/*ARGSUSED*/
CCRETVAL
e_delnext(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar) {/* if I'm at the end */
	if (!VImode) {
		return(CC_ERROR);
	}
	else {
	    if (Cursor != InputBuf)
		Cursor--;
	    else
		return(CC_ERROR);
	}
    }
    c_delafter(Argument);	/* delete after dot */
    if (Cursor > LastChar)
	Cursor = LastChar;	/* bounds check */
    return(CC_REFRESH);
}


/*ARGSUSED*/
CCRETVAL
e_delnext_eof(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar) {/* if I'm at the end */
	if (!VImode) {
	    if (Cursor == InputBuf) {	
		/* if I'm also at the beginning */
		so_write(STReof, 4);/* then do a EOF */
		flush();
		return(CC_EOF);
	    }
	    else 
		return(CC_ERROR);
	}
	else {
	    if (Cursor != InputBuf)
		Cursor--;
	    else
		return(CC_ERROR);
	}
    }
    c_delafter(Argument);	/* delete after dot */
    if (Cursor > LastChar)
	Cursor = LastChar;	/* bounds check */
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_delnext_list(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar) {	/* if I'm at the end */
	PastBottom();
	*LastChar = '\0';	/* just in case */
	return(CC_LIST_CHOICES);
    }
    else {
	c_delafter(Argument);	/* delete after dot */
	if (Cursor > LastChar)
	    Cursor = LastChar;	/* bounds check */
	return(CC_REFRESH);
    }
}

/*ARGSUSED*/
CCRETVAL
e_delnext_list_eof(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar) {	/* if I'm at the end */
	if (Cursor == InputBuf) {	/* if I'm also at the beginning */
	    so_write(STReof, 4);/* then do a EOF */
	    flush();
	    return(CC_EOF);
	}
	else {
	    PastBottom();
	    *LastChar = '\0';	/* just in case */
	    return(CC_LIST_CHOICES);
	}
    }
    else {
	c_delafter(Argument);	/* delete after dot */
	if (Cursor > LastChar)
	    Cursor = LastChar;	/* bounds check */
	return(CC_REFRESH);
    }
}

/*ARGSUSED*/
CCRETVAL
e_list_eof(c)
    int c;
{
    CCRETVAL rv;

    USE(c);
    if (Cursor == LastChar && Cursor == InputBuf) {
	so_write(STReof, 4);	/* then do a EOF */
	flush();
	rv = CC_EOF;
    }
    else {
	PastBottom();
	*LastChar = '\0';	/* just in case */
	rv = CC_LIST_CHOICES;
    }
    return rv;
}

/*ARGSUSED*/
CCRETVAL
e_delwordnext(c)
    int c;
{
    Char *cp;

    USE(c);
    if (Cursor == LastChar)
	return(CC_ERROR);
    /* else */

    cp = c_next_word(Cursor, LastChar, Argument);

    c_push_kill(Cursor, cp);	/* save the text */

    c_delafter((int)(cp - Cursor));	/* delete after dot */
    if (Cursor > LastChar)
	Cursor = LastChar;	/* bounds check */
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_toend(c)
    int c;
{
    USE(c);
    Cursor = LastChar;
    if (VImode)
	if (ActionFlag & TCSHOP_DELETE) {
	    c_delfini();
	    return(CC_REFRESH);
	}
    RefCursor();		/* move the cursor */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tobeg(c)
    int c;
{
    USE(c);
    Cursor = InputBuf;

    if (VImode) {
       while (Isspace(*Cursor)) /* We want FIRST non space character */
	Cursor++;
	if (ActionFlag & TCSHOP_DELETE) {
	    c_delfini();
	    return(CC_REFRESH);
	}
    }

    RefCursor();		/* move the cursor */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_killend(c)
    int c;
{
    USE(c);
    c_push_kill(Cursor, LastChar); /* copy it */
    LastChar = Cursor;		/* zap! -- delete to end */
    return(CC_REFRESH);
}


/*ARGSUSED*/
CCRETVAL
e_killbeg(c)
    int c;
{
    USE(c);
    c_push_kill(InputBuf, Cursor); /* copy it */
    c_delbefore((int)(Cursor - InputBuf));
    Cursor = InputBuf;		/* zap! */
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_killall(c)
    int c;
{
    USE(c);
    c_push_kill(InputBuf, LastChar); /* copy it */
    LastChar = InputBuf;	/* zap! -- delete all of it */
    Cursor = InputBuf;
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_killregion(c)
    int c;
{
    USE(c);
    if (!Mark)
	return(CC_ERROR);

    if (Mark > Cursor) {
	c_push_kill(Cursor, Mark); /* copy it */
	c_delafter((int)(Mark - Cursor)); /* delete it - UNUSED BY VI mode */
	Mark = Cursor;
    }
    else {			/* mark is before cursor */
	c_push_kill(Mark, Cursor); /* copy it */
	c_delbefore((int)(Cursor - Mark));
	Cursor = Mark;
    }
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_copyregion(c)
    int c;
{
    USE(c);
    if (!Mark)
	return(CC_ERROR);

    if (Mark > Cursor) {
	c_push_kill(Cursor, Mark); /* copy it */
    }
    else {			/* mark is before cursor */
	c_push_kill(Mark, Cursor); /* copy it */
    }
    return(CC_NORM);		/* don't even need to Refresh() */
}

/*ARGSUSED*/
CCRETVAL
e_charswitch(cc)
    int cc;
{
    Char c;

    USE(cc);

    /* do nothing if we are at beginning of line or have only one char */
    if (Cursor == &InputBuf[0] || LastChar == &InputBuf[1]) {
	return(CC_ERROR);
    }

    if (Cursor < LastChar) {
	Cursor++;
    }
    c = Cursor[-2];
    Cursor[-2] = Cursor[-1];
    Cursor[-1] = c;
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_gcharswitch(cc)
    int cc;
{				/* gosmacs style ^T */
    Char c;

    USE(cc);
    if (Cursor > &InputBuf[1]) {/* must have at least two chars entered */
	c = Cursor[-2];
	Cursor[-2] = Cursor[-1];
	Cursor[-1] = c;
	return(CC_REFRESH);
    }
    else {
	return(CC_ERROR);
    }
}

#if defined(DSPMBYTE) /* BY TAGA Nayuta VERY THANKS */
/*ARGSUSED*/
static void
e_charback_mbyte(argument)
     int argument;
{
    if (!_enable_mbdisp) {
	if (Argument > Cursor - InputBuf)
	    Cursor = InputBuf;
	else
	    Cursor -= Argument;
    }
    else {
	while (0 < argument && Cursor > InputBuf) {
	    if (Cursor - 1 != InputBuf &&
		Ismbyte1(*(Cursor - 2)) && Ismbyte2(*(Cursor - 1))) {
		Cursor--;
	    }
	    Cursor--;
	    argument--;
	}
    }
}
#endif

/*ARGSUSED*/
CCRETVAL
e_charback(c)
    int c;
{
    USE(c);
    if (Cursor > InputBuf) {
#if defined(DSPMBYTE) /* BY TAGA Nayuta VERY THANKS */
	e_charback_mbyte(Argument);
#else
	if (Argument > Cursor - InputBuf)
	    Cursor = InputBuf;
	else
	    Cursor -= Argument;
#endif

	if (VImode)
	    if (ActionFlag & TCSHOP_DELETE) {
		c_delfini();
		return(CC_REFRESH);
	    }

	RefCursor();
	return(CC_NORM);
    }
    else {
	return(CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
v_wordback(c)
    int c;
{
    USE(c);
    if (Cursor == InputBuf)
	return(CC_ERROR);
    /* else */

    Cursor = c_preword(Cursor, InputBuf, Argument, STRshwspace); /* bounds check */

    if (ActionFlag & TCSHOP_DELETE) {
	c_delfini();
	return(CC_REFRESH);
    }

    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_wordback(c)
    int c;
{
    USE(c);
    if (Cursor == InputBuf)
	return(CC_ERROR);
    /* else */

    Cursor = c_prev_word(Cursor, InputBuf, Argument); /* bounds check */

    if (VImode) 
	if (ActionFlag & TCSHOP_DELETE) {
	    c_delfini();
	    return(CC_REFRESH);
	}

    RefCursor();
    return(CC_NORM);
}

#if defined(DSPMBYTE) /* BY TAGA Nayuta VERY THANKS */
/*ARGSUSED*/
static void
e_charfwd_mbyte(argument)
     int argument;
{
    if (!_enable_mbdisp)
	Cursor += argument;
    else
	while (0 < argument && Cursor < LastChar) {
	    if (Cursor + 1 != LastChar &&
		Ismbyte1(*Cursor) && Ismbyte2(*(Cursor + 1))) {
		Cursor++;
	    }
	    Cursor++;
	    argument--;
	}
}
#endif

/*ARGSUSED*/
CCRETVAL
e_charfwd(c)
    int c;
{
    USE(c);
    if (Cursor < LastChar) {
#if defined(DSPMBYTE) /* BY TAGA Nayuta VERY THANKS */
	e_charfwd_mbyte(Argument);
#else
	Cursor += Argument;
#endif
	if (Cursor > LastChar)
	    Cursor = LastChar;

	if (VImode)
	    if (ActionFlag & TCSHOP_DELETE) {
		c_delfini();
		return(CC_REFRESH);
	    }

	RefCursor();
	return(CC_NORM);
    }
    else {
	return(CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
e_wordfwd(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar)
	return(CC_ERROR);
    /* else */

    Cursor = c_next_word(Cursor, LastChar, Argument);

    if (VImode)
	if (ActionFlag & TCSHOP_DELETE) {
	    c_delfini();
	    return(CC_REFRESH);
	}

    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_wordfwd(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar)
	return(CC_ERROR);
    /* else */

    Cursor = c_nexword(Cursor, LastChar, Argument);

    if (VImode)
	if (ActionFlag & TCSHOP_DELETE) {
	    c_delfini();
	    return(CC_REFRESH);
	}

    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_wordbegnext(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar)
	return(CC_ERROR);
    /* else */

    Cursor = c_next_word(Cursor, LastChar, Argument);
    if (Cursor < LastChar)
	Cursor++;

    if (VImode)
	if (ActionFlag & TCSHOP_DELETE) {
	    c_delfini();
	    return(CC_REFRESH);
	}

    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
static CCRETVAL
v_repeat_srch(c)
    int c;
{
    CCRETVAL rv = CC_ERROR;
#ifdef SDEBUG
    xprintf("dir %d patlen %d patbuf %S\n", 
	    c, patlen, patbuf);
#endif

    LastCmd = (KEYCMD) c;  /* Hack to stop c_hsetpat */
    LastChar = InputBuf;
    switch (c) {
    case F_DOWN_SEARCH_HIST:
	rv = e_down_search_hist(0);
	break;
    case F_UP_SEARCH_HIST:
	rv = e_up_search_hist(0);
	break;
    default:
	break;
    }
    return rv;
}

static CCRETVAL
v_csearch_back(ch, count, tflag)
    int ch, count, tflag;
{
    Char *cp;

    cp = Cursor;
    while (count--) {
	if (*cp == ch) 
	    cp--;
	while (cp > InputBuf && *cp != ch) 
	    cp--;
    }

    if (cp < InputBuf || (cp == InputBuf && *cp != ch))
	return(CC_ERROR);

    if (*cp == ch && tflag)
	cp++;

    Cursor = cp;

    if (ActionFlag & TCSHOP_DELETE) {
	Cursor++;
	c_delfini();
	return(CC_REFRESH);
    }

    RefCursor();
    return(CC_NORM);
}

static CCRETVAL
v_csearch_fwd(ch, count, tflag)
    int ch, count, tflag;
{
    Char *cp;

    cp = Cursor;
    while (count--) {
	if(*cp == ch) 
	    cp++;
	while (cp < LastChar && *cp != ch) 
	    cp++;
    }

    if (cp >= LastChar)
	return(CC_ERROR);

    if (*cp == ch && tflag)
	cp--;

    Cursor = cp;

    if (ActionFlag & TCSHOP_DELETE) {
	Cursor++;
	c_delfini();
	return(CC_REFRESH);
    }
    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
static CCRETVAL
v_action(c)
    int c;
{
    Char *cp, *kp;

    if (ActionFlag == TCSHOP_DELETE) {
	ActionFlag = TCSHOP_NOP;
	ActionPos = 0;
	
	UndoSize = 0;
	kp = UndoBuf;
	for (cp = InputBuf; cp < LastChar; cp++) {
	    *kp++ = *cp;
	    UndoSize++;
	}
		
	UndoAction = TCSHOP_INSERT;
	UndoPtr  = InputBuf;
	LastChar = InputBuf;
	Cursor   = InputBuf;
	if (c & TCSHOP_INSERT)
	    c_alternativ_key_map(0);
	    
	return(CC_REFRESH);
    }
#ifdef notdef
    else if (ActionFlag == TCSHOP_NOP) {
#endif
	ActionPos = Cursor;
	ActionFlag = c;
	return(CC_ARGHACK);  /* Do NOT clear out argument */
#ifdef notdef
    }
    else {
	ActionFlag = 0;
	ActionPos = 0;
	return(CC_ERROR);
    }
#endif
}

#ifdef COMMENT
/* by: Brian Allison <uiucdcs!convex!allison@RUTGERS.EDU> */
static void
c_get_word(begin, end)
    Char  **begin;
    Char  **end;
{
    Char   *cp;

    cp = &Cursor[0];
    while (Argument--) {
	while ((cp <= LastChar) && (isword(*cp)))
	    cp++;
	*end = --cp;
	while ((cp >= InputBuf) && (isword(*cp)))
	    cp--;
	*begin = ++cp;
    }
}
#endif /* COMMENT */

/*ARGSUSED*/
CCRETVAL
e_uppercase(c)
    int c;
{
    Char   *cp, *end;

    USE(c);
    end = c_next_word(Cursor, LastChar, Argument);

    for (cp = Cursor; cp < end; cp++)	/* PWP: was cp=begin */
	if (Islower(*cp))
	    *cp = Toupper(*cp);

    Cursor = end;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return(CC_REFRESH);
}


/*ARGSUSED*/
CCRETVAL
e_capitolcase(c)
    int c;
{
    Char   *cp, *end;

    USE(c);
    end = c_next_word(Cursor, LastChar, Argument);

    cp = Cursor;
    for (; cp < end; cp++) {
	if (Isalpha(*cp)) {
	    if (Islower(*cp))
		*cp = Toupper(*cp);
	    cp++;
	    break;
	}
    }
    for (; cp < end; cp++)
	if (Isupper(*cp))
	    *cp = Tolower(*cp);

    Cursor = end;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_lowercase(c)
    int c;
{
    Char   *cp, *end;

    USE(c);
    end = c_next_word(Cursor, LastChar, Argument);

    for (cp = Cursor; cp < end; cp++)
	if (Isupper(*cp))
	    *cp = Tolower(*cp);

    Cursor = end;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return(CC_REFRESH);
}


/*ARGSUSED*/
CCRETVAL
e_set_mark(c)
    int c;
{
    USE(c);
    Mark = Cursor;
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_exchange_mark(c)
    int c;
{
    Char *cp;

    USE(c);
    cp = Cursor;
    Cursor = Mark;
    Mark = cp;
    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_argfour(c)
    int c;
{				/* multiply current argument by 4 */
    USE(c);
    if (Argument > 1000000)
	return CC_ERROR;
    DoingArg = 1;
    Argument *= 4;
    return(CC_ARGHACK);
}

/*ARGSUSED*/
CCRETVAL
e_quote(c)
    int c;
{
    Char    ch;
    int     num;

    USE(c);
    QuoteModeOn();
    num = GetNextChar(&ch);
    QuoteModeOff();
    if (num == 1)
	return e_insert(ch);
    else
	return e_send_eof(0);
}

/*ARGSUSED*/
CCRETVAL
e_metanext(c)
    int c;
{
    USE(c);
    MetaNext = 1;
    return(CC_ARGHACK);	/* preserve argument */
}

#ifdef notdef
/*ARGSUSED*/
CCRETVAL
e_extendnext(c)
    int c;
{
    CurrentKeyMap = CcAltMap;
    return(CC_ARGHACK);	/* preserve argument */
}

#endif

/*ARGSUSED*/
CCRETVAL
v_insbeg(c)
    int c;
{				/* move to beginning of line and start vi
				 * insert mode */
    USE(c);
    Cursor = InputBuf;
    InsertPos = Cursor;

    UndoPtr  = Cursor;
    UndoAction = TCSHOP_DELETE;

    RefCursor();		/* move the cursor */
    c_alternativ_key_map(0);
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_replone(c)
    int c;
{				/* vi mode overwrite one character */
    USE(c);
    c_alternativ_key_map(0);
    inputmode = MODE_REPLACE_1;
    UndoAction = TCSHOP_CHANGE;	/* Set Up for VI undo command */
    UndoPtr = Cursor;
    UndoSize = 0;
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_replmode(c)
    int c;
{				/* vi mode start overwriting */
    USE(c);
    c_alternativ_key_map(0);
    inputmode = MODE_REPLACE;
    UndoAction = TCSHOP_CHANGE;	/* Set Up for VI undo command */
    UndoPtr = Cursor;
    UndoSize = 0;
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_substchar(c)
    int c;
{				/* vi mode substitute for one char */
    USE(c);
    c_delafter(Argument);
    c_alternativ_key_map(0);
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_substline(c)
    int c;
{				/* vi mode replace whole line */
    USE(c);
    (void) e_killall(0);
    c_alternativ_key_map(0);
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_chgtoend(c)
    int c;
{				/* vi mode change to end of line */
    USE(c);
    (void) e_killend(0);
    c_alternativ_key_map(0);
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_insert(c)
    int c;
{				/* vi mode start inserting */
    USE(c);
    c_alternativ_key_map(0);

    InsertPos = Cursor;
    UndoPtr = Cursor;
    UndoAction = TCSHOP_DELETE;

    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_add(c)
    int c;
{				/* vi mode start adding */
    USE(c);
    c_alternativ_key_map(0);
    if (Cursor < LastChar)
    {
	Cursor++;
	if (Cursor > LastChar)
	    Cursor = LastChar;
	RefCursor();
    }

    InsertPos = Cursor;
    UndoPtr = Cursor;
    UndoAction = TCSHOP_DELETE;

    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_addend(c)
    int c;
{				/* vi mode to add at end of line */
    USE(c);
    c_alternativ_key_map(0);
    Cursor = LastChar;

    InsertPos = LastChar;	/* Mark where insertion begins */
    UndoPtr = LastChar;
    UndoAction = TCSHOP_DELETE;

    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_change_case(cc)
    int cc;
{
    char    c;

    USE(cc);
    if (Cursor < LastChar) {
#ifndef WINNT_NATIVE
	c = *Cursor;
#else
	c = CHAR & *Cursor;
#endif /* WINNT_NATIVE */
	if (Isupper(c))
	    *Cursor++ = Tolower(c);
	else if (Islower(c))
	    *Cursor++ = Toupper(c);
	else
	    Cursor++;
	RefPlusOne();		/* fast refresh for one char */
	return(CC_NORM);
    }
    return(CC_ERROR);
}

/*ARGSUSED*/
CCRETVAL
e_expand(c)
    int c;
{
    Char *p;
    extern bool justpr;

    USE(c);
    for (p = InputBuf; Isspace(*p); p++)
	continue;
    if (p == LastChar)
	return(CC_ERROR);

    justpr++;
    Expand++;
    return(e_newline(0));
}

/*ARGSUSED*/
CCRETVAL
e_startover(c)
    int c;
{				/* erase all of current line, start again */
    USE(c);
    ResetInLine(0);		/* reset the input pointers */
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_redisp(c)
    int c;
{
    USE(c);
    ClearLines();
    ClearDisp();
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_cleardisp(c)
    int c;
{
    USE(c);
    ClearScreen();		/* clear the whole real screen */
    ClearDisp();		/* reset everything */
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_tty_int(c)
    int c;
{			
    USE(c);
#if defined(_MINIX) || defined(WINNT_NATIVE)
    /* SAK PATCH: erase all of current line, start again */
    ResetInLine(0);		/* reset the input pointers */
    xputchar('\n');
    ClearDisp();
    return (CC_REFRESH);
#else /* !_MINIX && !WINNT_NATIVE */
    /* do no editing */
    return (CC_NORM);
#endif /* _MINIX || WINNT_NATIVE */
}

/*
 * From: ghazi@cesl.rutgers.edu (Kaveh R. Ghazi)
 * Function to send a character back to the input stream in cooked
 * mode. Only works if we have TIOCSTI
 */
/*ARGSUSED*/
CCRETVAL
e_stuff_char(c)
     int c;
{
#ifdef TIOCSTI
     extern int Tty_raw_mode;
     int was_raw = Tty_raw_mode;
     char ch = (char) c;

     if (was_raw)
         (void) Cookedmode();

     (void) write(SHIN, "\n", 1);
     (void) ioctl(SHIN, TIOCSTI, (ioctl_t) &ch);

     if (was_raw)
         (void) Rawmode();
     return(e_redisp(c));
#else /* !TIOCSTI */  
     return(CC_ERROR);
#endif /* !TIOCSTI */  
}

/*ARGSUSED*/
CCRETVAL
e_insovr(c)
    int c;
{
    USE(c);
    inputmode = (inputmode == MODE_INSERT ? MODE_REPLACE : MODE_INSERT);
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_dsusp(c)
    int c;
{
    USE(c);
    /* do no editing */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_flusho(c)
    int c;
{
    USE(c);
    /* do no editing */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_quit(c)
    int c;
{
    USE(c);
    /* do no editing */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_tsusp(c)
    int c;
{
    USE(c);
    /* do no editing */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_stopo(c)
    int c;
{
    USE(c);
    /* do no editing */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_expand_history(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    c_substitute();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_magic_space(c)
    int c;
{
    USE(c);
    *LastChar = '\0';		/* just in case */
    c_substitute();
    return(e_insert(' '));
}

/*ARGSUSED*/
CCRETVAL
e_inc_fwd(c)
    int c;
{
    USE(c);
    patlen = 0;
    return e_inc_search(F_DOWN_SEARCH_HIST);
}


/*ARGSUSED*/
CCRETVAL
e_inc_back(c)
    int c;
{
    USE(c);
    patlen = 0;
    return e_inc_search(F_UP_SEARCH_HIST);
}

/*ARGSUSED*/
CCRETVAL
e_copyprev(c)
    int c;
{
    Char *cp, *oldc, *dp;

    USE(c);
    if (Cursor == InputBuf)
	return(CC_ERROR);
    /* else */

    oldc = Cursor;
    /* does a bounds check */
    cp = c_prev_word(Cursor, InputBuf, Argument);	

    c_insert((int)(oldc - cp));
    for (dp = oldc; cp < oldc && dp < LastChar; cp++)
	*dp++ = *cp;

    Cursor = dp;		/* put cursor at end */

    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_tty_starto(c)
    int c;
{
    USE(c);
    /* do no editing */
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_load_average(c)
    int c;
{
    USE(c);
    PastBottom();
#ifdef TIOCSTAT
    /*
     * Here we pass &c to the ioctl because some os's (NetBSD) expect it
     * there even if they don't use it. (lukem@netbsd.org)
     */
    if (ioctl(SHIN, TIOCSTAT, (ioctl_t) &c) < 0) 
#endif
	xprintf(CGETS(5, 1, "Load average unavailable\n"));
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_chgmeta(c)
    int c;
{
    USE(c);
    /*
     * Delete with insert == change: first we delete and then we leave in
     * insert mode.
     */
    return(v_action(TCSHOP_DELETE|TCSHOP_INSERT));
}

/*ARGSUSED*/
CCRETVAL
v_delmeta(c)
    int c;
{
    USE(c);
    return(v_action(TCSHOP_DELETE));
}


/*ARGSUSED*/
CCRETVAL
v_endword(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar)
	return(CC_ERROR);
    /* else */

    Cursor = c_endword(Cursor, LastChar, Argument, STRshwspace);

    if (ActionFlag & TCSHOP_DELETE)
    {
	Cursor++;
	c_delfini();
	return(CC_REFRESH);
    }

    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_eword(c)
    int c;
{
    USE(c);
    if (Cursor == LastChar)
	return(CC_ERROR);
    /* else */

    Cursor = c_eword(Cursor, LastChar, Argument);

    if (ActionFlag & TCSHOP_DELETE) {
	Cursor++;
	c_delfini();
	return(CC_REFRESH);
    }

    RefCursor();
    return(CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_char_fwd(c)
    int c;
{
    Char ch;

    USE(c);
    if (GetNextChar(&ch) != 1)
	return e_send_eof(0);

    srch_dir = CHAR_FWD;
    srch_char = ch;

    return v_csearch_fwd(ch, Argument, 0);

}

/*ARGSUSED*/
CCRETVAL
v_char_back(c)
    int c;
{
    Char ch;

    USE(c);
    if (GetNextChar(&ch) != 1)
	return e_send_eof(0);

    srch_dir = CHAR_BACK;
    srch_char = ch;

    return v_csearch_back(ch, Argument, 0);
}

/*ARGSUSED*/
CCRETVAL
v_charto_fwd(c)
    int c;
{
    Char ch;

    USE(c);
    if (GetNextChar(&ch) != 1)
	return e_send_eof(0);

    return v_csearch_fwd(ch, Argument, 1);

}

/*ARGSUSED*/
CCRETVAL
v_charto_back(c)
    int c;
{
    Char ch;

    USE(c);
    if (GetNextChar(&ch) != 1)
	return e_send_eof(0);

    return v_csearch_back(ch, Argument, 1);
}

/*ARGSUSED*/
CCRETVAL
v_rchar_fwd(c)
    int c;
{
    USE(c);
    if (srch_char == 0)
	return CC_ERROR;

    return srch_dir == CHAR_FWD ? v_csearch_fwd(srch_char, Argument, 0) : 
			          v_csearch_back(srch_char, Argument, 0);
}

/*ARGSUSED*/
CCRETVAL
v_rchar_back(c)
    int c;
{
    USE(c);
    if (srch_char == 0)
	return CC_ERROR;

    return srch_dir == CHAR_BACK ? v_csearch_fwd(srch_char, Argument, 0) : 
			           v_csearch_back(srch_char, Argument, 0);
}

/*ARGSUSED*/
CCRETVAL
v_undo(c)
    int c;
{
    int  loop;
    Char *kp, *cp;
    Char temp;
    int	 size;

    USE(c);
    switch (UndoAction) {
    case TCSHOP_DELETE|TCSHOP_INSERT:
    case TCSHOP_DELETE:
	if (UndoSize == 0) return(CC_NORM);
	cp = UndoPtr;
	kp = UndoBuf;
	for (loop=0; loop < UndoSize; loop++)	/* copy the chars */
	    *kp++ = *cp++;			/* into UndoBuf   */

	for (cp = UndoPtr; cp <= LastChar; cp++)
	    *cp = cp[UndoSize];

	LastChar -= UndoSize;
	Cursor   =  UndoPtr;
	
	UndoAction = TCSHOP_INSERT;
	break;

    case TCSHOP_INSERT:
	if (UndoSize == 0) return(CC_NORM);
	cp = UndoPtr;
	Cursor = UndoPtr;
	kp = UndoBuf;
	c_insert(UndoSize);		/* open the space, */
	for (loop = 0; loop < UndoSize; loop++)	/* copy the chars */
	    *cp++ = *kp++;

	UndoAction = TCSHOP_DELETE;
	break;

    case TCSHOP_CHANGE:
	if (UndoSize == 0) return(CC_NORM);
	cp = UndoPtr;
	Cursor = UndoPtr;
	kp = UndoBuf;
	size = (int)(Cursor-LastChar); /*  NOT NSL independant */
	if (size < UndoSize)
	    size = UndoSize;
	for(loop = 0; loop < size; loop++) {
	    temp = *kp;
	    *kp++ = *cp;
	    *cp++ = temp;
	}
	break;

    default:
	return(CC_ERROR);
    }

    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_ush_meta(c)
    int c;
{
    USE(c);
    return v_search(F_UP_SEARCH_HIST);
}

/*ARGSUSED*/
CCRETVAL
v_dsh_meta(c)
    int c;
{
    USE(c);
    return v_search(F_DOWN_SEARCH_HIST);
}

/*ARGSUSED*/
CCRETVAL
v_rsrch_fwd(c)
    int c;
{
    USE(c);
    if (patlen == 0) return(CC_ERROR);
    return(v_repeat_srch(searchdir));
}

/*ARGSUSED*/
CCRETVAL
v_rsrch_back(c)
    int c;
{
    USE(c);
    if (patlen == 0) return(CC_ERROR);
    return(v_repeat_srch(searchdir == F_UP_SEARCH_HIST ? 
			 F_DOWN_SEARCH_HIST : F_UP_SEARCH_HIST));
}

#ifndef WINNT_NATIVE
/* Since ed.defns.h  is generated from ed.defns.c, these empty 
   functions will keep the F_NUM_FNS consistent
 */
CCRETVAL
e_copy_to_clipboard(c)
    int c;
{
    USE(c);
    return CC_ERROR;
}

CCRETVAL
e_paste_from_clipboard(c)
    int c;
{
    USE(c);
    return (CC_ERROR);
}

CCRETVAL
e_dosify_next(c)
    int c;
{
    USE(c);
    return (CC_ERROR);
}
CCRETVAL
e_dosify_prev(c)
    int c;
{
    USE(c);
    return (CC_ERROR);
}
CCRETVAL
e_page_up(c)
    int c;
{
    USE(c);
    return (CC_ERROR);
}
CCRETVAL
e_page_down(c)
    int c;
{
    USE(c);
    return (CC_ERROR);
}
#endif /* !WINNT_NATIVE */

#ifdef notdef
void
MoveCursor(n)			/* move cursor + right - left char */
    int     n;
{
    Cursor = Cursor + n;
    if (Cursor < InputBuf)
	Cursor = InputBuf;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return;
}

Char *
GetCursor()
{
    return(Cursor);
}

int
PutCursor(p)
    Char   *p;
{
    if (p < InputBuf || p > LastChar)
	return 1;		/* Error */
    Cursor = p;
    return 0;
}
#endif
