/* $Header: /src/pub/tcsh/ed.screen.c,v 3.50 2003/02/08 20:03:25 christos Exp $ */
/*
 * ed.screen.c: Editor/termcap-curses interface
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
#include "sh.h"

RCSID("$Id: ed.screen.c,v 3.50 2003/02/08 20:03:25 christos Exp $")

#include "ed.h"
#include "tc.h"
#include "ed.defns.h"

#ifndef POSIX
/*
 * We don't prototype these, cause some systems have them wrong!
 */
extern int   tgetent	__P(());
extern char *tgetstr	__P(());
extern int   tgetflag	__P(());
extern int   tgetnum	__P(());
extern char *tgoto	__P(());
# define PUTPURE putpure
# define PUTRAW putraw
#else
extern int   tgetent	__P((char *, char *));
extern char *tgetstr	__P((char *, char **));
extern int   tgetflag	__P((char *));
extern int   tgetnum	__P((char *));
extern char *tgoto	__P((char *, int, int));
extern void  tputs	__P((char *, int, void (*)(int)));
# define PUTPURE ((void (*)__P((int))) putpure)
# define PUTRAW ((void (*)__P((int))) putraw)
#endif


/* #define DEBUG_LITERAL */

/*
 * IMPORTANT NOTE: these routines are allowed to look at the current screen
 * and the current possition assuming that it is correct.  If this is not
 * true, then the update will be WRONG!  This is (should be) a valid
 * assumption...
 */

#define TC_BUFSIZE 2048

#define GoodStr(a) (tstr[a].str != NULL && tstr[a].str[0] != '\0')
#define Str(a) tstr[a].str
#define Val(a) tval[a].val

static struct {
    char   *b_name;
    int     b_rate;
}       baud_rate[] = {

#ifdef B0
    { "0", B0 },
#endif
#ifdef B50
    { "50", B50 },
#endif
#ifdef B75
    { "75", B75 },
#endif
#ifdef B110
    { "110", B110 },
#endif
#ifdef B134
    { "134", B134 },
#endif
#ifdef B150
    { "150", B150 },
#endif
#ifdef B200
    { "200", B200 },
#endif
#ifdef B300
    { "300", B300 },
#endif
#ifdef B600
    { "600", B600 },
#endif
#ifdef B900
    { "900", B900 },
#endif
#ifdef B1200
    { "1200", B1200 },
#endif
#ifdef B1800
    { "1800", B1800 },
#endif
#ifdef B2400
    { "2400", B2400 },
#endif
#ifdef B3600
    { "3600", B3600 },
#endif
#ifdef B4800
    { "4800", B4800 },
#endif
#ifdef B7200
    { "7200", B7200 },
#endif
#ifdef B9600
    { "9600", B9600 },
#endif
#ifdef EXTA
    { "19200", EXTA },
#endif
#ifdef B19200
    { "19200", B19200 },
#endif
#ifdef EXTB
    { "38400", EXTB },
#endif
#ifdef B38400
    { "38400", B38400 },
#endif
    { NULL, 0 }
};

#define T_al	0
#define T_bl	1
#define T_cd	2
#define T_ce	3
#define T_ch	4
#define T_cl	5
#define	T_dc	6
#define	T_dl	7
#define	T_dm	8
#define	T_ed	9
#define	T_ei	10
#define	T_fs	11
#define	T_ho	12
#define	T_ic	13
#define	T_im	14 
#define	T_ip	15
#define	T_kd	16
#define	T_kl	17
#define T_kr	18
#define T_ku	19
#define T_md	20
#define T_me	21
#define T_nd	22
#define T_se	23
#define T_so	24
#define T_ts	25
#define T_up	26
#define T_us	27
#define T_ue	28
#define T_vb	29
#define T_DC	30
#define T_DO	31
#define T_IC	32
#define T_LE	33
#define T_RI	34
#define T_UP	35
#define T_kh    36
#define T_at7   37
#define T_str   38
static struct termcapstr {
    char   *name;
    char   *long_name;
    char   *str;
} tstr[T_str + 1];


#define T_am	0
#define T_pt	1
#define T_li	2
#define T_co	3
#define T_km	4
#define T_xn	5
#define T_val	6
static struct termcapval {
    char   *name;
    char   *long_name;
    int     val;
} tval[T_val + 1];

