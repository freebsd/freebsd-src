/* $Header: /src/pub/tcsh/tc.prompt.c,v 3.41 2000/11/11 23:03:39 christos Exp $ */
/*
 * tc.prompt.c: Prompt printing stuff
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
#include "sh.h"

RCSID("$Id: tc.prompt.c,v 3.41 2000/11/11 23:03:39 christos Exp $")

#include "ed.h"
#include "tw.h"

/*
 * kfk 21oct1983 -- add @ (time) and / ($cwd) in prompt.
 * PWP 4/27/87 -- rearange for tcsh.
 * mrdch@com.tau.edu.il 6/26/89 - added ~, T and .# - rearanged to switch()
 *                 instead of if/elseif
 * Luke Mewburn, <lukem@cs.rmit.edu.au>
 *	6-Sep-91	changed date format
 *	16-Feb-94	rewrote directory prompt code, added $ellipsis
 *	29-Dec-96	added rprompt support
 */

static char   *month_list[12];
static char   *day_list[7];

void
dateinit()
{
#ifdef notyet
  int i;

  setlocale(LC_TIME, "");

  for (i = 0; i < 12; i++)
      xfree((ptr_t) month_list[i]);
  month_list[0] = strsave(_time_info->abbrev_month[0]);
  month_list[1] = strsave(_time_info->abbrev_month[1]);
  month_list[2] = strsave(_time_info->abbrev_month[2]);
  month_list[3] = strsave(_time_info->abbrev_month[3]);
  month_list[4] = strsave(_time_info->abbrev_month[4]);
  month_list[5] = strsave(_time_info->abbrev_month[5]);
  month_list[6] = strsave(_time_info->abbrev_month[6]);
  month_list[7] = strsave(_time_info->abbrev_month[7]);
  month_list[8] = strsave(_time_info->abbrev_month[8]);
  month_list[9] = strsave(_time_info->abbrev_month[9]);
  month_list[10] = strsave(_time_info->abbrev_month[10]);
  month_list[11] = strsave(_time_info->abbrev_month[11]);

  for (i = 0; i < 7; i++)
      xfree((ptr_t) day_list[i]);
  day_list[0] = strsave(_time_info->abbrev_wkday[0]);
  day_list[1] = strsave(_time_info->abbrev_wkday[1]);
  day_list[2] = strsave(_time_info->abbrev_wkday[2]);
  day_list[3] = strsave(_time_info->abbrev_wkday[3]);
  day_list[4] = strsave(_time_info->abbrev_wkday[4]);
  day_list[5] = strsave(_time_info->abbrev_wkday[5]);
  day_list[6] = strsave(_time_info->abbrev_wkday[6]);
#else
  month_list[0] = "Jan";
  month_list[1] = "Feb";
  month_list[2] = "Mar";
  month_list[3] = "Apr";
  month_list[4] = "May";
  month_list[5] = "Jun";
  month_list[6] = "Jul";
  month_list[7] = "Aug";
  month_list[8] = "Sep";
  month_list[9] = "Oct";
  month_list[10] = "Nov";
  month_list[11] = "Dec";

  day_list[0] = "Sun";
  day_list[1] = "Mon";
  day_list[2] = "Tue";
  day_list[3] = "Wed";
  day_list[4] = "Thu";
  day_list[5] = "Fri";
  day_list[6] = "Sat";
#endif
}

void
printprompt(promptno, str)
    int     promptno;
    char   *str;
{
    static  Char *ocp = NULL;
    static  char *ostr = NULL;
    time_t  lclock = time(NULL);
    Char   *cp;

    switch (promptno) {
    default:
    case 0:
	cp = varval(STRprompt);
	break;
    case 1:
	cp = varval(STRprompt2);
	break;
    case 2:
	cp = varval(STRprompt3);
	break;
    case 3:
	if (ocp != NULL) {
	    cp = ocp;
	    str = ostr;
	}
	else 
	    cp = varval(STRprompt);
	break;
    }

    if (promptno < 2) {
	ocp = cp;
	ostr = str;
    }

    PromptBuf[0] = '\0';
    tprintf(FMT_PROMPT, PromptBuf, cp, 2 * INBUFSIZE - 2, str, lclock, NULL);

    if (!editing) {
	for (cp = PromptBuf; *cp ; )
	    (void) putraw(*cp++);
	SetAttributes(0);
	flush();
    }

    RPromptBuf[0] = '\0';
    if (promptno == 0) {	/* determine rprompt if using main prompt */
	cp = varval(STRrprompt);
	tprintf(FMT_PROMPT, RPromptBuf, cp, INBUFSIZE - 2, NULL, lclock, NULL);

				/* if not editing, put rprompt after prompt */
	if (!editing && RPromptBuf[0] != '\0') {
	    for (cp = RPromptBuf; *cp ; )
		(void) putraw(*cp++);
	    SetAttributes(0);
	    putraw(' ');
	    flush();
	}
    }
}

