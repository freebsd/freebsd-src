/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
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

#if !defined(lint) && !defined(SCCSID)
static char sccsid[] = "@(#)tty.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint && not SCCSID */

/*
 * tty.c: tty interface stuff
 */
#include "sys.h"
#include "tty.h"
#include "el.h"

typedef struct ttymodes_t {
    char *m_name;
    int   m_value;
    int   m_type;
} ttymodes_t;

typedef struct ttymap_t {
    int nch, och;		 /* Internal and termio rep of chars */
    el_action_t bind[3]; 	/* emacs, vi, and vi-cmd */
} ttymap_t;


private ttyperm_t ttyperm = {
    {
	{ "iflag:", ICRNL, (INLCR|IGNCR) },
	{ "oflag:", (OPOST|ONLCR), ONLRET },
	{ "cflag:", 0, 0 },
	{ "lflag:", (ISIG|ICANON|ECHO|ECHOE|ECHOCTL|IEXTEN),
		    (NOFLSH|ECHONL|EXTPROC|FLUSHO) },
	{ "chars:", 	0, 0 },
    },
    {
	{ "iflag:", (INLCR|ICRNL), IGNCR },
	{ "oflag:", (OPOST|ONLCR), ONLRET },
	{ "cflag:", 0, 0 },
	{ "lflag:", ISIG,
		    (NOFLSH|ICANON|ECHO|ECHOK|ECHONL|EXTPROC|IEXTEN|FLUSHO) },
	{ "chars:", (C_SH(C_MIN)|C_SH(C_TIME)|C_SH(C_SWTCH)|C_SH(C_DSWTCH)|
		     C_SH(C_SUSP)|C_SH(C_DSUSP)|C_SH(C_EOL)|C_SH(C_DISCARD)|
		     C_SH(C_PGOFF)|C_SH(C_PAGE)|C_SH(C_STATUS)), 0 }
    },
    {
	{ "iflag:", 0, IXON | IXOFF | INLCR | ICRNL },
	{ "oflag:", 0, 0 },
	{ "cflag:", 0, 0 },
	{ "lflag:", 0, ISIG | IEXTEN },
	{ "chars:", 0, 0 },
    }
};

private ttychar_t ttychar = {
    {
	CINTR,		 CQUIT, 	 CERASE, 	   CKILL,
	CEOF, 		 CEOL, 		 CEOL2, 	   CSWTCH,
	CDSWTCH,	 CERASE2,	 CSTART, 	   CSTOP,
	CWERASE, 	 CSUSP, 	 CDSUSP, 	   CREPRINT,
	CDISCARD, 	 CLNEXT,	 CSTATUS,	   CPAGE,
	CPGOFF,		 CKILL2, 	 CBRK, 		   CMIN,
	CTIME
    },
    {
	CINTR, 		 CQUIT, 	  CERASE, 	   CKILL,
	_POSIX_VDISABLE, _POSIX_VDISABLE, _POSIX_VDISABLE, _POSIX_VDISABLE,
	_POSIX_VDISABLE, CERASE2,	  CSTART, 	   CSTOP,
	_POSIX_VDISABLE, CSUSP,           _POSIX_VDISABLE, _POSIX_VDISABLE,
	CDISCARD, 	 _POSIX_VDISABLE, _POSIX_VDISABLE, _POSIX_VDISABLE,
	_POSIX_VDISABLE, _POSIX_VDISABLE, _POSIX_VDISABLE, 1,
	0
    },
    {
	0,		 0,		  0,		   0,
	0,		 0,		  0,		   0,
	0,		 0,		  0,		   0,
	0,		 0,		  0,		   0,
	0,		 0,		  0,		   0,
	0,		 0,		  0,		   0,
	0
    }
};

private ttymap_t tty_map[] = {
#ifdef VERASE
	{ C_ERASE,   VERASE,
	    { ED_DELETE_PREV_CHAR, VI_DELETE_PREV_CHAR, ED_PREV_CHAR } },
#endif /* VERASE */
#ifdef VERASE2
	{ C_ERASE2,  VERASE2,
	    { ED_DELETE_PREV_CHAR, VI_DELETE_PREV_CHAR, ED_PREV_CHAR } },
#endif /* VERASE2 */
#ifdef VKILL
    	{ C_KILL,    VKILL,
	    { EM_KILL_LINE, VI_KILL_LINE_PREV, ED_UNASSIGNED } },
#endif /* VKILL */
#ifdef VKILL2
    	{ C_KILL2,   VKILL2,
	    { EM_KILL_LINE, VI_KILL_LINE_PREV, ED_UNASSIGNED } },
#endif /* VKILL2 */
#ifdef VEOF
    	{ C_EOF,     VEOF,
	    { EM_DELETE_OR_LIST, VI_LIST_OR_EOF, ED_UNASSIGNED } },
#endif /* VEOF */
#ifdef VWERASE
    	{ C_WERASE,  VWERASE,
	    { ED_DELETE_PREV_WORD, ED_DELETE_PREV_WORD, ED_PREV_WORD } },
#endif /* VWERASE */
#ifdef VREPRINT
   	{ C_REPRINT, VREPRINT,
	    { ED_REDISPLAY, ED_INSERT, ED_REDISPLAY } },
#endif /* VREPRINT */
#ifdef VLNEXT
    	{ C_LNEXT,   VLNEXT,
	    { ED_QUOTED_INSERT, ED_QUOTED_INSERT, ED_UNASSIGNED } },
#endif /* VLNEXT */
	{ -1,	     -1,
	    { ED_UNASSIGNED, ED_UNASSIGNED, ED_UNASSIGNED } }
    };

