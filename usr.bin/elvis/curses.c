/* curses.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains the functions & variables needed for a tiny subset of
 * curses.  The principle advantage of this version of curses is its
 * extreme speed.  Disadvantages are potentially larger code, few supported
 * functions, limited compatibility with full curses, and only stdscr.
 */

#include "config.h"
#include "vi.h"

#if ANY_UNIX
# if UNIXV || COH_386
#  ifdef TERMIOS
#   include	<termios.h>
#  else
#   include	<termio.h>
#  endif
#  ifndef NO_S5WINSIZE
#   ifndef _SEQUENT_
#    include	<sys/stream.h>	/* winsize struct defined in one of these? */
#    include	<sys/ptem.h>
#   endif
#  else
#   undef	TIOCGWINSZ	/* we can't handle it correctly yet */
#  endif
# else
#  include	<sgtty.h>
# endif
#endif

#if TOS
# include	<osbind.h>
#endif

#if OSK
# include	<sgstat.h>
#endif

#if VMS
extern int VMS_read_raw;  /* Set in initscr() */
#endif


static void	 starttcap();

/* variables, publicly available & used in the macros */
char	*termtype;	/* name of terminal entry */
short	ospeed;		/* speed of the tty, eg B2400 */
#if OSK
char	PC_;	/* Pad char */
char	*BC;	/* backspace character string */
#else
char	PC;		/* Pad char */
#endif
WINDOW	*stdscr;	/* pointer into kbuf[] */
WINDOW	kbuf[KBSIZ];	/* a very large output buffer */
int	LINES;		/* :li#: number of rows */
int	COLS;		/* :co#: number of columns */
int	AM;		/* :am:  boolean: auto margins? */
int	PT;		/* :pt:  boolean: physical tabs? */
char	*VB;		/* :vb=: visible bell */
char	*UP;		/* :up=: move cursor up */
char	*SO = "";	/* :so=: standout start */
char	*SE = "";	/* :se=: standout end */
char	*US = "";	/* :us=: underline start */
char	*UE = "";	/* :ue=: underline end */
char	*MD = "";	/* :md=: bold start */
char	*ME = "";	/* :me=: bold end */
char	*AS = "";	/* :as=: alternate (italic) start */
char	*AE = "";	/* :ae=: alternate (italic) end */
#ifndef NO_VISIBLE
char	*MV;		/* :mv=: "visible" selection start */
#endif
char	*CM;		/* :cm=: cursor movement */
char	*CE;		/* :ce=: clear to end of line */
char	*CD;		/* :cd=: clear to end of screen */
char	*AL;		/* :al=: add a line */
char	*DL;		/* :dl=: delete a line */
#if OSK
char	*SR_;		/* :sr=: scroll reverse */
#else
char	*SR;		/* :sr=: scroll reverse */
#endif
char	*KS = "";	/* :ks=: switch keypad to application mode */
char	*KE = "";	/* :ke=: switch keypad to system mode */
char	*KU;		/* :ku=: key sequence sent by up arrow */
char	*KD;		/* :kd=: key sequence sent by down arrow */
char	*KL;		/* :kl=: key sequence sent by left arrow */
char	*KR;		/* :kr=: key sequence sent by right arrow */
char	*HM;		/* :HM=: key sequence sent by the <Home> key */
char	*EN;		/* :EN=: key sequence sent by the <End> key */
char	*PU;		/* :PU=: key sequence sent by the <PgUp> key */
char	*PD;		/* :PD=: key sequence sent by the <PgDn> key */
char	*KI;		/* :kI=: key sequence sent by the <Insert> key */
#ifndef NO_FKEY
char	*FKEY[NFKEYS];	/* :k0=: ... :k9=: sequences sent by function keys */
#endif
char	*IM = "";	/* :im=: insert mode start */
char	*IC = "";	/* :ic=: insert the following character */
char	*EI = "";	/* :ei=: insert mode end */
char	*DC;		/* :dc=: delete a character */
char	*TI = "";	/* :ti=: terminal init */	/* GB */
char	*TE = "";	/* :te=: terminal exit */	/* GB */
#ifndef NO_CURSORSHAPE
#if 1
char	*CQ = (char *)0;/* :cQ=: normal cursor */
char	*CX = (char *)1;/* :cX=: cursor used for EX command/entry */
char	*CV = (char *)2;/* :cV=: cursor used for VI command mode */
char	*CI = (char *)3;/* :cI=: cursor used for VI input mode */
char	*CR = (char *)4;/* :cR=: cursor used for VI replace mode */
#else
char	*CQ = "";	/* :cQ=: normal cursor */
char	*CX = "";	/* :cX=: cursor used for EX command/entry */
char	*CV = "";	/* :cV=: cursor used for VI command mode */
char	*CI = "";	/* :cI=: cursor used for VI input mode */
char	*CR = "";	/* :cR=: cursor used for VI replace mode */
#endif
#endif
char	*aend = "";	/* end an attribute -- either UE or ME */
char	ERASEKEY;	/* backspace key taken from ioctl structure */
#ifndef NO_COLOR
char	normalcolor[24];
char	SOcolor[24];
char	SEcolor[24];
char	UScolor[24];
char	UEcolor[24];
char	MDcolor[24];
char	MEcolor[24];
char	AScolor[24];
char	AEcolor[24];
# ifndef NO_POPUP
char	POPUPcolor[24];
# endif
# ifndef NO_VISIBLE
char	VISIBLEcolor[24];
# endif
#endif