void
terminit()
{
#ifdef NLS_CATALOGS
    int i;

    for (i = 0; i < T_str + 1; i++)
	xfree((ptr_t) tstr[i].long_name);

    for (i = 0; i < T_val + 1; i++)
	xfree((ptr_t) tval[i].long_name);
#endif

    tstr[T_al].name = "al";
    tstr[T_al].long_name = CSAVS(4, 1, "add new blank line");

    tstr[T_bl].name = "bl";
    tstr[T_bl].long_name = CSAVS(4, 2, "audible bell");

    tstr[T_cd].name = "cd";
    tstr[T_cd].long_name = CSAVS(4, 3, "clear to bottom");

    tstr[T_ce].name = "ce";
    tstr[T_ce].long_name = CSAVS(4, 4, "clear to end of line");

    tstr[T_ch].name = "ch";
    tstr[T_ch].long_name = CSAVS(4, 5, "cursor to horiz pos");

    tstr[T_cl].name = "cl";
    tstr[T_cl].long_name = CSAVS(4, 6, "clear screen");

    tstr[T_dc].name = "dc";
    tstr[T_dc].long_name = CSAVS(4, 7, "delete a character");

    tstr[T_dl].name = "dl";
    tstr[T_dl].long_name = CSAVS(4, 8, "delete a line");

    tstr[T_dm].name = "dm";
    tstr[T_dm].long_name = CSAVS(4, 9, "start delete mode");

    tstr[T_ed].name = "ed";
    tstr[T_ed].long_name = CSAVS(4, 10, "end delete mode");

    tstr[T_ei].name = "ei";
    tstr[T_ei].long_name = CSAVS(4, 11, "end insert mode");

    tstr[T_fs].name = "fs";
    tstr[T_fs].long_name = CSAVS(4, 12, "cursor from status line");

    tstr[T_ho].name = "ho";
    tstr[T_ho].long_name = CSAVS(4, 13, "home cursor");

    tstr[T_ic].name = "ic";
    tstr[T_ic].long_name = CSAVS(4, 14, "insert character");

    tstr[T_im].name = "im";
    tstr[T_im].long_name = CSAVS(4, 15, "start insert mode");

    tstr[T_ip].name = "ip";
    tstr[T_ip].long_name = CSAVS(4, 16, "insert padding");

    tstr[T_kd].name = "kd";
    tstr[T_kd].long_name = CSAVS(4, 17, "sends cursor down");

    tstr[T_kl].name = "kl";
    tstr[T_kl].long_name = CSAVS(4, 18, "sends cursor left");

    tstr[T_kr].name = "kr";
    tstr[T_kr].long_name = CSAVS(4, 19, "sends cursor right");

    tstr[T_ku].name = "ku";
    tstr[T_ku].long_name = CSAVS(4, 20, "sends cursor up");

    tstr[T_md].name = "md";
    tstr[T_md].long_name = CSAVS(4, 21, "begin bold");

    tstr[T_me].name = "me";
    tstr[T_me].long_name = CSAVS(4, 22, "end attributes");

    tstr[T_nd].name = "nd";
    tstr[T_nd].long_name = CSAVS(4, 23, "non destructive space");

    tstr[T_se].name = "se";
    tstr[T_se].long_name = CSAVS(4, 24, "end standout");

    tstr[T_so].name = "so";
    tstr[T_so].long_name = CSAVS(4, 25, "begin standout");

    tstr[T_ts].name = "ts";
    tstr[T_ts].long_name = CSAVS(4, 26, "cursor to status line");

    tstr[T_up].name = "up";
    tstr[T_up].long_name = CSAVS(4, 27, "cursor up one");

    tstr[T_us].name = "us";
    tstr[T_us].long_name = CSAVS(4, 28, "begin underline");

    tstr[T_ue].name = "ue";
    tstr[T_ue].long_name = CSAVS(4, 29, "end underline");

    tstr[T_vb].name = "vb";
    tstr[T_vb].long_name = CSAVS(4, 30, "visible bell");

    tstr[T_DC].name = "DC";
    tstr[T_DC].long_name = CSAVS(4, 31, "delete multiple chars");

    tstr[T_DO].name = "DO";
    tstr[T_DO].long_name = CSAVS(4, 32, "cursor down multiple");

    tstr[T_IC].name = "IC";
    tstr[T_IC].long_name = CSAVS(4, 33, "insert multiple chars");

    tstr[T_LE].name = "LE";
    tstr[T_LE].long_name = CSAVS(4, 34, "cursor left multiple");

    tstr[T_RI].name = "RI";
    tstr[T_RI].long_name = CSAVS(4, 35, "cursor right multiple");

    tstr[T_UP].name = "UP";
    tstr[T_UP].long_name = CSAVS(4, 36, "cursor up multiple");

    tstr[T_kh].name = "kh";
    tstr[T_kh].long_name = CSAVS(4, 37, "send cursor home");

    tstr[T_at7].name = "@7";
    tstr[T_at7].long_name = CSAVS(4, 38, "send cursor end");

    tstr[T_str].name = NULL;
    tstr[T_str].long_name = NULL;


    tval[T_am].name = "am";
    tval[T_am].long_name = CSAVS(4, 37, "Has automatic margins");

    tval[T_pt].name = "pt";
    tval[T_pt].long_name = CSAVS(4, 38, "Can use physical tabs");

    tval[T_li].name = "li";
    tval[T_li].long_name = CSAVS(4, 39, "Number of lines");

    tval[T_co].name = "co";
    tval[T_co].long_name = CSAVS(4, 40, "Number of columns");

    tval[T_km].name = "km";
    tval[T_km].long_name = CSAVS(4, 41, "Has meta key");

    tval[T_xn].name = "xn";
    tval[T_xn].long_name = CSAVS(4, 42, "Newline ignored at right margin");

    tval[T_val].name = NULL;
    tval[T_val].long_name = NULL;
}

/*
 * A very useful table from justin@crim.ca (Justin Bur) :-)
 * (Modified by per@erix.ericsson.se (Per Hedeland)
 *  - first (and second:-) case fixed)
 *
 * Description     Termcap variables       tcsh behavior
 * 		   am      xn              UseRightmost    SendCRLF
 * --------------  ------- -------         ------------    ------------
 * Automargins     yes     no              yes             no
 * Magic Margins   yes     yes             yes             no
 * No Wrap         no      --              yes             yes
 */

static bool me_all = 0;		/* does two or more of the attributes use me */

static	void	ReBufferDisplay	__P((void));
static	void	TCalloc		__P((struct termcapstr *, char *)); 


static void
TCalloc(t, cap)
    struct termcapstr *t;
    char   *cap;
{
    static char termcap_alloc[TC_BUFSIZE];
    char    termbuf[TC_BUFSIZE];
    struct termcapstr *ts;
    static int tloc = 0;
    int     tlen, clen;

    if (cap == NULL || *cap == '\0') {
	t->str = NULL;
	return;
    }
    else
	clen = strlen(cap);

    if (t->str == NULL)
	tlen = 0;
    else
	tlen = strlen(t->str);

    /*
     * New string is shorter; no need to allocate space
     */
    if (clen <= tlen) {
	(void) strcpy(t->str, cap);
	return;
    }

    /*
     * New string is longer; see if we have enough space to append
     */
    if (tloc + 3 < TC_BUFSIZE) {
	(void) strcpy(t->str = &termcap_alloc[tloc], cap);
	tloc += clen + 1;	/* one for \0 */
	return;
    }

    /*
     * Compact our buffer; no need to check compaction, cause we know it
     * fits...
     */
    tlen = 0;
    for (ts = tstr; ts->name != NULL; ts++)
	if (t != ts && ts->str != NULL && ts->str[0] != '\0') {
	    char   *ptr;

	    for (ptr = ts->str; *ptr != '\0'; termbuf[tlen++] = *ptr++)
		continue;
	    termbuf[tlen++] = '\0';
	}
    (void) memmove((ptr_t) termcap_alloc, (ptr_t) termbuf, (size_t) TC_BUFSIZE);
    tloc = tlen;
    if (tloc + 3 >= TC_BUFSIZE) {
	stderror(ERR_NAME | ERR_TCNOSTR);
	return;
    }
    (void) strcpy(t->str = &termcap_alloc[tloc], cap);
    tloc += clen + 1;		/* one for \0 */
    return;
}


/*ARGSUSED*/
void
TellTC(what)
    char   *what;
{
    struct termcapstr *t;

    USE(what);
    xprintf(CGETS(7, 1, "\n\tTcsh thinks your terminal has the\n"));
    xprintf(CGETS(7, 2, "\tfollowing characteristics:\n\n"));
    xprintf(CGETS(7, 3, "\tIt has %d columns and %d lines\n"),
	    Val(T_co), Val(T_li));
    xprintf(CGETS(7, 4, "\tIt has %s meta key\n"), T_HasMeta ?
	    CGETS(7, 5, "a") : CGETS(7, 6, "no"));
    xprintf(CGETS(7, 7, "\tIt can%s use tabs\n"), T_Tabs ?
	    "" : CGETS(7, 8, " not"));
    xprintf(CGETS(7, 9, "\tIt %s automatic margins\n"),
		    (T_Margin&MARGIN_AUTO)?
		    CGETS(7, 10, "has"):
		    CGETS(7, 11, "does not have"));
    if (T_Margin & MARGIN_AUTO)
	xprintf(CGETS(7, 12, "\tIt %s magic margins\n"),
			(T_Margin & MARGIN_MAGIC) ?
			CGETS(7, 10, "has"):
			CGETS(7, 11, "does not have"));

    for (t = tstr; t->name != NULL; t++)
	xprintf("\t%36s (%s) == %s\n", t->long_name, t->name,
		t->str && *t->str ? t->str : CGETS(7, 13, "(empty)"));
    xputchar('\n');
}