private ttymodes_t ttymodes[] = {
# ifdef	IGNBRK
    { "ignbrk",	IGNBRK,	M_INP },
# endif /* IGNBRK */
# ifdef	BRKINT
    { "brkint",	BRKINT,	M_INP },
# endif /* BRKINT */
# ifdef	IGNPAR
    { "ignpar",	IGNPAR,	M_INP },
# endif /* IGNPAR */
# ifdef	PARMRK
    { "parmrk",	PARMRK,	M_INP },
# endif /* PARMRK */
# ifdef	INPCK
    { "inpck",	INPCK,	M_INP },
# endif /* INPCK */
# ifdef	ISTRIP
    { "istrip",	ISTRIP,	M_INP },
# endif /* ISTRIP */
# ifdef	INLCR
    { "inlcr",	INLCR,	M_INP },
# endif /* INLCR */
# ifdef	IGNCR
    { "igncr",	IGNCR,	M_INP },
# endif /* IGNCR */
# ifdef	ICRNL
    { "icrnl",	ICRNL,	M_INP },
# endif /* ICRNL */
# ifdef	IUCLC
    { "iuclc",	IUCLC,	M_INP },
# endif /* IUCLC */
# ifdef	IXON
    { "ixon",	IXON,	M_INP },
# endif /* IXON */
# ifdef	IXANY
    { "ixany",	IXANY,	M_INP },
# endif /* IXANY */
# ifdef	IXOFF
    { "ixoff",	IXOFF,	M_INP },
# endif /* IXOFF */
# ifdef  IMAXBEL
    { "imaxbel",IMAXBEL,M_INP },
# endif /* IMAXBEL */

# ifdef	OPOST
    { "opost",	OPOST,	M_OUT },
# endif /* OPOST */
# ifdef	OLCUC
    { "olcuc",	OLCUC,	M_OUT },
# endif /* OLCUC */
# ifdef	ONLCR
    { "onlcr",	ONLCR,	M_OUT },
# endif /* ONLCR */
# ifdef	OCRNL
    { "ocrnl",	OCRNL,	M_OUT },
# endif /* OCRNL */
# ifdef	ONOCR
    { "onocr",	ONOCR,	M_OUT },
# endif /* ONOCR */
# ifdef ONOEOT
    { "onoeot",	ONOEOT,	M_OUT },
# endif /* ONOEOT */
# ifdef	ONLRET
    { "onlret",	ONLRET,	M_OUT },
# endif /* ONLRET */
# ifdef	OFILL
    { "ofill",	OFILL,	M_OUT },
# endif /* OFILL */
# ifdef	OFDEL
    { "ofdel",	OFDEL,	M_OUT },
# endif /* OFDEL */
# ifdef	NLDLY
    { "nldly",	NLDLY,	M_OUT },
# endif /* NLDLY */
# ifdef	CRDLY
    { "crdly",	CRDLY,	M_OUT },
# endif /* CRDLY */
# ifdef	TABDLY
    { "tabdly",	TABDLY,	M_OUT },
# endif /* TABDLY */
# ifdef	XTABS
    { "xtabs",	XTABS,	M_OUT },
# endif /* XTABS */
# ifdef	BSDLY
    { "bsdly",	BSDLY,	M_OUT },
# endif /* BSDLY */
# ifdef	VTDLY
    { "vtdly",	VTDLY,	M_OUT },
# endif /* VTDLY */
# ifdef	FFDLY
    { "ffdly",	FFDLY,	M_OUT },
# endif /* FFDLY */
# ifdef	PAGEOUT
    { "pageout",PAGEOUT,M_OUT },
# endif /* PAGEOUT */
# ifdef	WRAP
    { "wrap",	WRAP,	M_OUT },
# endif /* WRAP */

# ifdef	CIGNORE
    { "cignore",CIGNORE,M_CTL },
# endif /* CBAUD */
# ifdef	CBAUD
    { "cbaud",	CBAUD,	M_CTL },
# endif /* CBAUD */
# ifdef	CSTOPB
    { "cstopb",	CSTOPB,	M_CTL },
# endif /* CSTOPB */
# ifdef	CREAD
    { "cread",	CREAD,	M_CTL },
# endif /* CREAD */
# ifdef	PARENB
    { "parenb",	PARENB,	M_CTL },
# endif /* PARENB */
# ifdef	PARODD
    { "parodd",	PARODD,	M_CTL },
# endif /* PARODD */
# ifdef	HUPCL
    { "hupcl",	HUPCL,	M_CTL },
# endif /* HUPCL */
# ifdef	CLOCAL
    { "clocal",	CLOCAL,	M_CTL },
# endif /* CLOCAL */
# ifdef	LOBLK
    { "loblk",	LOBLK,	M_CTL },
# endif /* LOBLK */
# ifdef	CIBAUD
    { "cibaud",	CIBAUD,	M_CTL },
# endif /* CIBAUD */
# ifdef CRTSCTS
#  ifdef CCTS_OFLOW
    { "ccts_oflow",CCTS_OFLOW,M_CTL },
#  else
    { "crtscts",CRTSCTS,M_CTL },
#  endif /* CCTS_OFLOW */
# endif /* CRTSCTS */
# ifdef CRTS_IFLOW
    { "crts_iflow",CRTS_IFLOW,M_CTL },
# endif /* CRTS_IFLOW */
# ifdef MDMBUF
    { "mdmbuf",	MDMBUF,	M_CTL },
# endif /* MDMBUF */
# ifdef RCV1EN
    { "rcv1en",	RCV1EN,	M_CTL },
# endif /* RCV1EN */
# ifdef XMT1EN
    { "xmt1en",	XMT1EN,	M_CTL },
# endif /* XMT1EN */

# ifdef	ISIG
    { "isig",	ISIG,	M_LIN },
# endif /* ISIG */
# ifdef	ICANON
    { "icanon",	ICANON,	M_LIN },
# endif /* ICANON */
# ifdef	XCASE
    { "xcase",	XCASE,	M_LIN },
# endif /* XCASE */
# ifdef	ECHO
    { "echo",	ECHO,	M_LIN },
# endif /* ECHO */
# ifdef	ECHOE
    { "echoe",	ECHOE,	M_LIN },
# endif /* ECHOE */
# ifdef	ECHOK
    { "echok",	ECHOK,	M_LIN },
# endif /* ECHOK */
# ifdef	ECHONL
    { "echonl",	ECHONL,	M_LIN },
# endif /* ECHONL */
# ifdef	NOFLSH
    { "noflsh",	NOFLSH,	M_LIN },
# endif /* NOFLSH */
# ifdef	TOSTOP
    { "tostop",	TOSTOP,	M_LIN },
# endif /* TOSTOP */
# ifdef	ECHOCTL
    { "echoctl",ECHOCTL,M_LIN },
# endif /* ECHOCTL */
# ifdef	ECHOPRT
    { "echoprt",ECHOPRT,M_LIN },
# endif /* ECHOPRT */
# ifdef	ECHOKE
    { "echoke",	ECHOKE,	M_LIN },
# endif /* ECHOKE */
# ifdef	DEFECHO
    { "defecho",DEFECHO,M_LIN },
# endif /* DEFECHO */
# ifdef	FLUSHO
    { "flusho",	FLUSHO,	M_LIN },
# endif /* FLUSHO */
# ifdef	PENDIN
    { "pendin",	PENDIN,	M_LIN },
# endif /* PENDIN */
# ifdef	IEXTEN
    { "iexten",	IEXTEN,	M_LIN },
# endif /* IEXTEN */
# ifdef	NOKERNINFO
    { "nokerninfo",NOKERNINFO,M_LIN },
# endif /* NOKERNINFO */
# ifdef	ALTWERASE
    { "altwerase",ALTWERASE,M_LIN },
# endif /* ALTWERASE */
# ifdef	EXTPROC
    { "extproc",EXTPROC, M_LIN },
# endif /* EXTPROC */

# if defined(VINTR)
    { "intr",		C_SH(C_INTR), 	M_CHAR },
# endif /* VINTR */
# if defined(VQUIT)
    { "quit",		C_SH(C_QUIT), 	M_CHAR },
# endif /* VQUIT */
# if defined(VERASE)
    { "erase",		C_SH(C_ERASE), 	M_CHAR },
# endif /* VERASE */
# if defined(VKILL)
    { "kill",		C_SH(C_KILL), 	M_CHAR },
# endif /* VKILL */
# if defined(VEOF)
    { "eof",		C_SH(C_EOF), 	M_CHAR },
# endif /* VEOF */
# if defined(VEOL)
    { "eol",		C_SH(C_EOL), 	M_CHAR },
# endif /* VEOL */
# if defined(VEOL2)
    { "eol2",		C_SH(C_EOL2), 	M_CHAR },
# endif  /* VEOL2 */
# if defined(VSWTCH)
    { "swtch",		C_SH(C_SWTCH), 	M_CHAR },
# endif /* VSWTCH */
# if defined(VDSWTCH)
    { "dswtch",		C_SH(C_DSWTCH),	M_CHAR },
# endif /* VDSWTCH */
# if defined(VERASE2)
    { "erase2",		C_SH(C_ERASE2),	M_CHAR },
# endif /* VERASE2 */
# if defined(VSTART)
    { "start",		C_SH(C_START), 	M_CHAR },
# endif /* VSTART */
# if defined(VSTOP)
    { "stop",		C_SH(C_STOP), 	M_CHAR },
# endif /* VSTOP */
# if defined(VWERASE)
    { "werase",		C_SH(C_WERASE),	M_CHAR },
# endif /* VWERASE */
# if defined(VSUSP)
    { "susp",		C_SH(C_SUSP), 	M_CHAR },
# endif /* VSUSP */
# if defined(VDSUSP)
    { "dsusp",		C_SH(C_DSUSP), 	M_CHAR },
# endif /* VDSUSP */
# if defined(VREPRINT)
    { "reprint",	C_SH(C_REPRINT),M_CHAR },
# endif /* VREPRINT */
# if defined(VDISCARD)
    { "discard",	C_SH(C_DISCARD),M_CHAR },
# endif /* VDISCARD */
# if defined(VLNEXT)
    { "lnext",		C_SH(C_LNEXT), 	M_CHAR },
# endif /* VLNEXT */
# if defined(VSTATUS)
    { "status",		C_SH(C_STATUS),	M_CHAR },
# endif /* VSTATUS */
# if defined(VPAGE)
    { "page",		C_SH(C_PAGE), 	M_CHAR },
# endif /* VPAGE */
# if defined(VPGOFF)
    { "pgoff",		C_SH(C_PGOFF), 	M_CHAR },
# endif /* VPGOFF */
# if defined(VKILL2)
    { "kill2",		C_SH(C_KILL2), 	M_CHAR },
# endif /* VKILL2 */
# if defined(VBRK)
    { "brk",		C_SH(C_BRK), 	M_CHAR },
# endif /* VBRK */
# if defined(VMIN)
    { "min",		C_SH(C_MIN), 	M_CHAR },
# endif /* VMIN */
# if defined(VTIME)
    { "time",		C_SH(C_TIME), 	M_CHAR },
# endif /* VTIME */
    { NULL, 0, -1 },
};