#if ANY_UNIX
# if UNIXV || COH_386
#  ifdef TERMIOS
static struct termios	oldtermio;	/* original tty mode */
static struct termios	newtermio;	/* cbreak/noecho tty mode */
#  else
static struct termio	oldtermio;	/* original tty mode */
static struct termio	newtermio;	/* cbreak/noecho tty mode */
#  endif
# else
static struct sgttyb	oldsgttyb;	/* original tty mode */
static struct sgttyb	newsgttyb;	/* cbreak/nl/noecho tty mode */
static int		oldint;		/* ^C or DEL, the "intr" character */
#  ifdef TIOCSLTC
static int		oldswitch;	/* ^Z, the "suspend" character */
static int		olddswitch;	/* ^Y, the "delayed suspend" char */
static int		oldquote;	/* ^V, the "quote next char" char */
#  endif
# endif
#endif

#if OSK
static struct sgbuf	oldsgttyb;	/* orginal tty mode */
static struct sgbuf	newsgttyb;	/* noecho tty mode */
#endif

static char	*capbuf;	/* capability string buffer */


/* Initialize the Curses package. */
void initscr()
{
	/* make sure TERM variable is set */
	termtype = getenv("TERM");

#if VMS
	/* VMS getenv() handles TERM as a environment setting.  Foreign 
	 * terminal support can be implemented by setting the ELVIS_TERM
	 * logical or symbol to match a tinytcap entry.
	 */
	if (!strcmp(termtype,"unknown"))
		termtype = getenv("ELVIS_TERM");
#endif
#if MSDOS
	/* For MS-DOS, if TERM is unset we can default to "pcbios", or
	 * maybe "rainbow".
	 */
	if (!termtype)
	{
#ifdef RAINBOW
		if (*(unsigned char far*)(0xffff000eL) == 6   /* Rainbow 100a */
		 || *(unsigned char far*)(0xffff000eL) == 148)/* Rainbow 100b */
		{
			termtype = "rainbow";
		}
		else
#endif
			termtype = "pcbios";
	}
	if (!strcmp(termtype, "pcbios"))
#else
	if (!termtype)
#endif
	{
#if ANY_UNIX
		write(2, "Environment variable TERM must be set\n", (unsigned)38);
		exit(2);
#endif
#if OSK
		writeln(2, "Environment variable TERM must be set\n", (unsigned)38);
		exit(2);
#endif
#if AMIGA
		termtype = TERMTYPE;
		starttcap(termtype);
#endif
#if MSDOS
		starttcap("pcbios");
#endif
#if TOS
		termtype = "vt52";
		starttcap(termtype);
#endif
#if VMS
		write(2, "UNKNOWN terminal: define ELVIS_TERM\n", (unsigned)36);
		exit(2);
#endif
	}
	else
	{
#if MSDOS
		*o_pcbios = 0;
#endif
		/* start termcap stuff */
		starttcap(termtype);
	}

	/* create stdscr and curscr */
	stdscr = kbuf;

	/* change the terminal mode to cbreak/noecho */
#if ANY_UNIX
# if UNIXV || COH_386
#  ifdef TERMIOS
	tcgetattr(2, &oldtermio);
#  else
	ioctl(2, TCGETA, &oldtermio);
#  endif
# else
	ioctl(2, TIOCGETP, &oldsgttyb);
# endif
#endif

#if OSK
	_gs_opt(0, &oldsgttyb);
#endif

#if VMS
	VMS_read_raw = 1;   /* cbreak/noecho */
	vms_open_tty();
#endif
	resume_curses(TRUE);
}