static void
ReBufferDisplay()
{
    register int i;
    Char  **b;
    Char  **bufp;

    b = Display;
    Display = NULL;
    if (b != NULL) {
	for (bufp = b; *bufp != NULL; bufp++)
	    xfree((ptr_t) * bufp);
	xfree((ptr_t) b);
    }
    b = Vdisplay;
    Vdisplay = NULL;
    if (b != NULL) {
	for (bufp = b; *bufp != NULL; bufp++)
	    xfree((ptr_t) * bufp);
	xfree((ptr_t) b);
    }
    TermH = Val(T_co);
    TermV = (INBUFSIZE * 4) / TermH + 1;
    b = (Char **) xmalloc((size_t) (sizeof(Char *) * (TermV + 1)));
    for (i = 0; i < TermV; i++)
	b[i] = (Char *) xmalloc((size_t) (sizeof(Char) * (TermH + 1)));
    b[TermV] = NULL;
    Display = b;
    b = (Char **) xmalloc((size_t) (sizeof(Char *) * (TermV + 1)));
    for (i = 0; i < TermV; i++)
	b[i] = (Char *) xmalloc((size_t) (sizeof(Char) * (TermH + 1)));
    b[TermV] = NULL;
    Vdisplay = b;
}

void
SetTC(what, how)
    char   *what, *how;
{
    struct termcapstr *ts;
    struct termcapval *tv;

    /*
     * Do the strings first
     */
    setname("settc");
    for (ts = tstr; ts->name != NULL; ts++)
	if (strcmp(ts->name, what) == 0)
	    break;
    if (ts->name != NULL) {
	TCalloc(ts, how);
	/*
	 * Reset variables
	 */
	if (GoodStr(T_me) && GoodStr(T_ue))
	    me_all = (strcmp(Str(T_me), Str(T_ue)) == 0);
	else
	    me_all = 0;
	if (GoodStr(T_me) && GoodStr(T_se))
	    me_all |= (strcmp(Str(T_me), Str(T_se)) == 0);

	T_CanCEOL = GoodStr(T_ce);
	T_CanDel = GoodStr(T_dc) || GoodStr(T_DC);
	T_CanIns = GoodStr(T_im) || GoodStr(T_ic) || GoodStr(T_IC);
	T_CanUP = GoodStr(T_up) || GoodStr(T_UP);
	return;
    }

    /*
     * Do the numeric ones second
     */
    for (tv = tval; tv->name != NULL; tv++)
	if (strcmp(tv->name, what) == 0)
	    break;

    if (tv->name != NULL) {
	if (tv == &tval[T_pt] || tv == &tval[T_km] || 
	    tv == &tval[T_am] || tv == &tval[T_xn]) {
	    if (strcmp(how, "yes") == 0)
		tv->val = 1;
	    else if (strcmp(how, "no") == 0)
		tv->val = 0;
	    else {
		stderror(ERR_SETTCUS, tv->name);
		return;
	    }
	    T_Tabs = (Char) Val(T_pt);
	    T_HasMeta = (Char) Val(T_km);
	    T_Margin = (Char) Val(T_am) ? MARGIN_AUTO : 0;
	    T_Margin |= (Char) Val(T_xn) ? MARGIN_MAGIC : 0;
	    if (tv == &tval[T_am] || tv == &tval[T_xn]) 
		ChangeSize(Val(T_li), Val(T_co));
	    return;
	}
	else {
	    tv->val = atoi(how);
	    T_Cols = (Char) Val(T_co);
	    T_Lines = (Char) Val(T_li);
	    if (tv == &tval[T_co] || tv == &tval[T_li])
		ChangeSize(Val(T_li), Val(T_co));
	    return;
	}
    }
    stderror(ERR_NAME | ERR_TCCAP, what);
    return;
}


/*
 * Print the termcap string out with variable substitution
 */