#define tty_getty(el, td) tcgetattr((el)->el_infd, (td))
#define tty_setty(el, td) tcsetattr((el)->el_infd, TCSADRAIN, (td))

#define tty__gettabs(td)     ((((td)->c_oflag & TAB3) == TAB3) ? 0 : 1)
#define tty__geteightbit(td) (((td)->c_cflag & CSIZE) == CS8)
#define tty__cooked_mode(td) ((td)->c_lflag & ICANON)

private void    tty__getchar	__P((struct termios *, unsigned char *));
private void    tty__setchar	__P((struct termios *, unsigned char *));
private speed_t tty__getspeed	__P((struct termios *));
private int     tty_setup	__P((EditLine *));

#define t_qu t_ts


/* tty_setup():
 *	Get the tty parameters and initialize the editing state
 */
private int
tty_setup(el)
    EditLine *el;
{
    int rst = 1;
    if (tty_getty(el, &el->el_tty.t_ed) == -1) {
#ifdef DEBUG_TTY
	(void) fprintf(el->el_errfile,
		       "tty_setup: tty_getty: %s\n", strerror(errno));
#endif /* DEBUG_TTY */
	return(-1);
    }
    el->el_tty.t_ts    = el->el_tty.t_ex = el->el_tty.t_ed;

    el->el_tty.t_speed = tty__getspeed(&el->el_tty.t_ex);
    el->el_tty.t_tabs  = tty__gettabs(&el->el_tty.t_ex);
    el->el_tty.t_eight = tty__geteightbit(&el->el_tty.t_ex);

    el->el_tty.t_ex.c_iflag &= ~el->el_tty.t_t[EX_IO][M_INP].t_clrmask;
    el->el_tty.t_ex.c_iflag |=  el->el_tty.t_t[EX_IO][M_INP].t_setmask;

    el->el_tty.t_ex.c_oflag &= ~el->el_tty.t_t[EX_IO][M_OUT].t_clrmask;
    el->el_tty.t_ex.c_oflag |=  el->el_tty.t_t[EX_IO][M_OUT].t_setmask;

    el->el_tty.t_ex.c_cflag &= ~el->el_tty.t_t[EX_IO][M_CTL].t_clrmask;
    el->el_tty.t_ex.c_cflag |=  el->el_tty.t_t[EX_IO][M_CTL].t_setmask;

    el->el_tty.t_ex.c_lflag &= ~el->el_tty.t_t[EX_IO][M_LIN].t_clrmask;
    el->el_tty.t_ex.c_lflag |=  el->el_tty.t_t[EX_IO][M_LIN].t_setmask;

    /*
     * Reset the tty chars to reasonable defaults
     * If they are disabled, then enable them.
     */
    if (rst) {
        if (tty__cooked_mode(&el->el_tty.t_ts)) {
            tty__getchar(&el->el_tty.t_ts, el->el_tty.t_c[TS_IO]);
            /*
             * Don't affect CMIN and CTIME for the editor mode
             */
            for (rst = 0; rst < C_NCC - 2; rst++)
                if (el->el_tty.t_c[TS_IO][rst] != el->el_tty.t_vdisable &&
                    el->el_tty.t_c[ED_IO][rst] != el->el_tty.t_vdisable)
                    el->el_tty.t_c[ED_IO][rst]  = el->el_tty.t_c[TS_IO][rst];
            for (rst = 0; rst < C_NCC; rst++)
                if (el->el_tty.t_c[TS_IO][rst] != el->el_tty.t_vdisable &&
                    el->el_tty.t_c[EX_IO][rst] != el->el_tty.t_vdisable)
                    el->el_tty.t_c[EX_IO][rst]  = el->el_tty.t_c[TS_IO][rst];
        }
        tty__setchar(&el->el_tty.t_ex, el->el_tty.t_c[EX_IO]);
        if (tty_setty(el, &el->el_tty.t_ex) == -1) {
#ifdef DEBUG_TTY
            (void) fprintf(el->el_errfile, "tty_setup: tty_setty: %s\n",
			   strerror(errno));
#endif /* DEBUG_TTY */
            return(-1);
        }
    }
    else
        tty__setchar(&el->el_tty.t_ex, el->el_tty.t_c[EX_IO]);

    el->el_tty.t_ed.c_iflag &= ~el->el_tty.t_t[ED_IO][M_INP].t_clrmask;
    el->el_tty.t_ed.c_iflag |=  el->el_tty.t_t[ED_IO][M_INP].t_setmask;

    el->el_tty.t_ed.c_oflag &= ~el->el_tty.t_t[ED_IO][M_OUT].t_clrmask;
    el->el_tty.t_ed.c_oflag |=  el->el_tty.t_t[ED_IO][M_OUT].t_setmask;

    el->el_tty.t_ed.c_cflag &= ~el->el_tty.t_t[ED_IO][M_CTL].t_clrmask;
    el->el_tty.t_ed.c_cflag |=  el->el_tty.t_t[ED_IO][M_CTL].t_setmask;

    el->el_tty.t_ed.c_lflag &= ~el->el_tty.t_t[ED_IO][M_LIN].t_clrmask;
    el->el_tty.t_ed.c_lflag |=  el->el_tty.t_t[ED_IO][M_LIN].t_setmask;

    tty__setchar(&el->el_tty.t_ed, el->el_tty.t_c[ED_IO]);
    return 0;
}