/* Shut down the Curses package. */
void endwin()
{
	/* change the terminal mode back the way it was */
	suspend_curses();
#if AMIGA
	amiclosewin();
#endif
}


static int curses_active = FALSE;

extern int oldcurs;

/* Send any required termination strings.  Turn off "raw" mode. */
void suspend_curses()
{
#if ANY_UNIX && !(UNIXV || COH_386)
	struct tchars	tbuf;
# ifdef TIOCSLTC
	struct ltchars	ltbuf;
# endif
#endif
#ifndef NO_CURSORSHAPE
	if (has_CQ)
	{
		do_CQ();
		oldcurs = 0;
	}
#endif
	if (has_TE)					/* GB */
	{
		do_TE();
	}
	if (has_KE)
	{
		do_KE();
	}
#ifndef NO_COLOR
	quitcolor();
#endif
	refresh();

	/* change the terminal mode back the way it was */
#if ANY_UNIX
# if (UNIXV || COH_386)
#  ifdef TERMIOS
	tcsetattr(2, TCSADRAIN, &oldtermio);
#  else
	ioctl(2, TCSETAW, &oldtermio);
#  endif
# else
	ioctl(2, TIOCSETP, &oldsgttyb);

	ioctl(2, TIOCGETC, (struct sgttyb *) &tbuf);
	tbuf.t_intrc = oldint;
	ioctl(2, TIOCSETC, (struct sgttyb *) &tbuf);

#  ifdef TIOCSLTC
	ioctl(2, TIOCGLTC, &ltbuf);
	ltbuf.t_suspc = oldswitch;
	ltbuf.t_dsuspc = olddswitch;
	ltbuf.t_lnextc = oldquote;
	ioctl(2, TIOCSLTC, &ltbuf);
#  endif
# endif
#endif
#if OSK
	_ss_opt(0, &oldsgttyb);
#endif
#if AMIGA
	ttyshutdown();
#endif
#if MSDOS
	raw_set_stdio(FALSE);
#endif

#if VMS
	VMS_read_raw = 0;
#endif
	curses_active = FALSE;
}


/* put the terminal in RAW mode.  If "quietly" is FALSE, then ask the user
 * to hit a key, and wait for keystroke before returning.
 */