void
tprintf(what, buf, fmt, siz, str, tim, info)
    int what;
    Char *buf;
    const Char *fmt;
    size_t siz;
    char *str;
    time_t tim;
    ptr_t info;
{
    Char   *z, *q;
    Char    attributes = 0;
    static int print_prompt_did_ding = 0;
    Char    buff[BUFSIZE];
    /* Need to be unsigned to avoid sign extension */
    const unsigned char   *cz;
    unsigned char    cbuff[BUFSIZE];

    Char *p  = buf;
    Char *ep = &p[siz];
    const Char *cp = fmt;
    Char Scp;
    struct tm *t = localtime(&tim);

			/* prompt stuff */
    static Char *olddir = NULL, *olduser = NULL;
    extern int tlength;	/* cache cleared */
    int updirs, sz;
    size_t pdirs;

    for (; *cp; cp++) {
	if (p >= ep)
	    break;
#ifdef DSPMBYTE
	if (Ismbyte1(*cp) && ! (cp[1] == '\0'))
	{
	    *p++ = attributes | *cp++;	/* normal character */
	    *p++ = attributes | *cp;	/* normal character */
	}
	else
#endif /* DSPMBYTE */
	if ((*cp == '%') && ! (cp[1] == '\0')) {
	    cp++;
	    switch (*cp) {
	    case 'R':
		if (what == FMT_HISTORY)
		    fmthist('R', info, (char *) (cz = cbuff), sizeof(cbuff));
		else
		    cz = (unsigned char *) str;
		if (cz != NULL)
		    for (; *cz; *p++ = attributes | *cz++)
			if (p >= ep) break;
		break;
	    case '#':
		*p++ = attributes | ((uid == 0) ? PRCHROOT : PRCH);
		break;
	    case '!':
	    case 'h':
		switch (what) {
		case FMT_HISTORY:
		    fmthist('h', info, (char *) cbuff, sizeof(cbuff));
		    break;
		case FMT_SCHED:
		    (void) xsnprintf((char *) cbuff, sizeof(cbuff), "%d", 
			*(int *)info);
		    break;
		default:
		    (void) xsnprintf((char *) cbuff, sizeof(cbuff), "%d",
			eventno + 1);
		    break;
		}
		for (cz = cbuff; *cz; *p++ = attributes | *cz++)
		    if (p >= ep) break;
		break;
	    case 'T':		/* 24 hour format	 */
	    case '@':
	    case 't':		/* 12 hour am/pm format */
	    case 'p':		/* With seconds	*/
	    case 'P':
		{
		    char    ampm = 'a';
		    int     hr = t->tm_hour;

		    if (p >= ep - 10) break;

		    /* addition by Hans J. Albertsson */
		    /* and another adapted from Justin Bur */
		    if (adrof(STRampm) || (*cp != 'T' && *cp != 'P')) {
			if (hr >= 12) {
			    if (hr > 12)
				hr -= 12;
			    ampm = 'p';
			}
			else if (hr == 0)
			    hr = 12;
		    }		/* else do a 24 hour clock */

		    /* "DING!" stuff by Hans also */
		    if (t->tm_min || print_prompt_did_ding || 
			what != FMT_PROMPT || adrof(STRnoding)) {
			if (t->tm_min)
			    print_prompt_did_ding = 0;
			p = Itoa(hr, p, 0, attributes);
			*p++ = attributes | ':';
			p = Itoa(t->tm_min, p, 2, attributes);
			if (*cp == 'p' || *cp == 'P') {
			    *p++ = attributes | ':';
			    p = Itoa(t->tm_sec, p, 2, attributes);
			}
			if (adrof(STRampm) || (*cp != 'T' && *cp != 'P')) {
			    *p++ = attributes | ampm;
			    *p++ = attributes | 'm';
			}
		    }
		    else {	/* we need to ding */
			int     i = 0;

			(void) Strcpy(buff, STRDING);
			while (buff[i]) {
			    *p++ = attributes | buff[i++];
			}
			print_prompt_did_ding = 1;
		    }
		}
		break;

	    case 'M':
#ifndef HAVENOUTMP
		if (what == FMT_WHO)
		    cz = (unsigned char *) who_info(info, 'M',
			(char *) cbuff, sizeof(cbuff));
		else 
#endif /* HAVENOUTMP */
		    cz = (unsigned char *) getenv("HOST");
		/*
		 * Bug pointed out by Laurent Dami <dami@cui.unige.ch>: don't
		 * derefrence that NULL (if HOST is not set)...
		 */
		if (cz != NULL)
		    for (; *cz ; *p++ = attributes | *cz++)
			if (p >= ep) break;
		break;

	    case 'm':
#ifndef HAVENOUTMP
		if (what == FMT_WHO)
		    cz = (unsigned char *) who_info(info, 'm', (char *) cbuff,
			sizeof(cbuff));
		else 
#endif /* HAVENOUTMP */
		    cz = (unsigned char *) getenv("HOST");

		if (cz != NULL)
		    for ( ; *cz && (what == FMT_WHO || *cz != '.')
			  ; *p++ = attributes | *cz++ )
			if (p >= ep) break;
		break;

			/* lukem: new directory prompt code */
	    case '~':
	    case '/':
	    case '.':
	    case 'c':
	    case 'C':
		Scp = *cp;
		if (Scp == 'c')		/* store format type (c == .) */
		    Scp = '.';
		if ((z = varval(STRcwd)) == STRNULL)
		    break;		/* no cwd, so don't do anything */

			/* show ~ whenever possible - a la dirs */
		if (Scp == '~' || Scp == '.' ) {
		    if (tlength == 0 || olddir != z) {
			olddir = z;		/* have we changed dir? */
			olduser = getusername(&olddir);
		    }
		    if (olduser)
			z = olddir;
		}
		updirs = pdirs = 0;

			/* option to determine fixed # of dirs from path */
		if (Scp == '.' || Scp == 'C') {
		    int skip;
#ifdef WINNT_NATIVE
		    if (z[1] == ':') {
		    	*p++ = attributes | *z++;
		    	*p++ = attributes | *z++;
		    }
			if (*z == '/' && z[1] == '/') {
				*p++ = attributes | *z++;
				*p++ = attributes | *z++;
				do {
					*p++ = attributes | *z++;
				}while(*z != '/');
			}
#endif /* WINNT_NATIVE */
		    q = z;
		    while (*z)				/* calc # of /'s */
			if (*z++ == '/')
			    updirs++;
		    if ((Scp == 'C' && *q != '/'))
			updirs++;

		    if (cp[1] == '0') {			/* print <x> or ...  */
			pdirs = 1;
			cp++;
		    }
		    if (cp[1] >= '1' && cp[1] <= '9') {	/* calc # to skip  */
			skip = cp[1] - '0';
			cp++;
		    }
		    else
			skip = 1;

		    updirs -= skip;
		    while (skip-- > 0) {
			while ((z > q) && (*z != '/'))
			    z--;			/* back up */
			if (skip && z > q)
			    z--;
		    }
		    if (*z == '/' && z != q)
			z++;
		} /* . || C */

							/* print ~[user] */
		if ((olduser) && ((Scp == '~') ||
		     (Scp == '.' && (pdirs || (!pdirs && updirs <= 0))) )) {
		    *p++ = attributes | '~';
		    if (p >= ep) break;
		    for (q = olduser; *q; *p++ = attributes | *q++)
			if (p >= ep) break;
		}

			/* RWM - tell you how many dirs we've ignored */
			/*       and add '/' at front of this         */
		if (updirs > 0 && pdirs) {
		    if (p >= ep - 5) break;
		    if (adrof(STRellipsis)) {
			*p++ = attributes | '.';
			*p++ = attributes | '.';
			*p++ = attributes | '.';
		    } else {
			*p++ = attributes | '/';
			*p++ = attributes | '<';
			if (updirs > 9) {
			    *p++ = attributes | '9';
			    *p++ = attributes | '+';
			} else
			    *p++ = attributes | ('0' + updirs);
			*p++ = attributes | tcsh ? '>' : '%';
		    }
		}
		
		for (; *z ; *p++ = attributes | *z++)
		    if (p >= ep) break;
		break;
			/* lukem: end of new directory prompt code */

	    case 'n':
#ifndef HAVENOUTMP
		if (what == FMT_WHO) {
		    cz = (unsigned char *) who_info(info, 'n',
			(char *) cbuff, sizeof(cbuff));
		    for (; cz && *cz ; *p++ = attributes | *cz++)
			if (p >= ep) break;
		}
		else  
#endif /* HAVENOUTMP */
		{
		    if ((z = varval(STRuser)) != STRNULL)
			for (; *z; *p++ = attributes | *z++)
			    if (p >= ep) break;
		}
		break;
	    case 'l':
#ifndef HAVENOUTMP
		if (what == FMT_WHO) {
		    cz = (unsigned char *) who_info(info, 'l',
			(char *) cbuff, sizeof(cbuff));
		    for (; cz && *cz ; *p++ = attributes | *cz++)
			if (p >= ep) break;
		}
		else  
#endif /* HAVENOUTMP */
		{
		    if ((z = varval(STRtty)) != STRNULL)
			for (; *z; *p++ = attributes | *z++)
			    if (p >= ep) break;
		}
		break;
	    case 'd':
		for (cz = (unsigned char *) day_list[t->tm_wday]; *cz;
		    *p++ = attributes | *cz++)
		    if (p >= ep) break;
		break;
	    case 'D':
		if (p >= ep - 3) break;
		p = Itoa(t->tm_mday, p, 2, attributes);
		break;
	    case 'w':
		if (p >= ep - 5) break;
		for (cz = (unsigned char *) month_list[t->tm_mon]; *cz;
		    *p++ = attributes | *cz++)
		    if (p >= ep) break;
		break;
	    case 'W':
		if (p >= ep - 3) break;
		p = Itoa(t->tm_mon + 1, p, 2, attributes);
		break;
	    case 'y':
		if (p >= ep - 3) break;
		p = Itoa(t->tm_year % 100, p, 2, attributes);
		break;
	    case 'Y':
		if (p >= ep - 5) break;
		p = Itoa(t->tm_year + 1900, p, 4, attributes);
		break;
	    case 'S':		/* start standout */
		attributes |= STANDOUT;
		break;
	    case 'B':		/* start bold */
		attributes |= BOLD;
		break;
	    case 'U':		/* start underline */
		attributes |= UNDER;
		break;
	    case 's':		/* end standout */
		attributes &= ~STANDOUT;
		break;
	    case 'b':		/* end bold */
		attributes &= ~BOLD;
		break;
	    case 'u':		/* end underline */
		attributes &= ~UNDER;
		break;
	    case 'L':
		ClearToBottom();
		break;
	    case '?':
		if ((z = varval(STRstatus)) != STRNULL)
		    for (; *z; *p++ = attributes | *z++)
			if (p >= ep) break;
		break;
	    case '$':
		sz = (int) (ep - p);
		(void) expdollar(&p, &cp, &sz, attributes);
		break;
	    case '%':
		*p++ = attributes | '%';
		break;
	    case '{':		/* literal characters start */
#if LITERAL == 0
		/*
		 * No literal capability, so skip all chars in the literal
		 * string
		 */
		while (*cp != '\0' && (*cp != '%' || cp[1] != '}'))
		    cp++;
#endif				/* LITERAL == 0 */
		attributes |= LITERAL;
		break;
	    case '}':		/* literal characters end */
		attributes &= ~LITERAL;
		break;
	    default:
#ifndef HAVENOUTMP
		if (*cp == 'a' && what == FMT_WHO) {
		    cz = who_info(info, 'a', (char *) cbuff, sizeof(cbuff));
		    for (; cz && *cz; *p++ = attributes | *cz++)
			if (p >= ep) break;
		}
		else 
#endif /* HAVENOUTMP */
		{
		    if (p >= ep - 3) break;
		    *p++ = attributes | '%';
		    *p++ = attributes | *cp;
		}
		break;
	    }
	}
	else if (*cp == '\\' || *cp == '^') 
	    *p++ = attributes | parseescape(&cp);
	else if (*cp == HIST) {	/* EGS: handle '!'s in prompts */
	    if (what == FMT_HISTORY) 
		fmthist('h', info, (char *) cbuff, sizeof(cbuff));
	    else
		(void) xsnprintf((char *) cbuff, sizeof(cbuff), "%d", eventno + 1);
	    for (cz = cbuff; *cz; *p++ = attributes | *cz++)
		if (p >= ep) break;
	}
	else 
	    *p++ = attributes | *cp;	/* normal character */
    }
    *p = '\0';
}