protected int
tty_init(el)
    EditLine *el;
{
    el->el_tty.t_mode     = EX_IO;
    el->el_tty.t_vdisable = _POSIX_VDISABLE;
    (void) memcpy(el->el_tty.t_t, ttyperm, sizeof(ttyperm_t));
    (void) memcpy(el->el_tty.t_c, ttychar, sizeof(ttychar_t));
    return tty_setup(el);
} /* end tty_init */


/* tty_end():
 *	Restore the tty to its original settings
 */
protected void
/*ARGSUSED*/
tty_end(el)
    EditLine *el;
{
    /* XXX: Maybe reset to an initial state? */
}


/* tty__getspeed():
 *	Get the tty speed
 */
private speed_t
tty__getspeed(td)
    struct termios *td;
{
    speed_t spd;

    if ((spd = cfgetispeed(td)) == 0)
	spd = cfgetospeed(td);
    return spd;
} /* end tty__getspeed */


/* tty__getchar():
 *	Get the tty characters
 */
private void
tty__getchar(td, s)
    struct termios *td;
    unsigned char *s;
{
# ifdef VINTR
    s[C_INTR]	= td->c_cc[VINTR];
# endif /* VINTR */
# ifdef VQUIT
    s[C_QUIT]	= td->c_cc[VQUIT];
# endif /* VQUIT */
# ifdef VERASE
    s[C_ERASE]	= td->c_cc[VERASE];
# endif /* VERASE */
# ifdef VKILL
    s[C_KILL]	= td->c_cc[VKILL];
# endif /* VKILL */
# ifdef VEOF
    s[C_EOF]	= td->c_cc[VEOF];
# endif /* VEOF */
# ifdef VEOL
    s[C_EOL]	= td->c_cc[VEOL];
# endif /* VEOL */
# ifdef VEOL2
    s[C_EOL2]	= td->c_cc[VEOL2];
# endif  /* VEOL2 */
# ifdef VSWTCH
    s[C_SWTCH]	= td->c_cc[VSWTCH];
# endif /* VSWTCH */
# ifdef VDSWTCH
    s[C_DSWTCH]	= td->c_cc[VDSWTCH];
# endif /* VDSWTCH */
# ifdef VERASE2
    s[C_ERASE2]	= td->c_cc[VERASE2];
# endif /* VERASE2 */
# ifdef VSTART
    s[C_START]	= td->c_cc[VSTART];
# endif /* VSTART */
# ifdef VSTOP
    s[C_STOP]	= td->c_cc[VSTOP];
# endif /* VSTOP */
# ifdef VWERASE
    s[C_WERASE]	= td->c_cc[VWERASE];
# endif /* VWERASE */
# ifdef VSUSP
    s[C_SUSP]	= td->c_cc[VSUSP];
# endif /* VSUSP */
# ifdef VDSUSP
    s[C_DSUSP]	= td->c_cc[VDSUSP];
# endif /* VDSUSP */
# ifdef VREPRINT
    s[C_REPRINT]= td->c_cc[VREPRINT];
# endif /* VREPRINT */
# ifdef VDISCARD
    s[C_DISCARD]= td->c_cc[VDISCARD];
# endif /* VDISCARD */
# ifdef VLNEXT
    s[C_LNEXT]	= td->c_cc[VLNEXT];
# endif /* VLNEXT */
# ifdef VSTATUS
    s[C_STATUS]	= td->c_cc[VSTATUS];
# endif /* VSTATUS */
# ifdef VPAGE
    s[C_PAGE]	= td->c_cc[VPAGE];
# endif /* VPAGE */
# ifdef VPGOFF
    s[C_PGOFF]	= td->c_cc[VPGOFF];
# endif /* VPGOFF */
# ifdef VKILL2
    s[C_KILL2]	= td->c_cc[VKILL2];
# endif /* KILL2 */
# ifdef VMIN
    s[C_MIN]	= td->c_cc[VMIN];
# endif /* VMIN */
# ifdef VTIME
    s[C_TIME]	= td->c_cc[VTIME];
# endif /* VTIME */
} /* tty__getchar */