void resume_curses(quietly)
	int	quietly;
{
	if (!curses_active)
	{
		/* change the terminal mode to cbreak/noecho */
#if ANY_UNIX
# if UNIXV || COH_386
		ospeed = (oldtermio.c_cflag & CBAUD);
		ERASEKEY = oldtermio.c_cc[VERASE];
		newtermio = oldtermio;
		newtermio.c_iflag &= (IXON|IXOFF|IXANY|ISTRIP|IGNBRK);
		newtermio.c_oflag &= ~OPOST;
		newtermio.c_lflag &= ISIG;
		newtermio.c_cc[VINTR] = ctrl('C'); /* always use ^C for interrupts */
		newtermio.c_cc[VMIN] = 1;
		newtermio.c_cc[VTIME] = 0;
#  ifdef VSWTCH
		newtermio.c_cc[VSWTCH] = 0;
#  endif
#  ifdef VSUSP
		newtermio.c_cc[VSUSP] = 0;
#  endif
#  ifdef TERMIOS
		tcsetattr(2, TCSADRAIN, &newtermio);
#  else
		ioctl(2, TCSETAW, &newtermio);
#  endif
# else /* BSD, V7, Coherent-286, or Minix */
		struct tchars	tbuf;
#  ifdef TIOCSLTC
		struct ltchars	ltbuf;
#  endif

		ospeed = oldsgttyb.sg_ospeed;
		ERASEKEY = oldsgttyb.sg_erase;
		newsgttyb = oldsgttyb;
		newsgttyb.sg_flags |= CBREAK;
		newsgttyb.sg_flags &= ~(CRMOD|ECHO|XTABS);
		ioctl(2, TIOCSETP, &newsgttyb);

		ioctl(2, TIOCGETC, (struct sgttyb *) &tbuf);
		oldint = tbuf.t_intrc;
		tbuf.t_intrc = ctrl('C');	/* always use ^C for interrupts */
		ioctl(2, TIOCSETC, (struct sgttyb *) &tbuf);

#  ifdef TIOCSLTC
		ioctl(2, TIOCGLTC, &ltbuf);
		oldswitch = ltbuf.t_suspc;
		ltbuf.t_suspc = 0;		/* disable ^Z for elvis */
		olddswitch = ltbuf.t_dsuspc;
		ltbuf.t_dsuspc = 0;		/* disable ^Y for elvis */
		oldquote = ltbuf.t_lnextc;
		ltbuf.t_lnextc = 0;		/* disable ^V for elvis */
		ioctl(2, TIOCSLTC, &ltbuf);
#  endif

# endif
#endif
#if OSK
		newsgttyb = oldsgttyb;
		newsgttyb.sg_echo = 0;
		newsgttyb.sg_eofch = 0;
		newsgttyb.sg_kbach = 0;
		newsgttyb.sg_kbich = ctrl('C');
		_ss_opt(0, &newsgttyb);
		ospeed = oldsgttyb.sg_baud;
		ERASEKEY = oldsgttyb.sg_bspch;
#endif
#if AMIGA
		/* turn on window resize and RAW */
		ttysetup();
#endif
#if MSDOS
		raw_set_stdio(TRUE);
#endif

#if VMS
		VMS_read_raw = 1;
		{ int c;
			read(0,&c,0);   /* Flush the tty buffer. */
		}
		ERASEKEY = '\177';  /* Accept <DEL> as <^H> for VMS */
#endif

		curses_active = TRUE;
	}

	/* If we're supposed to quit quietly, then we're done */
	if (quietly)
	{
		if (has_TI)					/* GB */
		{
			do_TI();
		}
		if (has_KS)
		{
			do_KS();
		}

		return;
	}

	signal(SIGINT, SIG_IGN);

	move(LINES - 1, 0);
	do_SO();
#if VMS
	qaddstr("\n[Press <RETURN> to continue]");
#else
	qaddstr("[Press <RETURN> to continue]");
#endif
	do_SE();
	refresh();
	ttyread(kbuf, 20, 0); /* in RAW mode, so <20 is very likely */
	if (has_TI)
	{
		do_TI();
	}
	if (kbuf[0] == ':')
	{
		mode = MODE_COLON;
		addch('\n');
		refresh();
	}
	else
	{
		mode = MODE_VI;
		redraw(MARK_UNSET, FALSE);
	}	
	exwrote = FALSE;

	signal(SIGINT, trapint);
}

/* This function fetches an optional string from termcap */
static void mayhave(T, s)
	char	**T;	/* where to store the returned pointer */
	char	*s;	/* name of the capability */
{
	char	*val;

	val = tgetstr(s, &capbuf);
	if (val)
	{
		*T = val;
	}
}


