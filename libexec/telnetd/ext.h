/*
 * Copyright (c) 1989, 1993
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
 *	@(#)ext.h	8.2 (Berkeley) 12/15/93
 *	$FreeBSD$
 */

/*
 * Telnet server variable declarations
 */
extern char	options[256];
extern char	do_dont_resp[256];
extern char	will_wont_resp[256];
extern int	linemode;	/* linemode on/off */
#ifdef	LINEMODE
extern int	uselinemode;	/* what linemode to use (on/off) */
extern int	editmode;	/* edit modes in use */
extern int	useeditmode;	/* edit modes to use */
extern int	alwayslinemode;	/* command line option */
# ifdef	KLUDGELINEMODE
extern int	lmodetype;	/* Client support for linemode */
# endif	/* KLUDGELINEMODE */
#endif	/* LINEMODE */
extern int	flowmode;	/* current flow control state */
extern int	restartany;	/* restart output on any character state */
#ifdef DIAGNOSTICS
extern int	diagnostic;	/* telnet diagnostic capabilities */
#endif /* DIAGNOSTICS */
#ifdef BFTPDAEMON
extern int	bftpd;		/* behave as bftp daemon */
#endif /* BFTPDAEMON */
#if	defined(SecurID)
extern int	require_SecurID;
#endif
#if	defined(AUTHENTICATION)
extern int	auth_level;
#endif

extern slcfun	slctab[NSLC + 1];	/* slc mapping table */

char	*terminaltype;

/*
 * I/O data buffers, pointers, and counters.
 */
extern char	ptyobuf[BUFSIZ+NETSLOP], *pfrontp, *pbackp;

extern char	netibuf[BUFSIZ], *netip;

extern char	netobuf[BUFSIZ+NETSLOP], *nfrontp, *nbackp;
extern char	*neturg;		/* one past last bye of urgent data */

extern int	pcc, ncc;

#if defined(CRAY2) && defined(UNICOS5)
extern int unpcc;  /* characters left unprocessed by CRAY-2 terminal routine */
extern char *unptyip;  /* pointer to remaining characters in buffer */
#endif

extern int	pty, net;
extern char	*line;
extern int	SYNCHing;		/* we are in TELNET SYNCH mode */

#ifndef	P
# ifdef	__STDC__
#  define P(x)	x
# else
#  define P(x)	()
# endif
#endif

extern void
	_termstat P((void)),
	add_slc P((int, int, int)),
	check_slc P((void)),
	change_slc P((int, int, int)),
	cleanup P((int)),
	clientstat P((int, int, int)),
	copy_termbuf P((char *, int)),
	deferslc P((void)),
	defer_terminit P((void)),
	do_opt_slc P((unsigned char *, int)),
	doeof P((void)),
	dooption P((int)),
	dontoption P((int)),
	edithost P((char *, char *)),
	fatal P((int, char *)),
	fatalperror P((int, char *)),
	get_slc_defaults P((void)),
	init_env P((void)),
	init_termbuf P((void)),
	interrupt P((void)),
	localstat P((void)),
	flowstat P((void)),
	netclear P((void)),
	netflush P((void)),
#ifdef DIAGNOSTICS
	printoption P((char *, int)),
	printdata P((char *, char *, int)),
	printsub P((int, unsigned char *, int)),
#endif
	ptyflush P((void)),
	putchr P((int)),
	putf P((char *, char *)),
	recv_ayt P((void)),
	send_do P((int, int)),
	send_dont P((int, int)),
	send_slc P((void)),
	send_status P((void)),
	send_will P((int, int)),
	send_wont P((int, int)),
	sendbrk P((void)),
	sendsusp P((void)),
	set_termbuf P((void)),
	start_login P((char *, int, char *)),
	start_slc P((int)),
#if	defined(AUTHENTICATION)
	start_slave P((char *)),
#else
	start_slave P((char *, int, char *)),
#endif
	suboption P((void)),
	telrcv P((void)),
	ttloop P((void)),
	tty_binaryin P((int)),
	tty_binaryout P((int));

extern int
	end_slc P((unsigned char **)),
	getnpty P((void)),
#ifndef convex
	getpty P((int *)),
#endif
	login_tty P((int)),
	spcset P((int, cc_t *, cc_t **)),
	stilloob P((int)),
	terminit P((void)),
	termstat P((void)),
	tty_flowmode P((void)),
	tty_restartany P((void)),
	tty_isbinaryin P((void)),
	tty_isbinaryout P((void)),
	tty_iscrnl P((void)),
	tty_isecho P((void)),
	tty_isediting P((void)),
	tty_islitecho P((void)),
	tty_isnewmap P((void)),
	tty_israw P((void)),
	tty_issofttab P((void)),
	tty_istrapsig P((void)),
	tty_linemode P((void));

extern void
	tty_rspeed P((int)),
	tty_setecho P((int)),
	tty_setedit P((int)),
	tty_setlinemode P((int)),
	tty_setlitecho P((int)),
	tty_setsig P((int)),
	tty_setsofttab P((int)),
	tty_tspeed P((int)),
	willoption P((int)),
	wontoption P((int)),
	writenet P((unsigned char *, int));



/*
 * The following are some clocks used to decide how to interpret
 * the relationship between various variables.
 */

extern struct {
    int
	system,			/* what the current time is */
	echotoggle,		/* last time user entered echo character */
	modenegotiated,		/* last time operating mode negotiated */
	didnetreceive,		/* last time we read data from network */
	ttypesubopt,		/* ttype subopt is received */
	tspeedsubopt,		/* tspeed subopt is received */
	environsubopt,		/* environ subopt is received */
	oenvironsubopt,		/* old environ subopt is received */
	xdisplocsubopt,		/* xdisploc subopt is received */
	baseline,		/* time started to do timed action */
	gotDM;			/* when did we last see a data mark */
} clocks;


#if	defined(CRAY2) && defined(UNICOS5)
extern int	needtermstat;
#endif

#ifndef	DEFAULT_IM
# ifdef CRAY
#  define DEFAULT_IM	"\r\n\r\nCray UNICOS (%h) (%t)\r\n\r\r\n\r"
# else
#  ifdef sun
#   define DEFAULT_IM	"\r\n\r\nSunOS UNIX (%h) (%t)\r\n\r\r\n\r"
#  else
#   ifdef ultrix
#    define DEFAULT_IM	"\r\n\r\nULTRIX (%h) (%t)\r\n\r\r\n\r"
#   else
#    define DEFAULT_IM	"\r\n\r\nFreeBSD (%h) (%t)\r\n\r\r\n\r"
#   endif
#  endif
# endif
#endif