/* tty__setchar():
 *	Set the tty characters
 */
private void
tty__setchar(td, s)
    struct termios *td;
    unsigned char *s;
{
# ifdef VINTR
    td->c_cc[VINTR]	= s[C_INTR];
# endif /* VINTR */
# ifdef VQUIT
    td->c_cc[VQUIT]	= s[C_QUIT];
# endif /* VQUIT */
# ifdef VERASE
    td->c_cc[VERASE]	= s[C_ERASE];
# endif /* VERASE */
# ifdef VKILL
    td->c_cc[VKILL]	= s[C_KILL];
# endif /* VKILL */
# ifdef VEOF
    td->c_cc[VEOF]	= s[C_EOF];
# endif /* VEOF */
# ifdef VEOL
    td->c_cc[VEOL]	= s[C_EOL];
# endif /* VEOL */
# ifdef VEOL2
    td->c_cc[VEOL2]	= s[C_EOL2];
# endif  /* VEOL2 */
# ifdef VSWTCH
    td->c_cc[VSWTCH]	= s[C_SWTCH];
# endif /* VSWTCH */
# ifdef VDSWTCH
    td->c_cc[VDSWTCH]	= s[C_DSWTCH];
# endif /* VDSWTCH */
# ifdef VERASE2
    td->c_cc[VERASE2]	= s[C_ERASE2];
# endif /* VERASE2 */
# ifdef VSTART
    td->c_cc[VSTART]	= s[C_START];
# endif /* VSTART */
# ifdef VSTOP
    td->c_cc[VSTOP]	= s[C_STOP];
# endif /* VSTOP */
# ifdef VWERASE
    td->c_cc[VWERASE]	= s[C_WERASE];
# endif /* VWERASE */
# ifdef VSUSP
    td->c_cc[VSUSP]	= s[C_SUSP];
# endif /* VSUSP */
# ifdef VDSUSP
    td->c_cc[VDSUSP]	= s[C_DSUSP];
# endif /* VDSUSP */
# ifdef VREPRINT
    td->c_cc[VREPRINT]	= s[C_REPRINT];
# endif /* VREPRINT */
# ifdef VDISCARD
    td->c_cc[VDISCARD]	= s[C_DISCARD];
# endif /* VDISCARD */
# ifdef VLNEXT
    td->c_cc[VLNEXT]	= s[C_LNEXT];
# endif /* VLNEXT */
# ifdef VSTATUS
    td->c_cc[VSTATUS]	= s[C_STATUS];
# endif /* VSTATUS */
# ifdef VPAGE
    td->c_cc[VPAGE]	= s[C_PAGE];
# endif /* VPAGE */
# ifdef VPGOFF
    td->c_cc[VPGOFF]	= s[C_PGOFF];
# endif /* VPGOFF */
# ifdef VKILL2
    td->c_cc[VKILL2]	= s[C_KILL2];
# endif /* VKILL2 */
# ifdef VMIN
    td->c_cc[VMIN]	= s[C_MIN];
# endif /* VMIN */
# ifdef VTIME
    td->c_cc[VTIME]	= s[C_TIME];
# endif /* VTIME */
} /* tty__setchar */