/* This function fetches a required string from termcap */
static void musthave(T, s)
	char	**T;	/* where to store the returned pointer */
	char	*s;	/* name of the capability */
{
	mayhave(T, s);
	if (!*T)
	{
		write(2, "This termcap entry lacks the :", (unsigned)30);
		write(2, s, (unsigned)2);
		write(2, "=: capability\n", (unsigned)14);
#if OSK
		write(2, "\l", 1);
#endif
		exit(2);
	}
}


/* This function fetches a pair of strings from termcap.  If one of them is
 * missing, then the other one is ignored.
 */
static void pair(T, U, sT, sU)
	char	**T;	/* where to store the first pointer */
	char	**U;	/* where to store the second pointer */
	char	*sT;	/* name of the first capability */
	char	*sU;	/* name of the second capability */
{
	mayhave(T, sT);
	mayhave(U, sU);
	if (!**T || !**U)
	{
		*T = *U = "";
	}
}



/* Read everything from termcap */
static void starttcap(term)
	char	*term;
{
	static char	cbmem[800];

	/* allocate memory for capbuf */
	capbuf = cbmem;

	/* get the termcap entry */
	switch (tgetent(kbuf, term))
	{
	  case -1:
		write(2, "Can't read /etc/termcap\n", (unsigned)24);
#if OSK
		write(2, "\l", 1);
#endif
		exit(2);

	  case 0:
		write(2, "Unrecognized TERM type\n", (unsigned)23);
#if OSK
		write(2, "\l", 1);
#endif
		exit(3);
	}

	/* get strings */
	musthave(&UP, "up");
	mayhave(&VB, "vb");
	musthave(&CM, "cm");
	pair(&SO, &SE, "so", "se");
	mayhave(&TI, "ti");
	mayhave(&TE, "te");
	if (tgetnum("ug") <= 0)
	{
		pair(&US, &UE, "us", "ue");
		pair(&MD, &ME, "md", "me");

		/* get italics, or have it default to underline */
		pair(&AS, &AE, "as", "ae");
		if (!*AS)
		{
			AS = US;
			AE = UE;
		}
	}
#ifndef NO_VISIBLE
	MV = SO; /* by default */
	mayhave(&MV, "mv");
#endif
	mayhave(&AL, "al");
	mayhave(&DL, "dl");
	musthave(&CE, "ce");
	mayhave(&CD, "cd");
#if OSK
	mayhave(&SR_, "sr");
#else	
	mayhave(&SR, "sr");
#endif
	pair(&IM, &EI, "im", "ei");
	mayhave(&IC, "ic");
	mayhave(&DC, "dc");

	/* other termcap stuff */
	AM = (tgetflag("am") && !tgetflag("xn"));
	PT = tgetflag("pt");
#if AMIGA
	amiopenwin(termtype);	/* Must run this before ttysetup(); */
	ttysetup();	/* Must run this before getsize(0); */
#endif
	getsize(0);

	/* Key sequences */
	pair(&KS, &KE, "ks", "ke");	/* keypad enable/disable */
	mayhave(&KU, "ku");		/* up */
	mayhave(&KD, "kd");		/* down */
	mayhave(&KR, "kr");		/* right */
	mayhave(&KL, "kl");		/* left */
	if (KL && KL[0]=='\b' && !KL[1])
	{
		/* never use '\b' as a left arrow! */
		KL = (char *)0;
	}
	mayhave(&PU, "kP");		/* PgUp */
	mayhave(&PD, "kN");		/* PgDn */
	mayhave(&HM, "kh");		/* Home */
	mayhave(&EN, "kH");		/* End */
	mayhave(&KI, "kI");		/* Insert */
#ifndef CRUNCH
	if (!PU) mayhave(&PU, "K2");	/* "3x3 pad" names for PgUp, etc. */
	if (!PD) mayhave(&PD, "K5");
	if (!HM) mayhave(&HM, "K1");
	if (!EN) mayhave(&EN, "K4");

	mayhave(&PU, "PU");		/* old XENIX names for PgUp, etc. */
	mayhave(&PD, "PD");		/* (overrides others, if used.) */
	mayhave(&HM, "HM");
	mayhave(&EN, "EN");
#endif
#ifndef NO_FKEY
	mayhave(&FKEY[0], "k0");		/* function key codes */
	mayhave(&FKEY[1], "k1");
	mayhave(&FKEY[2], "k2");
	mayhave(&FKEY[3], "k3");
	mayhave(&FKEY[4], "k4");
	mayhave(&FKEY[5], "k5");
	mayhave(&FKEY[6], "k6");
	mayhave(&FKEY[7], "k7");
	mayhave(&FKEY[8], "k8");
	mayhave(&FKEY[9], "k9");
# ifndef NO_SHIFT_FKEY
	mayhave(&FKEY[10], "s0");		/* shift function key codes */
	mayhave(&FKEY[11], "s1");
	mayhave(&FKEY[12], "s2");
	mayhave(&FKEY[13], "s3");
	mayhave(&FKEY[14], "s4");
	mayhave(&FKEY[15], "s5");
	mayhave(&FKEY[16], "s6");
	mayhave(&FKEY[17], "s7");
	mayhave(&FKEY[18], "s8");
	mayhave(&FKEY[19], "s9");
#  ifndef NO_CTRL_FKEY
	mayhave(&FKEY[20], "c0");		/* control function key codes */
	mayhave(&FKEY[21], "c1");
	mayhave(&FKEY[22], "c2");
	mayhave(&FKEY[23], "c3");
	mayhave(&FKEY[24], "c4");
	mayhave(&FKEY[25], "c5");
	mayhave(&FKEY[26], "c6");
	mayhave(&FKEY[27], "c7");
	mayhave(&FKEY[28], "c8");
	mayhave(&FKEY[29], "c9");
#   ifndef NO_ALT_FKEY
	mayhave(&FKEY[30], "a0");		/* alt function key codes */
	mayhave(&FKEY[31], "a1");
	mayhave(&FKEY[32], "a2");
	mayhave(&FKEY[33], "a3");
	mayhave(&FKEY[34], "a4");
	mayhave(&FKEY[35], "a5");
	mayhave(&FKEY[36], "a6");
	mayhave(&FKEY[37], "a7");
	mayhave(&FKEY[38], "a8");
	mayhave(&FKEY[39], "a9");
#   endif
#  endif
# endif
#endif

#ifndef NO_CURSORSHAPE
	/* cursor shapes */
	CQ = tgetstr("cQ", &capbuf);
	if (has_CQ)
	{
		CX = tgetstr("cX", &capbuf);
		if (!CX) CX = CQ;
		CV = tgetstr("cV", &capbuf);
		if (!CV) CV = CQ;
		CI = tgetstr("cI", &capbuf);
		if (!CI) CI = CQ;
		CR = tgetstr("cR", &capbuf);
		if (!CR) CR = CQ;
	}
# ifndef CRUNCH
	else
	{
		CQ = CV = "";
		pair(&CQ, &CV, "ve", "vs");
		CX = CI = CR = CQ;
	}
# endif /* !CRUNCH */
#endif /* !NO_CURSORSHAPE */

#ifndef NO_COLOR
	strcpy(SOcolor, SO);
	strcpy(SEcolor, SE);
	strcpy(AScolor, AS);
	strcpy(AEcolor, AE);
	strcpy(MDcolor, MD);
	strcpy(MEcolor, ME);
	strcpy(UScolor, US);
	strcpy(UEcolor, UE);
# ifndef NO_POPUP
	strcpy(POPUPcolor, SO);
# endif
# ifndef NO_VISIBLE
	strcpy(VISIBLEcolor, MV);
# endif
#endif

}