void
EchoTC(v)
    Char  **v;
{
    char   *cap, *scap, cv[BUFSIZE];
    int     arg_need, arg_cols, arg_rows;
    int     verbose = 0, silent = 0;
    char   *area;
    static char *fmts = "%s\n", *fmtd = "%d\n";
    struct termcapstr *t;
    char    buf[TC_BUFSIZE];

    area = buf;

    setname("echotc");

    tglob(v);
    if (gflag) {
	v = globall(v);
	if (v == 0)
	    stderror(ERR_NAME | ERR_NOMATCH);
    }
    else
	v = gargv = saveblk(v);
    trim(v);

    if (!*v || *v[0] == '\0')
	return;
    if (v[0][0] == '-') {
	switch (v[0][1]) {
	case 'v':
	    verbose = 1;
	    break;
	case 's':
	    silent = 1;
	    break;
	default:
	    stderror(ERR_NAME | ERR_TCUSAGE);
	    break;
	}
	v++;
    }
    if (!*v || *v[0] == '\0')
	return;
    (void) strcpy(cv, short2str(*v));
    if (strcmp(cv, "tabs") == 0) {
	xprintf(fmts, T_Tabs ? CGETS(7, 14, "yes") :
		CGETS(7, 15, "no"));
	flush();
	return;
    }
    else if (strcmp(cv, "meta") == 0) {
	xprintf(fmts, Val(T_km) ? CGETS(7, 14, "yes") :
		CGETS(7, 15, "no"));
	flush();
	return;
    }
    else if (strcmp(cv, "xn") == 0) {
	xprintf(fmts, T_Margin & MARGIN_MAGIC ? CGETS(7, 14, "yes") :
		CGETS(7, 15,  "no"));
	flush();
	return;
    }
    else if (strcmp(cv, "am") == 0) {
	xprintf(fmts, T_Margin & MARGIN_AUTO ? CGETS(7, 14, "yes") :
		CGETS(7, 15, "no"));
	flush();
	return;
    }
    else if (strcmp(cv, "baud") == 0) {
	int     i;

	for (i = 0; baud_rate[i].b_name != NULL; i++)
	    if (T_Speed == baud_rate[i].b_rate) {
		xprintf(fmts, baud_rate[i].b_name);
		flush();
		return;
	    }
	xprintf(fmtd, 0);
	flush();
	return;
    }
    else if (strcmp(cv, "rows") == 0 || strcmp(cv, "lines") == 0) {
	xprintf(fmtd, Val(T_li));
	flush();
	return;
    }
    else if (strcmp(cv, "cols") == 0) {
	xprintf(fmtd, Val(T_co));
	flush();
	return;
    }

    /* 
     * Try to use our local definition first
     */
    scap = NULL;
    for (t = tstr; t->name != NULL; t++)
	if (strcmp(t->name, cv) == 0) {
	    scap = t->str;
	    break;
	}
    if (t->name == NULL)
	scap = tgetstr(cv, &area);
    if (!scap || scap[0] == '\0') {
	if (tgetflag(cv)) {
	    xprintf(CGETS(7, 14, "yes\n"));
	    return;
	}
	if (silent)
	    return;
	else
	    stderror(ERR_NAME | ERR_TCCAP, cv);
    }

    /*
     * Count home many values we need for this capability.
     */
    for (cap = scap, arg_need = 0; *cap; cap++)
	if (*cap == '%')
	    switch (*++cap) {
	    case 'd':
	    case '2':
	    case '3':
	    case '.':
	    case '+':
		arg_need++;
		break;
	    case '%':
	    case '>':
	    case 'i':
	    case 'r':
	    case 'n':
	    case 'B':
	    case 'D':
		break;
	    default:
		/*
		 * hpux has lot's of them...
		 */
		if (verbose)
		    stderror(ERR_NAME | ERR_TCPARM, *cap);
		/* This is bad, but I won't complain */
		break;
	    }

    switch (arg_need) {
    case 0:
	v++;
	if (*v && *v[0]) {
	    if (silent)
		return;
	    else
		stderror(ERR_NAME | ERR_TCARGS, cv, arg_need);
	}
	(void) tputs(scap, 1, PUTRAW);
	break;
    case 1:
	v++;
	if (!*v || *v[0] == '\0')
	    stderror(ERR_NAME | ERR_TCNARGS, cv, 1);
	arg_cols = 0;
	arg_rows = atoi(short2str(*v));
	v++;
	if (*v && *v[0]) {
	    if (silent)
		return;
	    else
		stderror(ERR_NAME | ERR_TCARGS, cv, arg_need);
	}
	(void) tputs(tgoto(scap, arg_cols, arg_rows), 1, PUTRAW);
	break;
    default:
	/* This is wrong, but I will ignore it... */
	if (verbose)
	    stderror(ERR_NAME | ERR_TCARGS, cv, arg_need);
	/*FALLTHROUGH*/
    case 2:
	v++;
	if (!*v || *v[0] == '\0') {
	    if (silent)
		return;
	    else
		stderror(ERR_NAME | ERR_TCNARGS, cv, 2);
	}
	arg_cols = atoi(short2str(*v));
	v++;
	if (!*v || *v[0] == '\0') {
	    if (silent)
		return;
	    else
		stderror(ERR_NAME | ERR_TCNARGS, cv, 2);
	}
	arg_rows = atoi(short2str(*v));
	v++;
	if (*v && *v[0]) {
	    if (silent)
		return;
	    else
		stderror(ERR_NAME | ERR_TCARGS, cv, arg_need);
	}
	(void) tputs(tgoto(scap, arg_cols, arg_rows), arg_rows, PUTRAW);
	break;
    }
    flush();
    if (gargv) {
	blkfree(gargv);
	gargv = 0;
    }
}

bool    GotTermCaps = 0;

static struct {
    Char   *name;
    int     key;
    XmapVal fun;
    int	    type;
} arrow[] = {
#define A_K_DN	0
    { STRdown,	T_kd },
#define A_K_UP	1
    { STRup,	T_ku },
#define A_K_LT	2
    { STRleft,	T_kl },
#define A_K_RT	3
    { STRright, T_kr },
#define A_K_HO  4
    { STRhome,  T_kh },
#define A_K_EN  5
    { STRend,   T_at7}
};
#define A_K_NKEYS 6

void
ResetArrowKeys()
{
    arrow[A_K_DN].fun.cmd = F_DOWN_HIST;
    arrow[A_K_DN].type    = XK_CMD;

    arrow[A_K_UP].fun.cmd = F_UP_HIST;
    arrow[A_K_UP].type    = XK_CMD;

    arrow[A_K_LT].fun.cmd = F_CHARBACK;
    arrow[A_K_LT].type    = XK_CMD;

    arrow[A_K_RT].fun.cmd = F_CHARFWD;
    arrow[A_K_RT].type    = XK_CMD;

    arrow[A_K_HO].fun.cmd = F_TOBEG;
    arrow[A_K_HO].type    = XK_CMD;

    arrow[A_K_EN].fun.cmd = F_TOEND;
    arrow[A_K_EN].type    = XK_CMD;
}