/* tty_bind_char():
 *	Rebind the editline functions
 */
protected void
tty_bind_char(el, force)
    EditLine *el;
    int force;
{
    unsigned char *t_n = el->el_tty.t_c[ED_IO];
    unsigned char *t_o = el->el_tty.t_ed.c_cc;
    char new[2], old[2];
    ttymap_t *tp;
    el_action_t  *dmap, *dalt, *map, *alt;
    new[1] = old[1] = '\0';


    map = el->el_map.key;
    alt = el->el_map.alt;
    if (el->el_map.type == MAP_VI) {
	dmap = el->el_map.vii;
	dalt = el->el_map.vic;
    }
    else {
	dmap = el->el_map.emacs;
	dalt = NULL;
    }

    for (tp = tty_map; tp->nch != -1; tp++) {
	new[0] = t_n[tp->nch];
	old[0] = t_o[tp->och];
	if (new[0] == old[0] && !force)
	    continue;
	/* Put the old default binding back, and set the new binding */
	key_clear(el, map, old);
	map[old[0]] = dmap[old[0]];
	key_clear(el, map, new);
	/* MAP_VI == 1, MAP_EMACS == 0... */
	map[new[0]] = tp->bind[el->el_map.type];
	if (dalt) {
	    key_clear(el, alt, old);
	    alt[old[0]] = dalt[old[0]];
	    key_clear(el, alt, new);
	    alt[new[0]] = tp->bind[el->el_map.type+1];
	}
    }
}

/* tty_rawmode():
 * 	Set terminal into 1 character at a time mode.
 */