/* This function gets the window size.  It uses the TIOCGWINSZ ioctl call if
 * your system has it, or tgetnum("li") and tgetnum("co") if it doesn't.
 * This function is called once during initialization, and thereafter it is
 * called whenever the SIGWINCH signal is sent to this process.
 */
int getsize(signo)
	int	signo;
{
	int	lines;
	int	cols;
#ifdef TIOCGWINSZ
	struct winsize size;
#endif

#ifdef SIGWINCH
	/* reset the signal vector */
	signal(SIGWINCH, (void *)getsize);
#endif

	/* get the window size, one way or another. */
	lines = cols = 0;
#ifdef TIOCGWINSZ
	if (ioctl(2, TIOCGWINSZ, &size) >= 0)
	{
		lines = size.ws_row;
		cols = size.ws_col;
	}
#endif
#if AMIGA
	/* Amiga gets window size by asking the console.device */
	if (!strcmp(TERMTYPE, termtype))
	{
	    auto long len;
	    auto char buf[30];
	    
	    Write(Output(), "\2330 q", 4); /* Ask the console.device */
	    len = Read(Input(), buf, 29);
	    buf[len] = '\000';
	    sscanf(&buf[5], "%d;%d", &lines, &cols);
	}
#endif
	if ((lines == 0 || cols == 0) && signo == 0)
	{
		LINES = tgetnum("li");
		COLS = tgetnum("co");
	}
#if MSDOS
# ifdef RAINBOW
	if (!strcmp(termtype, "rainbow"))
	{
		/* Determine whether Rainbow is in 80-column or 132-column mode */
		cols = *(unsigned char far *)0xee000f57L;
	}
	else
# endif
	{
		lines = v_rows();
		cols = v_cols();
	}
#endif
	if (lines >= 2 && cols >= 30)
	{
		LINES = lines;
		COLS = cols;
	}

	/* Make sure we got values that we can live with */
	if (LINES < 2 || COLS < 30)
	{
		write(2, "Screen too small\n", (unsigned)17);
#if OSK
		write(2, "\l", 1);
#endif
		endwin();
		exit(2);
	}

#if AMIGA
	if (*o_lines != LINES || *o_columns != COLS)
	{
		*o_lines = LINES;
		*o_columns = COLS;
	}
#endif

	return 0;
}