Char *
expdollar(dstp, srcp, spp, attr)
    Char **dstp;
    const Char **srcp;
    size_t *spp;
    int	    attr;
{
    struct varent *vp;
    Char var[MAXVARLEN];
    const Char *src = *srcp;
    Char *val;
    Char *dst = *dstp;
    int i, curly = 0;

    /* found a variable, expand it */
    for (i = 0; i < MAXVARLEN; i++) {
	var[i] = *++src & TRIM;
	if (i == 0 && var[i] == '{') {
	    curly = 1;
	    var[i] = *++src & TRIM;
	}
	if (!alnum(var[i])) {
	    
	    var[i] = '\0';
	    break;
	}
    }
    if (curly && (*src & TRIM) == '}')
	src++;

    vp = adrof(var);
    val = (!vp) ? tgetenv(var) : NULL;
    if (vp) {
	for (i = 0; vp->vec[i] != NULL; i++) {
	    for (val = vp->vec[i]; *spp > 0 && *val; (*spp)--)
		*dst++ = *val++ | attr;
	    if (vp->vec[i+1] && *spp > 0) {
		*dst++ = ' ' | attr;
		(*spp)--;
	    }
	}
    }
    else if (val) {
	for (; *spp > 0 && *val; (*spp)--)
	    *dst++ = *val++ | attr;
    }
    else {
	**dstp = '\0';
	*srcp = src;
	return NULL;
    }
    *dst = '\0';

    val = *dstp;
    *srcp = src;
    *dstp = dst;

    return val;
}