protected int
tty_rawmode(el)
    EditLine *el;
{
    if (el->el_tty.t_mode == ED_IO || el->el_tty.t_mode == QU_IO)
	return (0);

    if (tty_getty(el, &el->el_tty.t_ts) == -1) {
#ifdef DEBUG_TTY
	(void) fprintf(el->el_errfile, "tty_rawmode: tty_getty: %s\n", strerror(errno));
#endif /* DEBUG_TTY */
	return(-1);
    }

    /*
     * We always keep up with the eight bit setting and the speed of the
     * tty. But only we only believe changes that are made to cooked mode!
     */
    el->el_tty.t_eight = tty__geteightbit(&el->el_tty.t_ts);
    el->el_tty.t_speed = tty__getspeed(&el->el_tty.t_ts);

    if (tty__getspeed(&el->el_tty.t_ex) != el->el_tty.t_speed ||
	tty__getspeed(&el->el_tty.t_ed) != el->el_tty.t_speed) {
	(void) cfsetispeed(&el->el_tty.t_ex, el->el_tty.t_speed);
	(void) cfsetospeed(&el->el_tty.t_ex, el->el_tty.t_speed);
	(void) cfsetispeed(&el->el_tty.t_ed, el->el_tty.t_speed);
	(void) cfsetospeed(&el->el_tty.t_ed, el->el_tty.t_speed);
    }

    if (tty__cooked_mode(&el->el_tty.t_ts)) {
	if (el->el_tty.t_ts.c_cflag != el->el_tty.t_ex.c_cflag) {
	    el->el_tty.t_ex.c_cflag  = el->el_tty.t_ts.c_cflag;
	    el->el_tty.t_ex.c_cflag &= ~el->el_tty.t_t[EX_IO][M_CTL].t_clrmask;
	    el->el_tty.t_ex.c_cflag |=  el->el_tty.t_t[EX_IO][M_CTL].t_setmask;

	    el->el_tty.t_ed.c_cflag  = el->el_tty.t_ts.c_cflag;
	    el->el_tty.t_ed.c_cflag &= ~el->el_tty.t_t[ED_IO][M_CTL].t_clrmask;
	    el->el_tty.t_ed.c_cflag |=  el->el_tty.t_t[ED_IO][M_CTL].t_setmask;
	}

	if ((el->el_tty.t_ts.c_lflag != el->el_tty.t_ex.c_lflag) &&
	    (el->el_tty.t_ts.c_lflag != el->el_tty.t_ed.c_lflag)) {
	    el->el_tty.t_ex.c_lflag = el->el_tty.t_ts.c_lflag;
	    el->el_tty.t_ex.c_lflag &= ~el->el_tty.t_t[EX_IO][M_LIN].t_clrmask;
	    el->el_tty.t_ex.c_lflag |=  el->el_tty.t_t[EX_IO][M_LIN].t_setmask;

	    el->el_tty.t_ed.c_lflag = el->el_tty.t_ts.c_lflag;
	    el->el_tty.t_ed.c_lflag &= ~el->el_tty.t_t[ED_IO][M_LIN].t_clrmask;
	    el->el_tty.t_ed.c_lflag |=  el->el_tty.t_t[ED_IO][M_LIN].t_setmask;
	}

	if ((el->el_tty.t_ts.c_iflag != el->el_tty.t_ex.c_iflag) &&
	    (el->el_tty.t_ts.c_iflag != el->el_tty.t_ed.c_iflag)) {
	    el->el_tty.t_ex.c_iflag = el->el_tty.t_ts.c_iflag;
	    el->el_tty.t_ex.c_iflag &= ~el->el_tty.t_t[EX_IO][M_INP].t_clrmask;
	    el->el_tty.t_ex.c_iflag |=  el->el_tty.t_t[EX_IO][M_INP].t_setmask;

	    el->el_tty.t_ed.c_iflag = el->el_tty.t_ts.c_iflag;
	    el->el_tty.t_ed.c_iflag &= ~el->el_tty.t_t[ED_IO][M_INP].t_clrmask;
	    el->el_tty.t_ed.c_iflag |=  el->el_tty.t_t[ED_IO][M_INP].t_setmask;
	}

	if ((el->el_tty.t_ts.c_oflag != el->el_tty.t_ex.c_oflag) &&
	    (el->el_tty.t_ts.c_oflag != el->el_tty.t_ed.c_oflag)) {
	    el->el_tty.t_ex.c_oflag = el->el_tty.t_ts.c_oflag;
	    el->el_tty.t_ex.c_oflag &= ~el->el_tty.t_t[EX_IO][M_OUT].t_clrmask;
	    el->el_tty.t_ex.c_oflag |=  el->el_tty.t_t[EX_IO][M_OUT].t_setmask;

	    el->el_tty.t_ed.c_oflag = el->el_tty.t_ts.c_oflag;
	    el->el_tty.t_ed.c_oflag &= ~el->el_tty.t_t[ED_IO][M_OUT].t_clrmask;
	    el->el_tty.t_ed.c_oflag |=  el->el_tty.t_t[ED_IO][M_OUT].t_setmask;
	}

	if (tty__gettabs(&el->el_tty.t_ex) == 0)
	    el->el_tty.t_tabs = 0;
	else
	    el->el_tty.t_tabs = EL_CAN_TAB ? 1 : 0;

	{
	    int i;

	    tty__getchar(&el->el_tty.t_ts, el->el_tty.t_c[TS_IO]);
	    /*
	     * Check if the user made any changes.
	     * If he did, then propagate the changes to the
	     * edit and execute data structures.
	     */
	    for (i = 0; i < C_NCC; i++)
		if (el->el_tty.t_c[TS_IO][i] != el->el_tty.t_c[EX_IO][i])
		    break;

	    if (i != C_NCC) {
		/*
		 * Propagate changes only to the unprotected chars
		 * that have been modified just now.
		 */
		for (i = 0; i < C_NCC; i++) {
		    if (!((el->el_tty.t_t[ED_IO][M_CHAR].t_setmask & C_SH(i)))
		      && (el->el_tty.t_c[TS_IO][i] != el->el_tty.t_c[EX_IO][i]))
			el->el_tty.t_c[ED_IO][i] = el->el_tty.t_c[TS_IO][i];
		    if (el->el_tty.t_t[ED_IO][M_CHAR].t_clrmask & C_SH(i))
			el->el_tty.t_c[ED_IO][i] = el->el_tty.t_vdisable;
		}
		tty_bind_char(el, 0);
		tty__setchar(&el->el_tty.t_ed, el->el_tty.t_c[ED_IO]);

		for (i = 0; i < C_NCC; i++) {
		    if (!((el->el_tty.t_t[EX_IO][M_CHAR].t_setmask & C_SH(i)))
		      && (el->el_tty.t_c[TS_IO][i] != el->el_tty.t_c[EX_IO][i]))
			el->el_tty.t_c[EX_IO][i] = el->el_tty.t_c[TS_IO][i];
		    if (el->el_tty.t_t[EX_IO][M_CHAR].t_clrmask & C_SH(i))
			el->el_tty.t_c[EX_IO][i] = el->el_tty.t_vdisable;
		}
		tty__setchar(&el->el_tty.t_ex, el->el_tty.t_c[EX_IO]);
	    }

	}
    }

    if (tty_setty(el, &el->el_tty.t_ed) == -1) {
#ifdef DEBUG_TTY
	(void) fprintf(el->el_errfile, "tty_rawmode: tty_setty: %s\n",
		       strerror(errno));
#endif /* DEBUG_TTY */
	return -1;
    }
    el->el_tty.t_mode = ED_IO;
    return (0);
} /* end tty_rawmode */


/* tty_cookedmode():
 *	Set the tty back to normal mode
 */
protected int
tty_cookedmode(el)
    EditLine *el;
{				/* set tty in normal setup */
    if (el->el_tty.t_mode == EX_IO)
	return (0);

    if (tty_setty(el, &el->el_tty.t_ex) == -1) {
#ifdef DEBUG_TTY
	(void) fprintf(el->el_errfile, "tty_cookedmode: tty_setty: %s\n",
		       strerror(errno));
#endif /* DEBUG_TTY */
	return -1;
    }
    el->el_tty.t_mode = EX_IO;
    return (0);
} /* end tty_cookedmode */


/* tty_quotemode():
 *	Turn on quote mode
 */