/* This is a function version of addch() -- it is used by tputs() */
int faddch(ch)
	int	ch;
{
	addch(ch);

	return 0;
}

/* This function quickly adds a string to the output queue.  It does *NOT*
 * convert \n into <CR><LF>.
 */
void qaddstr(str)
	char	*str;
{
	REG char *s_, *d_;

#if MSDOS
	if (o_pcbios[0])
	{
		while (*str)
			qaddch(*str++);
		return;
	}
#endif
	for (s_=(str), d_=stdscr; *d_++ = *s_++; )
	{
	}
	stdscr = d_ - 1;
}

/* Output the ESC sequence needed to go into any video mode, if supported */
void attrset(a)
	int	a;
{
	do_aend();
	if (a == A_BOLD)
	{
		do_MD();
		aend = ME;
	}
	else if (a == A_UNDERLINE)
	{
		do_US();
		aend = UE;
	}
	else if (a == A_ALTCHARSET)
	{
		do_AS();
		aend = AE;
	}
	else
	{
		aend = "";
	}
}


/* Insert a single character into the display */
void insch(ch)
	int	ch;
{
	if (has_IM)
		do_IM();
	do_IC();
	qaddch(ch);
	if (has_EI)
		do_EI();
}

void wrefresh()
{
	if (stdscr != kbuf)
	{
		VOIDBIOS(;,ttywrite(kbuf, (unsigned)(stdscr - kbuf)));
		stdscr = kbuf;
	}
}

void wqrefresh()
{
	if (stdscr - kbuf > 2000)
	{
		VOIDBIOS(stdscr = kbuf,
		{
			ttywrite(kbuf, (unsigned)(stdscr - kbuf)); 
			stdscr = kbuf;
		});
	}
}