void
DefaultArrowKeys() 
{
    static Char strA[] = {033, '[', 'A', '\0'};
    static Char strB[] = {033, '[', 'B', '\0'};
    static Char strC[] = {033, '[', 'C', '\0'};
    static Char strD[] = {033, '[', 'D', '\0'};
    static Char strH[] = {033, '[', 'H', '\0'};
    static Char strF[] = {033, '[', 'F', '\0'};
    static Char stOA[] = {033, 'O', 'A', '\0'};
    static Char stOB[] = {033, 'O', 'B', '\0'};
    static Char stOC[] = {033, 'O', 'C', '\0'};
    static Char stOD[] = {033, 'O', 'D', '\0'};
    static Char stOH[] = {033, 'O', 'H', '\0'};
    static Char stOF[] = {033, 'O', 'F', '\0'};

    CStr cs;
#ifndef IS_ASCII
    if (strA[0] == 033)
    {
	strA[0] = CTL_ESC('\033');
	strB[0] = CTL_ESC('\033');
	strC[0] = CTL_ESC('\033');
	strD[0] = CTL_ESC('\033');
	strH[0] = CTL_ESC('\033');
	strF[0] = CTL_ESC('\033');
	stOA[0] = CTL_ESC('\033');
	stOB[0] = CTL_ESC('\033');
	stOC[0] = CTL_ESC('\033');
	stOD[0] = CTL_ESC('\033');
	stOH[0] = CTL_ESC('\033');
	stOF[0] = CTL_ESC('\033');
    }
#endif

    cs.len = 3;

    cs.buf = strA; AddXkey(&cs, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
    cs.buf = strB; AddXkey(&cs, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
    cs.buf = strC; AddXkey(&cs, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
    cs.buf = strD; AddXkey(&cs, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
    cs.buf = strH; AddXkey(&cs, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
    cs.buf = strF; AddXkey(&cs, &arrow[A_K_EN].fun, arrow[A_K_EN].type);
    cs.buf = stOA; AddXkey(&cs, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
    cs.buf = stOB; AddXkey(&cs, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
    cs.buf = stOC; AddXkey(&cs, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
    cs.buf = stOD; AddXkey(&cs, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
    cs.buf = stOH; AddXkey(&cs, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
    cs.buf = stOF; AddXkey(&cs, &arrow[A_K_EN].fun, arrow[A_K_EN].type);

    if (VImode) {
	cs.len = 2;
	cs.buf = &strA[1]; AddXkey(&cs, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	cs.buf = &strB[1]; AddXkey(&cs, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	cs.buf = &strC[1]; AddXkey(&cs, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	cs.buf = &strD[1]; AddXkey(&cs, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	cs.buf = &strH[1]; AddXkey(&cs, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	cs.buf = &strF[1]; AddXkey(&cs, &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	cs.buf = &stOA[1]; AddXkey(&cs, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	cs.buf = &stOB[1]; AddXkey(&cs, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	cs.buf = &stOC[1]; AddXkey(&cs, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	cs.buf = &stOD[1]; AddXkey(&cs, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	cs.buf = &stOH[1]; AddXkey(&cs, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	cs.buf = &stOF[1]; AddXkey(&cs, &arrow[A_K_EN].fun, arrow[A_K_EN].type);
    }
}


int
SetArrowKeys(name, fun, type)
    CStr *name;
    XmapVal *fun;
    int type;
{
    int i;
    for (i = 0; i < A_K_NKEYS; i++)
	if (Strcmp(name->buf, arrow[i].name) == 0) {
	    arrow[i].fun  = *fun;
	    arrow[i].type = type;
	    return 0;
	}
    return -1;
}

int
IsArrowKey(name)
    Char *name;
{
    int i;
    for (i = 0; i < A_K_NKEYS; i++)
	if (Strcmp(name, arrow[i].name) == 0)
	    return 1;
    return 0;
}

int
ClearArrowKeys(name)
    CStr *name;
{
    int i;
    for (i = 0; i < A_K_NKEYS; i++)
	if (Strcmp(name->buf, arrow[i].name) == 0) {
	    arrow[i].type = XK_NOD;
	    return 0;
	}
    return -1;
}

void
PrintArrowKeys(name)
    CStr *name;
{
    int i;

    for (i = 0; i < A_K_NKEYS; i++)
	if (name->len == 0 || Strcmp(name->buf, arrow[i].name) == 0)
	    if (arrow[i].type != XK_NOD) {
		CStr cs;
		cs.buf = arrow[i].name;
		cs.len = Strlen(cs.buf);
		(void) printOne(&cs, &arrow[i].fun, arrow[i].type);
	    }
}


void
BindArrowKeys()
{
    KEYCMD *map, *dmap;
    int     i, j;
    char   *p;
    CStr    cs;

    if (!GotTermCaps)
	return;
    map = VImode ? CcAltMap : CcKeyMap;
    dmap = VImode ? CcViCmdMap : CcEmacsMap;

    DefaultArrowKeys();

    for (i = 0; i < A_K_NKEYS; i++) {
	p = tstr[arrow[i].key].str;
	if (p && *p) {
	    j = (unsigned char) *p;
	    cs.buf = str2short(p);
	    cs.len = Strlen(cs.buf);
	    /*
	     * Assign the arrow keys only if:
	     *
	     * 1. They are multi-character arrow keys and the user 
	     *    has not re-assigned the leading character, or 
	     *    has re-assigned the leading character to be F_XKEY
	     * 2. They are single arrow keys pointing to an unassigned key.
	     */
	    if (arrow[i].type == XK_NOD) {
		ClearXkey(map, &cs);
	    }
	    else {
		if (p[1] && (dmap[j] == map[j] || map[j] == F_XKEY)) {
		    AddXkey(&cs, &arrow[i].fun, arrow[i].type);
		    map[j] = F_XKEY;
		}
		else if (map[j] == F_UNASSIGNED) {
		    ClearXkey(map, &cs);
		    if (arrow[i].type == XK_CMD)
			map[j] = arrow[i].fun.cmd;
		    else
			AddXkey(&cs, &arrow[i].fun, arrow[i].type);
		}
	    }
	}
    }
}

static Char cur_atr = 0;	/* current attributes */

void
SetAttributes(atr)
    int     atr;
{
    atr &= ATTRIBUTES;
    if (atr != cur_atr) {
	if (me_all && GoodStr(T_me)) {
	    if (((cur_atr & BOLD) && !(atr & BOLD)) ||
		((cur_atr & UNDER) && !(atr & UNDER)) ||
		((cur_atr & STANDOUT) && !(atr & STANDOUT))) {
		(void) tputs(Str(T_me), 1, PUTPURE);
		cur_atr = 0;
	    }
	}
	if ((atr & BOLD) != (cur_atr & BOLD)) {
	    if (atr & BOLD) {
		if (GoodStr(T_md) && GoodStr(T_me)) {
		    (void) tputs(Str(T_md), 1, PUTPURE);
		    cur_atr |= BOLD;
		}
	    }
	    else {
		if (GoodStr(T_md) && GoodStr(T_me)) {
		    (void) tputs(Str(T_me), 1, PUTPURE);
		    if ((cur_atr & STANDOUT) && GoodStr(T_se)) {
			(void) tputs(Str(T_se), 1, PUTPURE);
			cur_atr &= ~STANDOUT;
		    }
		    if ((cur_atr & UNDER) && GoodStr(T_ue)) {
			(void) tputs(Str(T_ue), 1, PUTPURE);
			cur_atr &= ~UNDER;
		    }
		    cur_atr &= ~BOLD;
		}
	    }
	}
	if ((atr & STANDOUT) != (cur_atr & STANDOUT)) {
	    if (atr & STANDOUT) {
		if (GoodStr(T_so) && GoodStr(T_se)) {
		    (void) tputs(Str(T_so), 1, PUTPURE);
		    cur_atr |= STANDOUT;
		}
	    }
	    else {
		if (GoodStr(T_se)) {
		    (void) tputs(Str(T_se), 1, PUTPURE);
		    cur_atr &= ~STANDOUT;
		}
	    }
	}
	if ((atr & UNDER) != (cur_atr & UNDER)) {
	    if (atr & UNDER) {
		if (GoodStr(T_us) && GoodStr(T_ue)) {
		    (void) tputs(Str(T_us), 1, PUTPURE);
		    cur_atr |= UNDER;
		}
	    }
	    else {
		if (GoodStr(T_ue)) {
		    (void) tputs(Str(T_ue), 1, PUTPURE);
		    cur_atr &= ~UNDER;
		}
	    }
	}
    }
}

/* PWP 6-27-88 -- if the tty driver thinks that we can tab, we ask termcap */
int
CanWeTab()
{
    return (Val(T_pt));
}

void
MoveToLine(where)		/* move to line <where> (first line == 0) */
    int     where;		/* as efficiently as possible; */
{
    int     del;

    if (where == CursorV)
	return;

    if (where > TermV) {
#ifdef DEBUG_SCREEN
	xprintf("MoveToLine: where is ridiculous: %d\r\n", where);
	flush();
#endif /* DEBUG_SCREEN */
	return;
    }

    del = where - CursorV;

    if (del > 0) {
	while (del > 0) {
	    if ((T_Margin & MARGIN_AUTO) && Display[CursorV][0] != '\0') {
		/* move without newline */
		MoveToChar(TermH - 1);
		so_write(&Display[CursorV][CursorH], 1); /* updates CursorH/V*/
		del--;
	    }
	    else {
		if ((del > 1) && GoodStr(T_DO)) {
		    (void) tputs(tgoto(Str(T_DO), del, del), del, PUTPURE);
		    del = 0;
		}
		else {
		    for ( ; del > 0; del--) 
			(void) putraw('\n');
		    CursorH = 0;	/* because the \n will become \r\n */
		}
	    }
	}
    }
    else {			/* del < 0 */
	if (GoodStr(T_UP) && (-del > 1 || !GoodStr(T_up)))
	    (void) tputs(tgoto(Str(T_UP), -del, -del), -del, PUTPURE);
	else {
	    int i;
	    if (GoodStr(T_up))
		for (i = 0; i < -del; i++)
		    (void) tputs(Str(T_up), 1, PUTPURE);
	}
    }
    CursorV = where;		/* now where is here */
}

void
MoveToChar(where)		/* move to character position (where) */
    int     where;
{				/* as efficiently as possible */
    int     del;

mc_again:
    if (where == CursorH)
	return;

    if (where >= TermH) {
#ifdef DEBUG_SCREEN
	xprintf("MoveToChar: where is riduculous: %d\r\n", where);
	flush();
#endif /* DEBUG_SCREEN */
	return;
    }

    if (!where) {		/* if where is first column */
	(void) putraw('\r');	/* do a CR */
	CursorH = 0;
	return;
    }

    del = where - CursorH;

    if ((del < -4 || del > 4) && GoodStr(T_ch))
	/* go there directly */
	(void) tputs(tgoto(Str(T_ch), where, where), where, PUTPURE);
    else {
	int i;
	if (del > 0) {		/* moving forward */
	    if ((del > 4) && GoodStr(T_RI))
		(void) tputs(tgoto(Str(T_RI), del, del), del, PUTPURE);
	    else {
		/* if I can do tabs, use them */
		if (T_Tabs
#ifdef DSPMBYTE
		    && !_enable_mbdisp
#endif /* DSPMBYTE */
		) {
		    if ((CursorH & 0370) != (where & 0370)) {
			/* if not within tab stop */
			for (i = (CursorH & 0370); i < (where & 0370); i += 8)
			    (void) putraw('\t');	/* then tab over */
			CursorH = where & 0370;
			/* Note: considering that we often want to go to
			   TermH - 1 for the wrapping, it would be nice to
			   optimize this case by tabbing to the last column
			   - but this doesn't work for all terminals! */
		    }
		}
		/* it's usually cheaper to just write the chars, so we do. */

		/* NOTE THAT so_write() WILL CHANGE CursorH!!! */
		so_write(&Display[CursorV][CursorH], where - CursorH);

	    }
	}
	else {			/* del < 0 := moving backward */
	    if ((-del > 4) && GoodStr(T_LE))
		(void) tputs(tgoto(Str(T_LE), -del, -del), -del, PUTPURE);
	    else {		/* can't go directly there */
		/* if the "cost" is greater than the "cost" from col 0 */
		if (T_Tabs ? (-del > ((where >> 3) + (where & 07)))
		    : (-del > where)) {
		    (void) putraw('\r');	/* do a CR */
		    CursorH = 0;
		    goto mc_again;	/* and try again */
		}
		for (i = 0; i < -del; i++)
		    (void) putraw('\b');
	    }
	}
    }
    CursorH = where;		/* now where is here */
}

void
so_write(cp, n)
    register Char *cp;
    register int n;
{
    if (n <= 0)
	return;			/* catch bugs */

    if (n > TermH) {
#ifdef DEBUG_SCREEN
	xprintf("so_write: n is riduculous: %d\r\n", n);
	flush();
#endif /* DEBUG_SCREEN */
	return;
    }

    do {
	if (*cp & LITERAL) {
	    extern Char *litptr[];
	    Char   *d;

#ifdef DEBUG_LITERAL
	    xprintf("so: litnum %d, litptr %x\r\n",
		    *cp & CHAR, litptr[*cp & CHAR]);
#endif /* DEBUG_LITERAL */
	    for (d = litptr[*cp++ & CHAR]; *d & LITERAL; d++)
		(void) putraw(*d & CHAR);
	    (void) putraw(*d);

	}
	else
	    (void) putraw(*cp++);
	CursorH++;
    } while (--n);

    if (CursorH >= TermH) { /* wrap? */
	if (T_Margin & MARGIN_AUTO) { /* yes */
	    CursorH = 0;
	    CursorV++;
	    if (T_Margin & MARGIN_MAGIC) {
		/* force the wrap to avoid the "magic" situation */
		Char c;
		if ((c = Display[CursorV][CursorH]) != '\0')
		    so_write(&c, 1);
		else
		    (void) putraw(' ');
		CursorH = 1;
	    }
	}
	else			/* no wrap, but cursor stays on screen */
	    CursorH = TermH - 1;
    }
}


void
DeleteChars(num)		/* deletes <num> characters */
    int     num;
{
    if (num <= 0)
	return;

    if (!T_CanDel) {
#ifdef DEBUG_EDIT
	xprintf(CGETS(7, 16, "ERROR: cannot delete\r\n"));
#endif /* DEBUG_EDIT */
	flush();
	return;
    }

    if (num > TermH) {
#ifdef DEBUG_SCREEN
	xprintf(CGETS(7, 17, "DeleteChars: num is riduculous: %d\r\n"), num);
	flush();
#endif /* DEBUG_SCREEN */
	return;
    }

    if (GoodStr(T_DC))		/* if I have multiple delete */
	if ((num > 1) || !GoodStr(T_dc)) {	/* if dc would be more expen. */
	    (void) tputs(tgoto(Str(T_DC), num, num), num, PUTPURE);
	    return;
	}

    if (GoodStr(T_dm))		/* if I have delete mode */
	(void) tputs(Str(T_dm), 1, PUTPURE);

    if (GoodStr(T_dc))		/* else do one at a time */
	while (num--)
	    (void) tputs(Str(T_dc), 1, PUTPURE);

    if (GoodStr(T_ed))		/* if I have delete mode */
	(void) tputs(Str(T_ed), 1, PUTPURE);
}

void
Insert_write(cp, num)		/* Puts terminal in insert character mode, */
    register Char *cp;
    register int num;		/* or inserts num characters in the line */
{
    if (num <= 0)
	return;
    if (!T_CanIns) {
#ifdef DEBUG_EDIT
	xprintf(CGETS(7, 18, "ERROR: cannot insert\r\n"));
#endif /* DEBUG_EDIT */
	flush();
	return;
    }

    if (num > TermH) {
#ifdef DEBUG_SCREEN
	xprintf(CGETS(7, 19, "StartInsert: num is riduculous: %d\r\n"), num);
	flush();
#endif /* DEBUG_SCREEN */
	return;
    }

    if (GoodStr(T_IC))		/* if I have multiple insert */
	if ((num > 1) || !GoodStr(T_ic)) {	/* if ic would be more expen. */
	    (void) tputs(tgoto(Str(T_IC), num, num), num, PUTPURE);
	    so_write(cp, num);	/* this updates CursorH/V */
	    return;
	}

    if (GoodStr(T_im) && GoodStr(T_ei)) { /* if I have insert mode */
	(void) tputs(Str(T_im), 1, PUTPURE);

	CursorH += num;
	do 
	    (void) putraw(*cp++);
	while (--num);

	if (GoodStr(T_ip))	/* have to make num chars insert */
	    (void) tputs(Str(T_ip), 1, PUTPURE);

	(void) tputs(Str(T_ei), 1, PUTPURE);
	return;
    }

    do {
	if (GoodStr(T_ic))	/* have to make num chars insert */
	    (void) tputs(Str(T_ic), 1, PUTPURE);	/* insert a char */

	(void) putraw(*cp++);

	CursorH++;

	if (GoodStr(T_ip))	/* have to make num chars insert */
	    (void) tputs(Str(T_ip), 1, PUTPURE);/* pad the inserted char */

    } while (--num);

}

void
ClearEOL(num)			/* clear to end of line.  There are num */
    int     num;		/* characters to clear */
{
    register int i;

    if (num <= 0)
	return;

    if (T_CanCEOL && GoodStr(T_ce))
	(void) tputs(Str(T_ce), 1, PUTPURE);
    else {
	for (i = 0; i < num; i++)
	    (void) putraw(' ');
	CursorH += num;		/* have written num spaces */
    }
}

void
ClearScreen()
{				/* clear the whole screen and home */
    if (GoodStr(T_cl))
	/* send the clear screen code */
	(void) tputs(Str(T_cl), Val(T_li), PUTPURE);
    else if (GoodStr(T_ho) && GoodStr(T_cd)) {
	(void) tputs(Str(T_ho), Val(T_li), PUTPURE);	/* home */
	/* clear to bottom of screen */
	(void) tputs(Str(T_cd), Val(T_li), PUTPURE);
    }
    else {
	(void) putraw('\r');
	(void) putraw('\n');
    }
}

void
SoundBeep()
{				/* produce a sound */
    beep_cmd ();
    if (adrof(STRnobeep))
	return;

    if (GoodStr(T_vb) && adrof(STRvisiblebell))
	(void) tputs(Str(T_vb), 1, PUTPURE);	/* visible bell */
    else if (GoodStr(T_bl))
	/* what termcap says we should use */
	(void) tputs(Str(T_bl), 1, PUTPURE);
    else
	(void) putraw(CTL_ESC('\007'));	/* an ASCII bell; ^G */
}

void
ClearToBottom()
{				/* clear to the bottom of the screen */
    if (GoodStr(T_cd))
	(void) tputs(Str(T_cd), Val(T_li), PUTPURE);
    else if (GoodStr(T_ce))
	(void) tputs(Str(T_ce), Val(T_li), PUTPURE);
}

void
GetTermCaps()
{				/* read in the needed terminal capabilites */
    register int i;
    char   *ptr;
    char    buf[TC_BUFSIZE];
    static char bp[TC_BUFSIZE];
    char   *area;
    struct termcapstr *t;


#ifdef SIG_WINDOW
# ifdef BSDSIGS
    sigmask_t omask;
# endif /* BSDSIGS */
    int     lins, cols;

    /* don't want to confuse things here */
# ifdef BSDSIGS
    omask = sigblock(sigmask(SIG_WINDOW)) & ~sigmask(SIG_WINDOW);
# else /* BSDSIGS */
    (void) sighold(SIG_WINDOW);
# endif /* BSDSIGS */
#endif /* SIG_WINDOW */
    area = buf;

    GotTermCaps = 1;

    setname("gettermcaps");
    ptr = getenv("TERM");

#ifdef apollo
    /* 
     * If we are on a pad, we pretend that we are dumb. Otherwise the termcap
     * library will put us in a weird screen mode, thinking that we are going
     * to use curses
     */
    if (isapad())
	ptr = "dumb";
#endif /* apollo */

    if (!ptr || !ptr[0] || !strcmp(ptr, "wm") || !strcmp(ptr,"dmx"))
	ptr = "dumb";

    setzero(bp, TC_BUFSIZE);

    i = tgetent(bp, ptr);
    if (i <= 0) {
	if (i == -1) {
#if (SYSVREL == 0) || defined(IRIS3D)
	    xprintf(CGETS(7, 20, "%s: Cannot open /etc/termcap.\n"), progname);
	}
	else if (i == 0) {
#endif /* SYSVREL */
	    xprintf(CGETS(7, 21,
			  "%s: No entry for terminal type \"%s\"\n"), progname,
		    getenv("TERM"));
	}
	xprintf(CGETS(7, 22, "%s: using dumb terminal settings.\n"), progname);
	Val(T_co) = 80;		/* do a dumb terminal */
	Val(T_pt) = Val(T_km) = Val(T_li) = 0;
	for (t = tstr; t->name != NULL; t++)
	    TCalloc(t, NULL);
    }
    else {
	/* Can we tab */
	Val(T_pt) = tgetflag("pt") && !tgetflag("xt");
	/* do we have a meta? */
	Val(T_km) = (tgetflag("km") || tgetflag("MT"));
	Val(T_am) = tgetflag("am");
	Val(T_xn) = tgetflag("xn");
	Val(T_co) = tgetnum("co");
	Val(T_li) = tgetnum("li");
	for (t = tstr; t->name != NULL; t++)
	    TCalloc(t, tgetstr(t->name, &area));
    }
    if (Val(T_co) < 2)
	Val(T_co) = 80;		/* just in case */
    if (Val(T_li) < 1)
	Val(T_li) = 24;

    T_Cols = (Char) Val(T_co);
    T_Lines = (Char) Val(T_li);
    if (T_Tabs)
	T_Tabs = (Char) Val(T_pt);
    T_HasMeta = (Char) Val(T_km);
    T_Margin = (Char) Val(T_am) ? MARGIN_AUTO : 0;
    T_Margin |= (Char) Val(T_xn) ? MARGIN_MAGIC : 0;
    T_CanCEOL = GoodStr(T_ce);
    T_CanDel = GoodStr(T_dc) || GoodStr(T_DC);
    T_CanIns = GoodStr(T_im) || GoodStr(T_ic) || GoodStr(T_IC);
    T_CanUP = GoodStr(T_up) || GoodStr(T_UP);
    if (GoodStr(T_me) && GoodStr(T_ue))
	me_all = (strcmp(Str(T_me), Str(T_ue)) == 0);
    else
	me_all = 0;
    if (GoodStr(T_me) && GoodStr(T_se))
	me_all |= (strcmp(Str(T_me), Str(T_se)) == 0);


#ifdef DEBUG_SCREEN
    if (!T_CanUP) {
	xprintf(CGETS(7, 23, "%s: WARNING: Your terminal cannot move up.\n",
		progname));
	xprintf(CGETS(7, 24, "Editing may be odd for long lines.\n"));
    }
    if (!T_CanCEOL)
	xprintf(CGETS(7, 25, "no clear EOL capability.\n"));
    if (!T_CanDel)
	xprintf(CGETS(7, 26, "no delete char capability.\n"));
    if (!T_CanIns)
	xprintf(CGETS(7, 27, "no insert char capability.\n"));
#endif /* DEBUG_SCREEN */



#ifdef SIG_WINDOW
    (void) GetSize(&lins, &cols);	/* get the correct window size */
    ChangeSize(lins, cols);

# ifdef BSDSIGS
    (void) sigsetmask(omask);	/* can change it again */
# else /* BSDSIGS */
    (void) sigrelse(SIG_WINDOW);
# endif /* BSDSIGS */
#else /* SIG_WINDOW */
    ChangeSize(Val(T_li), Val(T_co));
#endif /* SIG_WINDOW */

    BindArrowKeys();
}

#ifdef SIG_WINDOW
/* GetSize():
 *	Return the new window size in lines and cols, and
 *	true if the size was changed. This can fail if SHIN
 *	is not a tty, but it will work in most cases.
 */
int
GetSize(lins, cols)
    int    *lins, *cols;
{
    *cols = Val(T_co);
    *lins = Val(T_li);

#ifdef TIOCGWINSZ
# define KNOWsize
# ifndef lint
    {
	struct winsize ws;	/* from 4.3 */

	if (ioctl(SHIN, TIOCGWINSZ, (ioctl_t) &ws) != -1) {
	    if (ws.ws_col)
		*cols = ws.ws_col;
	    if (ws.ws_row)
		*lins = ws.ws_row;
	}
    }
# endif /* !lint */
#else /* TIOCGWINSZ */
# ifdef TIOCGSIZE
#  define KNOWsize
    {
	struct ttysize ts;	/* from Sun */

	if (ioctl(SHIN, TIOCGSIZE, (ioctl_t) &ts) != -1) {
	    if (ts.ts_cols)
		*cols = ts.ts_cols;
	    if (ts.ts_lines)
		*lins = ts.ts_lines;
	}
    }
# endif /* TIOCGSIZE */
#endif /* TIOCGWINSZ */

    return (Val(T_co) != *cols || Val(T_li) != *lins);
}

#endif /* SIGWINDOW */

void
ChangeSize(lins, cols)
    int     lins, cols;
{
    /*
     * Just in case
     */
    Val(T_co) = (cols < 2) ? 80 : cols;
    Val(T_li) = (lins < 1) ? 24 : lins;

#ifdef KNOWsize
    /*
     * We want to affect the environment only when we have a valid
     * setup, not when we get bad settings. Consider the following scenario:
     * We just logged in, and we have not initialized the editor yet.
     * We reset termcap with tset, and not $TERMCAP has the right
     * terminal size. But since the editor is not initialized yet, and
     * the kernel's notion of the terminal size might be wrong we arrive
     * here with lines = columns = 0. If we reset the environment we lose
     * our only chance to get the window size right.
     */
    if (Val(T_co) == cols && Val(T_li) == lins) {
	Char    buf[10];
	char   *tptr;

	if (getenv("COLUMNS")) {
	    (void) Itoa(Val(T_co), buf, 0, 0);
	    tsetenv(STRCOLUMNS, buf);
	}

	if (getenv("LINES")) {
	    (void) Itoa(Val(T_li), buf, 0, 0);
	    tsetenv(STRLINES, buf);
	}

	if ((tptr = getenv("TERMCAP")) != NULL) {
	    /* Leave 64 characters slop in case we enlarge the termcap string */
	    Char    termcap[1024+64], backup[1024+64], *ptr;
	    int     i;

	    ptr = str2short(tptr);
	    (void) Strncpy(termcap, ptr, 1024);
	    termcap[1023] = '\0';

	    /* update termcap string; first do columns */
	    buf[0] = 'c';
	    buf[1] = 'o';
	    buf[2] = '#';
	    buf[3] = '\0';
	    if ((ptr = Strstr(termcap, buf)) == NULL) {
		(void) Strcpy(backup, termcap);
	    }
	    else {
		i = (int) (ptr - termcap + Strlen(buf));
		(void) Strncpy(backup, termcap, (size_t) i);
		backup[i] = '\0';
		(void) Itoa(Val(T_co), buf, 0, 0);
		(void) Strcat(backup + i, buf);
		ptr = Strchr(ptr, ':');
		(void) Strcat(backup, ptr);
	    }

	    /* now do lines */
	    buf[0] = 'l';
	    buf[1] = 'i';
	    buf[2] = '#';
	    buf[3] = '\0';
	    if ((ptr = Strstr(backup, buf)) == NULL) {
		(void) Strcpy(termcap, backup);
	    }
	    else {
		i = (int) (ptr - backup + Strlen(buf));
		(void) Strncpy(termcap, backup, (size_t) i);
		termcap[i] = '\0';
		(void) Itoa(Val(T_li), buf, 0, 0);
		(void) Strcat(termcap, buf);
		ptr = Strchr(ptr, ':');
		(void) Strcat(termcap, ptr);
	    }
	    /*
	     * Chop the termcap string at 1024 characters to avoid core-dumps
	     * in the termcap routines
	     */
	    termcap[1023] = '\0';
	    tsetenv(STRTERMCAP, termcap);
	}
    }
#endif /* KNOWsize */

    ReBufferDisplay();		/* re-make display buffers */
    ClearDisp();
}