protected int
tty_quotemode(el)
    EditLine *el;
{
    if (el->el_tty.t_mode == QU_IO)
	return 0;

    el->el_tty.t_qu = el->el_tty.t_ed;

    el->el_tty.t_qu.c_iflag &= ~el->el_tty.t_t[QU_IO][M_INP].t_clrmask;
    el->el_tty.t_qu.c_iflag |=  el->el_tty.t_t[QU_IO][M_INP].t_setmask;

    el->el_tty.t_qu.c_oflag &= ~el->el_tty.t_t[QU_IO][M_OUT].t_clrmask;
    el->el_tty.t_qu.c_oflag |=  el->el_tty.t_t[QU_IO][M_OUT].t_setmask;

    el->el_tty.t_qu.c_cflag &= ~el->el_tty.t_t[QU_IO][M_CTL].t_clrmask;
    el->el_tty.t_qu.c_cflag |=  el->el_tty.t_t[QU_IO][M_CTL].t_setmask;

    el->el_tty.t_qu.c_lflag &= ~el->el_tty.t_t[QU_IO][M_LIN].t_clrmask;
    el->el_tty.t_qu.c_lflag |=  el->el_tty.t_t[QU_IO][M_LIN].t_setmask;

    if (tty_setty(el, &el->el_tty.t_qu) == -1) {
#ifdef DEBUG_TTY
	(void) fprintf(el->el_errfile, "QuoteModeOn: tty_setty: %s\n",
		       strerror(errno));
#endif /* DEBUG_TTY */
	return -1;
    }
    el->el_tty.t_mode = QU_IO;
    return 0;
} /* end tty_quotemode */


/* tty_noquotemode():
 *	Turn off quote mode
 */
protected int
tty_noquotemode(el)
    EditLine *el;
{
    if (el->el_tty.t_mode != QU_IO)
	return 0;
    if (tty_setty(el, &el->el_tty.t_ed) == -1) {
#ifdef DEBUG_TTY
	(void) fprintf(el->el_errfile, "QuoteModeOff: tty_setty: %s\n",
		       strerror(errno));
#endif /* DEBUG_TTY */
	return -1;
    }
    el->el_tty.t_mode = ED_IO;
    return 0;
}

/* tty_stty():
 *	Stty builtin
 */
protected int
/*ARGSUSED*/
tty_stty(el, argc, argv)
    EditLine *el;
    int argc;
    char **argv;
{
    ttymodes_t *m;
    char x, *d;
    int aflag = 0;
    char *s;
    char *name;
    int z = EX_IO;

    if (argv == NULL)
	return -1;
    name = *argv++;

    while (argv && *argv && argv[0][0] == '-' && argv[0][2] == '\0')
	switch (argv[0][1]) {
	case 'a':
	    aflag++;
	    argv++;
	    break;
	case 'd':
	    argv++;
	    z = ED_IO;
	    break;
	case 'x':
	    argv++;
	    z = EX_IO;
	    break;
	case 'q':
	    argv++;
	    z = QU_IO;
	    break;
	default:
	    (void) fprintf(el->el_errfile, "%s: Unknown switch `%c'.\n",
			   name, argv[0][1]);
	    return -1;
	}

    if (!argv || !*argv) {
	int i = -1;
	int len = 0, st = 0, cu;
	for (m = ttymodes; m->m_name; m++) {
	    if (m->m_type != i) {
		(void) fprintf(el->el_outfile, "%s%s", i != -1 ? "\n" : "",
			el->el_tty.t_t[z][m->m_type].t_name);
		i = m->m_type;
		st = len = strlen(el->el_tty.t_t[z][m->m_type].t_name);
	    }

	    x = (el->el_tty.t_t[z][i].t_setmask & m->m_value) ? '+' : '\0';
	    x = (el->el_tty.t_t[z][i].t_clrmask & m->m_value) ? '-' : x;

	    if (x != '\0' || aflag) {

		cu = strlen(m->m_name) + (x != '\0') + 1;

		if (len + cu >= el->el_term.t_size.h) {
		    (void) fprintf(el->el_outfile, "\n%*s", st, "");
		    len = st + cu;
		}
		else
		    len += cu;

		if (x != '\0')
		    (void) fprintf(el->el_outfile, "%c%s ", x, m->m_name);
		else
		    (void) fprintf(el->el_outfile, "%s ", m->m_name);
	    }
	}
	(void) fprintf(el->el_outfile, "\n");
	return 0;
    }

    while (argv && (s = *argv++)) {
	switch (*s) {
	case '+':
	case '-':
	    x = *s++;
	    break;
	default:
	    x = '\0';
	    break;
	}
	d = s;
	for (m = ttymodes; m->m_name; m++)
	    if (strcmp(m->m_name, d) == 0)
		break;

	if (!m->m_name)  {
	    (void) fprintf(el->el_errfile, "%s: Invalid argument `%s'.\n",
			   name, d);
	    return -1;
	}

	switch (x) {
	case '+':
	    el->el_tty.t_t[z][m->m_type].t_setmask |= m->m_value;
	    el->el_tty.t_t[z][m->m_type].t_clrmask &= ~m->m_value;
	    break;
	case '-':
	    el->el_tty.t_t[z][m->m_type].t_setmask &= ~m->m_value;
	    el->el_tty.t_t[z][m->m_type].t_clrmask |= m->m_value;
	    break;
	default:
	    el->el_tty.t_t[z][m->m_type].t_setmask &= ~m->m_value;
	    el->el_tty.t_t[z][m->m_type].t_clrmask &= ~m->m_value;
	    break;
	}
    }
    return 0;
} /* end tty_stty */


#ifdef notyet
/* tty_printchar():
 *	DEbugging routine to print the tty characters
 */
private void
tty_printchar(el, s)
    EditLine *el;
    unsigned char *s;
{
    ttyperm_t *m;
    int i;

    for (i = 0; i < C_NCC; i++) {
	for (m = el->el_tty.t_t; m->m_name; m++)
	    if (m->m_type == M_CHAR && C_SH(i) == m->m_value)
		break;
	if (m->m_name)
	    (void) fprintf(el->el_errfile, "%s ^%c ", m->m_name, s[i] + 'A'-1);
	if (i % 5 == 0)
	    (void) fprintf(el->el_errfile, "\n");
    }
    (void) fprintf(el->el_errfile, "\n");
}
#endif /* notyet */