#ifndef NO_COLOR
/* This function is called during termination.  It resets color modes */
int ansiquit()
{
	/* if ANSI terminal & colors were set, then reset the colors */
	if (!strcmp(UP, "\033[A") &&  strcmp(SOcolor, SO))
	{
		tputs("\033[37;40m\033[m", 1, faddch);
		clrtoeol();
		return 1;
	}
	return 0;
}

/* This sets the color strings that work for ANSI terminals.  If the TERMCAP
 * doesn't look like an ANSI terminal, then it returns FALSE.  If the colors
 * aren't understood, it also returns FALSE.  If all goes well, it returns TRUE
 */
int ansicolor(cmode, attrbyte)
	int	cmode;		/* mode to set, e.g. A_NORMAL */
	int	attrbyte;	/* IBM PC attribute byte */
{
	char	temp[24];	/* hold the new mode string */

	/* if not ANSI-ish, then fail */
	if (strcmp(UP, "\033[A") && strcmp(UP, "\033OA"))
	{
		/* Only give an error message if we're editing a file.
		 * (I.e., if we're *NOT* currently doing a ".exrc")
		 */
		if (tmpfd >= 0)
			msg("Don't know how to set colors for this terminal");
		return FALSE;
	}

	/* construct the color string */
#ifdef MWC /* either Coherent-286 ("COHERENT"), or Coherent-386 ("M_SYSV") */
	sprintf(temp, "\033[m\033[3%cm\033[4%cm%s%s",
		"04261537"[attrbyte & 0x07],
		"04261537"[(attrbyte >> 4) & 0x07],
		(attrbyte & 0x08) ? "\033[1m" : "",
		(attrbyte & 0x80) ? "\033[5m" : "");
#else
	sprintf(temp, "\033[m\033[3%c;4%c%s%sm",
		"04261537"[attrbyte & 0x07],
		"04261537"[(attrbyte >> 4) & 0x07],
		(attrbyte & 0x08) ? ";1" : "",
		(attrbyte & 0x80) ? ";5" : "");
#endif

	/* stick it in the right place */
	switch (cmode)
	{
	  case A_NORMAL:
		if (!strcmp(MEcolor, normalcolor))
			strcpy(MEcolor, temp);
		if (!strcmp(UEcolor, normalcolor))
			strcpy(UEcolor, temp);
		if (!strcmp(AEcolor, normalcolor))
			strcpy(AEcolor, temp);
		if (!strcmp(SEcolor, normalcolor))
			strcpy(SEcolor, temp);

		strcpy(normalcolor, temp);
		tputs(normalcolor, 1, faddch);
		break;

	  case A_BOLD:
		strcpy(MDcolor, temp);
		strcpy(MEcolor, normalcolor);
		break;

	  case A_UNDERLINE:
		strcpy(UScolor, temp);
		strcpy(UEcolor, normalcolor);
		break;

	  case A_ALTCHARSET:
		strcpy(AScolor, temp);
		strcpy(AEcolor, normalcolor);
		break;

	  case A_STANDOUT:
		strcpy(SOcolor, temp);
		strcpy(SEcolor, normalcolor);
		break;

#ifndef NO_POPUP
	  case A_POPUP:
		strcpy(POPUPcolor, temp);
		break;
#endif

#ifndef NO_VISIBLE
	  case A_VISIBLE:
		strcpy(VISIBLEcolor, temp);
		break;
#endif
	}

	return TRUE;
}


/* This function outputs the ESC sequence needed to switch the screen back
 * to "normal" mode.  On color terminals which haven't had their color set
 * yet, this is one of the termcap strings; for color terminals that really
 * have had colors defined, we just the "normal color" escape sequence.
 */
int
endcolor()
{
	if (aend == ME)
		tputs(MEcolor, 1, faddch);
	else if (aend == UE)
		tputs(UEcolor, 1, faddch);
	else if (aend == AE)
		tputs(AEcolor, 1, faddch);
	else if (aend == SE)
		tputs(SEcolor, 1, faddch);
	aend = "";
	return 0;
}


#endif /* !NO_COLOR */
