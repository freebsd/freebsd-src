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
 */

#ifndef lint
static char sccsid[] = "@(#)utility.c	8.4 (Berkeley) 5/30/95";
#endif /* not lint */

#ifdef __FreeBSD__
#include <locale.h>
#endif
#define PRINTOPTIONS
#include "telnetd.h"

/*
 * utility functions performing io related tasks
 */

/*
 * ttloop
 *
 *	A small subroutine to flush the network output buffer, get some data
 * from the network, and pass it through the telnet state machine.  We
 * also flush the pty input buffer (by dropping its data) if it becomes
 * too full.
 */

    void
ttloop()
{
    void netflush();

    DIAG(TD_REPORT, {sprintf(nfrontp, "td: ttloop\r\n");
		     nfrontp += strlen(nfrontp);});
    if (nfrontp-nbackp) {
	netflush();
    }
    ncc = read(net, netibuf, sizeof netibuf);
    if (ncc < 0) {
	syslog(LOG_INFO, "ttloop:  read: %m\n");
	exit(1);
    } else if (ncc == 0) {
	syslog(LOG_INFO, "ttloop:  peer died: %m\n");
	exit(1);
    }
    DIAG(TD_REPORT, {sprintf(nfrontp, "td: ttloop read %d chars\r\n", ncc);
		     nfrontp += strlen(nfrontp);});
    netip = netibuf;
    telrcv();			/* state machine */
    if (ncc > 0) {
	pfrontp = pbackp = ptyobuf;
	telrcv();
    }
}  /* end of ttloop */

/*
 * Check a descriptor to see if out of band data exists on it.
 */
    int
stilloob(s)
    int	s;		/* socket number */
{
    static struct timeval timeout = { 0 };
    fd_set	excepts;
    int value;

    do {
	FD_ZERO(&excepts);
	FD_SET(s, &excepts);
	value = select(s+1, (fd_set *)0, (fd_set *)0, &excepts, &timeout);
    } while ((value == -1) && (errno == EINTR));

    if (value < 0) {
	fatalperror(pty, "select");
    }
    if (FD_ISSET(s, &excepts)) {
	return 1;
    } else {
	return 0;
    }
}

	void
ptyflush()
{
	int n;

	if ((n = pfrontp - pbackp) > 0) {
		DIAG((TD_REPORT | TD_PTYDATA),
			{ sprintf(nfrontp, "td: ptyflush %d chars\r\n", n);
			  nfrontp += strlen(nfrontp); });
		DIAG(TD_PTYDATA, printdata("pd", pbackp, n));
		n = write(pty, pbackp, n);
	}
	if (n < 0) {
		if (errno == EWOULDBLOCK || errno == EINTR)
			return;
		cleanup(0);
	}
	pbackp += n;
	if (pbackp == pfrontp)
		pbackp = pfrontp = ptyobuf;
}

/*
 * nextitem()
 *
 *	Return the address of the next "item" in the TELNET data
 * stream.  This will be the address of the next character if
 * the current address is a user data character, or it will
 * be the address of the character following the TELNET command
 * if the current address is a TELNET IAC ("I Am a Command")
 * character.
 */
    char *
nextitem(current)
    char	*current;
{
    if ((*current&0xff) != IAC) {
	return current+1;
    }
    switch (*(current+1)&0xff) {
    case DO:
    case DONT:
    case WILL:
    case WONT:
	return current+3;
    case SB:		/* loop forever looking for the SE */
	{
	    register char *look = current+2;

	    for (;;) {
		if ((*look++&0xff) == IAC) {
		    if ((*look++&0xff) == SE) {
			return look;
		    }
		}
	    }
	}
    default:
	return current+2;
    }
}  /* end of nextitem */


/*
 * netclear()
 *
 *	We are about to do a TELNET SYNCH operation.  Clear
 * the path to the network.
 *
 *	Things are a bit tricky since we may have sent the first
 * byte or so of a previous TELNET command into the network.
 * So, we have to scan the network buffer from the beginning
 * until we are up to where we want to be.
 *
 *	A side effect of what we do, just to keep things
 * simple, is to clear the urgent data pointer.  The principal
 * caller should be setting the urgent data pointer AFTER calling
 * us in any case.
 */
    void
netclear()
{
    register char *thisitem, *next;
    char *good;
#define	wewant(p)	((nfrontp > p) && ((*p&0xff) == IAC) && \
				((*(p+1)&0xff) != EC) && ((*(p+1)&0xff) != EL))

#ifdef	ENCRYPTION
    thisitem = nclearto > netobuf ? nclearto : netobuf;
#else	/* ENCRYPTION */
    thisitem = netobuf;
#endif	/* ENCRYPTION */

    while ((next = nextitem(thisitem)) <= nbackp) {
	thisitem = next;
    }

    /* Now, thisitem is first before/at boundary. */

#ifdef	ENCRYPTION
    good = nclearto > netobuf ? nclearto : netobuf;
#else	/* ENCRYPTION */
    good = netobuf;	/* where the good bytes go */
#endif	/* ENCRYPTION */

    while (nfrontp > thisitem) {
	if (wewant(thisitem)) {
	    int length;

	    next = thisitem;
	    do {
		next = nextitem(next);
	    } while (wewant(next) && (nfrontp > next));
	    length = next-thisitem;
	    memmove(good, thisitem, length);
	    good += length;
	    thisitem = next;
	} else {
	    thisitem = nextitem(thisitem);
	}
    }

    nbackp = netobuf;
    nfrontp = good;		/* next byte to be sent */
    neturg = 0;
}  /* end of netclear */

/*
 *  netflush
 *		Send as much data as possible to the network,
 *	handling requests for urgent data.
 */
    void
netflush()
{
    int n;
    extern int not42;

    if ((n = nfrontp - nbackp) > 0) {
	DIAG(TD_REPORT,
	    { sprintf(nfrontp, "td: netflush %d chars\r\n", n);
	      n += strlen(nfrontp);  /* get count first */
	      nfrontp += strlen(nfrontp);  /* then move pointer */
	    });
#ifdef	ENCRYPTION
	if (encrypt_output) {
		char *s = nclearto ? nclearto : nbackp;
		if (nfrontp - s > 0) {
			(*encrypt_output)((unsigned char *)s, nfrontp-s);
			nclearto = nfrontp;
		}
	}
#endif	/* ENCRYPTION */
	/*
	 * if no urgent data, or if the other side appears to be an
	 * old 4.2 client (and thus unable to survive TCP urgent data),
	 * write the entire buffer in non-OOB mode.
	 */
	if ((neturg == 0) || (not42 == 0)) {
	    n = write(net, nbackp, n);	/* normal write */
	} else {
	    n = neturg - nbackp;
	    /*
	     * In 4.2 (and 4.3) systems, there is some question about
	     * what byte in a sendOOB operation is the "OOB" data.
	     * To make ourselves compatible, we only send ONE byte
	     * out of band, the one WE THINK should be OOB (though
	     * we really have more the TCP philosophy of urgent data
	     * rather than the Unix philosophy of OOB data).
	     */
	    if (n > 1) {
		n = send(net, nbackp, n-1, 0);	/* send URGENT all by itself */
	    } else {
		n = send(net, nbackp, n, MSG_OOB);	/* URGENT data */
	    }
	}
    }
    if (n < 0) {
	if (errno == EWOULDBLOCK || errno == EINTR)
		return;
	cleanup(0);
    }
    nbackp += n;
#ifdef	ENCRYPTION
    if (nbackp > nclearto)
	nclearto = 0;
#endif	/* ENCRYPTION */
    if (nbackp >= neturg) {
	neturg = 0;
    }
    if (nbackp == nfrontp) {
	nbackp = nfrontp = netobuf;
#ifdef	ENCRYPTION
	nclearto = 0;
#endif	/* ENCRYPTION */
    }
    return;
}  /* end of netflush */


/*
 * writenet
 *
 * Just a handy little function to write a bit of raw data to the net.
 * It will force a transmit of the buffer if necessary
 *
 * arguments
 *    ptr - A pointer to a character string to write
 *    len - How many bytes to write
 */
	void
writenet(ptr, len)
	register unsigned char *ptr;
	register int len;
{
	/* flush buffer if no room for new data) */
	if ((&netobuf[BUFSIZ] - nfrontp) < len) {
		/* if this fails, don't worry, buffer is a little big */
		netflush();
	}

	memmove(nfrontp, ptr, len);
	nfrontp += len;

}  /* end of writenet */


/*
 * miscellaneous functions doing a variety of little jobs follow ...
 */


	void
fatal(f, msg)
	int f;
	char *msg;
{
	char buf[BUFSIZ];

	(void) sprintf(buf, "telnetd: %s.\r\n", msg);
#ifdef	ENCRYPTION
	if (encrypt_output) {
		/*
		 * Better turn off encryption first....
		 * Hope it flushes...
		 */
		encrypt_send_end();
		netflush();
	}
#endif	/* ENCRYPTION */
	(void) write(f, buf, (int)strlen(buf));
	sleep(1);	/*XXX*/
	exit(1);
}

	void
fatalperror(f, msg)
	int f;
	char *msg;
{
	char buf[BUFSIZ], *strerror();

	(void) sprintf(buf, "%s: %s", msg, strerror(errno));
	fatal(f, buf);
}

char editedhost[32];

	void
edithost(pat, host)
	register char *pat;
	register char *host;
{
	register char *res = editedhost;
	char *strncpy();

	if (!pat)
		pat = "";
	while (*pat) {
		switch (*pat) {

		case '#':
			if (*host)
				host++;
			break;

		case '@':
			if (*host)
				*res++ = *host++;
			break;

		default:
			*res++ = *pat;
			break;
		}
		if (res == &editedhost[sizeof editedhost - 1]) {
			*res = '\0';
			return;
		}
		pat++;
	}
	if (*host)
		(void) strncpy(res, host,
				sizeof editedhost - (res - editedhost) -1);
	else
		*res = '\0';
	editedhost[sizeof editedhost - 1] = '\0';
}

static char *putlocation;

	void
putstr(s)
	register char *s;
{

	while (*s)
		putchr(*s++);
}

	void
putchr(cc)
	int cc;
{
	*putlocation++ = cc;
}

#ifdef __FreeBSD__
static char fmtstr[] = { "%+" };
#else
/*
 * This is split on two lines so that SCCS will not see the M
 * between two % signs and expand it...
 */
static char fmtstr[] = { "%l:%M\
%P on %A, %d %B %Y" };
#endif

	void
putf(cp, where)
	register char *cp;
	char *where;
{
	char *slash;
	time_t t;
	char db[100];
#ifdef	STREAMSPTY
	extern char *strchr();
#else
	extern char *strrchr();
#endif

	putlocation = where;

	while (*cp) {
		if (*cp != '%') {
			putchr(*cp++);
			continue;
		}
		switch (*++cp) {

		case 't':
#ifdef	STREAMSPTY
			/* names are like /dev/pts/2 -- we want pts/2 */
			slash = strchr(line+1, '/');
#else
			slash = strrchr(line, '/');
#endif
			if (slash == (char *) 0)
				putstr(line);
			else
				putstr(&slash[1]);
			break;

		case 'h':
			putstr(editedhost);
			break;

		case 'd':
#ifdef __FreeBSD__
			setlocale(LC_TIME, "");
#endif
			(void)time(&t);
			(void)strftime(db, sizeof(db), fmtstr, localtime(&t));
			putstr(db);
			break;

		case '%':
			putchr('%');
			break;
		}
		cp++;
	}
}

#ifdef DIAGNOSTICS
/*
 * Print telnet options and commands in plain text, if possible.
 */
	void
printoption(fmt, option)
	register char *fmt;
	register int option;
{
	if (TELOPT_OK(option))
		sprintf(nfrontp, "%s %s\r\n", fmt, TELOPT(option));
	else if (TELCMD_OK(option))
		sprintf(nfrontp, "%s %s\r\n", fmt, TELCMD(option));
	else
		sprintf(nfrontp, "%s %d\r\n", fmt, option);
	nfrontp += strlen(nfrontp);
	return;
}

    void
printsub(direction, pointer, length)
    char		direction;	/* '<' or '>' */
    unsigned char	*pointer;	/* where suboption data sits */
    int			length;		/* length of suboption data */
{
    register int i;
    char buf[512];

        if (!(diagnostic & TD_OPTIONS))
		return;

	if (direction) {
	    sprintf(nfrontp, "td: %s suboption ",
					direction == '<' ? "recv" : "send");
	    nfrontp += strlen(nfrontp);
	    if (length >= 3) {
		register int j;

		i = pointer[length-2];
		j = pointer[length-1];

		if (i != IAC || j != SE) {
		    sprintf(nfrontp, "(terminated by ");
		    nfrontp += strlen(nfrontp);
		    if (TELOPT_OK(i))
			sprintf(nfrontp, "%s ", TELOPT(i));
		    else if (TELCMD_OK(i))
			sprintf(nfrontp, "%s ", TELCMD(i));
		    else
			sprintf(nfrontp, "%d ", i);
		    nfrontp += strlen(nfrontp);
		    if (TELOPT_OK(j))
			sprintf(nfrontp, "%s", TELOPT(j));
		    else if (TELCMD_OK(j))
			sprintf(nfrontp, "%s", TELCMD(j));
		    else
			sprintf(nfrontp, "%d", j);
		    nfrontp += strlen(nfrontp);
		    sprintf(nfrontp, ", not IAC SE!) ");
		    nfrontp += strlen(nfrontp);
		}
	    }
	    length -= 2;
	}
	if (length < 1) {
	    sprintf(nfrontp, "(Empty suboption??\?)");
	    nfrontp += strlen(nfrontp);
	    return;
	}
	switch (pointer[0]) {
	case TELOPT_TTYPE:
	    sprintf(nfrontp, "TERMINAL-TYPE ");
	    nfrontp += strlen(nfrontp);
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		sprintf(nfrontp, "IS \"%.*s\"", length-2, (char *)pointer+2);
		break;
	    case TELQUAL_SEND:
		sprintf(nfrontp, "SEND");
		break;
	    default:
		sprintf(nfrontp,
				"- unknown qualifier %d (0x%x).",
				pointer[1], pointer[1]);
	    }
	    nfrontp += strlen(nfrontp);
	    break;
	case TELOPT_TSPEED:
	    sprintf(nfrontp, "TERMINAL-SPEED");
	    nfrontp += strlen(nfrontp);
	    if (length < 2) {
		sprintf(nfrontp, " (empty suboption??\?)");
		nfrontp += strlen(nfrontp);
		break;
	    }
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		sprintf(nfrontp, " IS %.*s", length-2, (char *)pointer+2);
		nfrontp += strlen(nfrontp);
		break;
	    default:
		if (pointer[1] == 1)
		    sprintf(nfrontp, " SEND");
		else
		    sprintf(nfrontp, " %d (unknown)", pointer[1]);
		nfrontp += strlen(nfrontp);
		for (i = 2; i < length; i++) {
		    sprintf(nfrontp, " ?%d?", pointer[i]);
		    nfrontp += strlen(nfrontp);
		}
		break;
	    }
	    break;

	case TELOPT_LFLOW:
	    sprintf(nfrontp, "TOGGLE-FLOW-CONTROL");
	    nfrontp += strlen(nfrontp);
	    if (length < 2) {
		sprintf(nfrontp, " (empty suboption??\?)");
		nfrontp += strlen(nfrontp);
		break;
	    }
	    switch (pointer[1]) {
	    case LFLOW_OFF:
		sprintf(nfrontp, " OFF"); break;
	    case LFLOW_ON:
		sprintf(nfrontp, " ON"); break;
	    case LFLOW_RESTART_ANY:
		sprintf(nfrontp, " RESTART-ANY"); break;
	    case LFLOW_RESTART_XON:
		sprintf(nfrontp, " RESTART-XON"); break;
	    default:
		sprintf(nfrontp, " %d (unknown)", pointer[1]);
	    }
	    nfrontp += strlen(nfrontp);
	    for (i = 2; i < length; i++) {
		sprintf(nfrontp, " ?%d?", pointer[i]);
		nfrontp += strlen(nfrontp);
	    }
	    break;

	case TELOPT_NAWS:
	    sprintf(nfrontp, "NAWS");
	    nfrontp += strlen(nfrontp);
	    if (length < 2) {
		sprintf(nfrontp, " (empty suboption??\?)");
		nfrontp += strlen(nfrontp);
		break;
	    }
	    if (length == 2) {
		sprintf(nfrontp, " ?%d?", pointer[1]);
		nfrontp += strlen(nfrontp);
		break;
	    }
	    sprintf(nfrontp, " %d %d (%d)",
		pointer[1], pointer[2],
		(int)((((unsigned int)pointer[1])<<8)|((unsigned int)pointer[2])));
	    nfrontp += strlen(nfrontp);
	    if (length == 4) {
		sprintf(nfrontp, " ?%d?", pointer[3]);
		nfrontp += strlen(nfrontp);
		break;
	    }
	    sprintf(nfrontp, " %d %d (%d)",
		pointer[3], pointer[4],
		(int)((((unsigned int)pointer[3])<<8)|((unsigned int)pointer[4])));
	    nfrontp += strlen(nfrontp);
	    for (i = 5; i < length; i++) {
		sprintf(nfrontp, " ?%d?", pointer[i]);
		nfrontp += strlen(nfrontp);
	    }
	    break;

	case TELOPT_LINEMODE:
	    sprintf(nfrontp, "LINEMODE ");
	    nfrontp += strlen(nfrontp);
	    if (length < 2) {
		sprintf(nfrontp, " (empty suboption??\?)");
		nfrontp += strlen(nfrontp);
		break;
	    }
	    switch (pointer[1]) {
	    case WILL:
		sprintf(nfrontp, "WILL ");
		goto common;
	    case WONT:
		sprintf(nfrontp, "WONT ");
		goto common;
	    case DO:
		sprintf(nfrontp, "DO ");
		goto common;
	    case DONT:
		sprintf(nfrontp, "DONT ");
	    common:
		nfrontp += strlen(nfrontp);
		if (length < 3) {
		    sprintf(nfrontp, "(no option??\?)");
		    nfrontp += strlen(nfrontp);
		    break;
		}
		switch (pointer[2]) {
		case LM_FORWARDMASK:
		    sprintf(nfrontp, "Forward Mask");
		    nfrontp += strlen(nfrontp);
		    for (i = 3; i < length; i++) {
			sprintf(nfrontp, " %x", pointer[i]);
			nfrontp += strlen(nfrontp);
		    }
		    break;
		default:
		    sprintf(nfrontp, "%d (unknown)", pointer[2]);
		    nfrontp += strlen(nfrontp);
		    for (i = 3; i < length; i++) {
			sprintf(nfrontp, " %d", pointer[i]);
			nfrontp += strlen(nfrontp);
		    }
		    break;
		}
		break;

	    case LM_SLC:
		sprintf(nfrontp, "SLC");
		nfrontp += strlen(nfrontp);
		for (i = 2; i < length - 2; i += 3) {
		    if (SLC_NAME_OK(pointer[i+SLC_FUNC]))
			sprintf(nfrontp, " %s", SLC_NAME(pointer[i+SLC_FUNC]));
		    else
			sprintf(nfrontp, " %d", pointer[i+SLC_FUNC]);
		    nfrontp += strlen(nfrontp);
		    switch (pointer[i+SLC_FLAGS]&SLC_LEVELBITS) {
		    case SLC_NOSUPPORT:
			sprintf(nfrontp, " NOSUPPORT"); break;
		    case SLC_CANTCHANGE:
			sprintf(nfrontp, " CANTCHANGE"); break;
		    case SLC_VARIABLE:
			sprintf(nfrontp, " VARIABLE"); break;
		    case SLC_DEFAULT:
			sprintf(nfrontp, " DEFAULT"); break;
		    }
		    nfrontp += strlen(nfrontp);
		    sprintf(nfrontp, "%s%s%s",
			pointer[i+SLC_FLAGS]&SLC_ACK ? "|ACK" : "",
			pointer[i+SLC_FLAGS]&SLC_FLUSHIN ? "|FLUSHIN" : "",
			pointer[i+SLC_FLAGS]&SLC_FLUSHOUT ? "|FLUSHOUT" : "");
		    nfrontp += strlen(nfrontp);
		    if (pointer[i+SLC_FLAGS]& ~(SLC_ACK|SLC_FLUSHIN|
						SLC_FLUSHOUT| SLC_LEVELBITS)) {
			sprintf(nfrontp, "(0x%x)", pointer[i+SLC_FLAGS]);
			nfrontp += strlen(nfrontp);
		    }
		    sprintf(nfrontp, " %d;", pointer[i+SLC_VALUE]);
		    nfrontp += strlen(nfrontp);
		    if ((pointer[i+SLC_VALUE] == IAC) &&
			(pointer[i+SLC_VALUE+1] == IAC))
				i++;
		}
		for (; i < length; i++) {
		    sprintf(nfrontp, " ?%d?", pointer[i]);
		    nfrontp += strlen(nfrontp);
		}
		break;

	    case LM_MODE:
		sprintf(nfrontp, "MODE ");
		nfrontp += strlen(nfrontp);
		if (length < 3) {
		    sprintf(nfrontp, "(no mode??\?)");
		    nfrontp += strlen(nfrontp);
		    break;
		}
		{
		    char tbuf[32];
		    sprintf(tbuf, "%s%s%s%s%s",
			pointer[2]&MODE_EDIT ? "|EDIT" : "",
			pointer[2]&MODE_TRAPSIG ? "|TRAPSIG" : "",
			pointer[2]&MODE_SOFT_TAB ? "|SOFT_TAB" : "",
			pointer[2]&MODE_LIT_ECHO ? "|LIT_ECHO" : "",
			pointer[2]&MODE_ACK ? "|ACK" : "");
		    sprintf(nfrontp, "%s", tbuf[1] ? &tbuf[1] : "0");
		    nfrontp += strlen(nfrontp);
		}
		if (pointer[2]&~(MODE_EDIT|MODE_TRAPSIG|MODE_ACK)) {
		    sprintf(nfrontp, " (0x%x)", pointer[2]);
		    nfrontp += strlen(nfrontp);
		}
		for (i = 3; i < length; i++) {
		    sprintf(nfrontp, " ?0x%x?", pointer[i]);
		    nfrontp += strlen(nfrontp);
		}
		break;
	    default:
		sprintf(nfrontp, "%d (unknown)", pointer[1]);
		nfrontp += strlen(nfrontp);
		for (i = 2; i < length; i++) {
		    sprintf(nfrontp, " %d", pointer[i]);
		    nfrontp += strlen(nfrontp);
		}
	    }
	    break;

	case TELOPT_STATUS: {
	    register char *cp;
	    register int j, k;

	    sprintf(nfrontp, "STATUS");
	    nfrontp += strlen(nfrontp);

	    switch (pointer[1]) {
	    default:
		if (pointer[1] == TELQUAL_SEND)
		    sprintf(nfrontp, " SEND");
		else
		    sprintf(nfrontp, " %d (unknown)", pointer[1]);
		nfrontp += strlen(nfrontp);
		for (i = 2; i < length; i++) {
		    sprintf(nfrontp, " ?%d?", pointer[i]);
		    nfrontp += strlen(nfrontp);
		}
		break;
	    case TELQUAL_IS:
		sprintf(nfrontp, " IS\r\n");
		nfrontp += strlen(nfrontp);

		for (i = 2; i < length; i++) {
		    switch(pointer[i]) {
		    case DO:	cp = "DO"; goto common2;
		    case DONT:	cp = "DONT"; goto common2;
		    case WILL:	cp = "WILL"; goto common2;
		    case WONT:	cp = "WONT"; goto common2;
		    common2:
			i++;
			if (TELOPT_OK(pointer[i]))
			    sprintf(nfrontp, " %s %s", cp, TELOPT(pointer[i]));
			else
			    sprintf(nfrontp, " %s %d", cp, pointer[i]);
			nfrontp += strlen(nfrontp);

			sprintf(nfrontp, "\r\n");
			nfrontp += strlen(nfrontp);
			break;

		    case SB:
			sprintf(nfrontp, " SB ");
			nfrontp += strlen(nfrontp);
			i++;
			j = k = i;
			while (j < length) {
			    if (pointer[j] == SE) {
				if (j+1 == length)
				    break;
				if (pointer[j+1] == SE)
				    j++;
				else
				    break;
			    }
			    pointer[k++] = pointer[j++];
			}
			printsub(0, &pointer[i], k - i);
			if (i < length) {
			    sprintf(nfrontp, " SE");
			    nfrontp += strlen(nfrontp);
			    i = j;
			} else
			    i = j - 1;

			sprintf(nfrontp, "\r\n");
			nfrontp += strlen(nfrontp);

			break;

		    default:
			sprintf(nfrontp, " %d", pointer[i]);
			nfrontp += strlen(nfrontp);
			break;
		    }
		}
		break;
	    }
	    break;
	  }

	case TELOPT_XDISPLOC:
	    sprintf(nfrontp, "X-DISPLAY-LOCATION ");
	    nfrontp += strlen(nfrontp);
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		sprintf(nfrontp, "IS \"%.*s\"", length-2, (char *)pointer+2);
		break;
	    case TELQUAL_SEND:
		sprintf(nfrontp, "SEND");
		break;
	    default:
		sprintf(nfrontp, "- unknown qualifier %d (0x%x).",
				pointer[1], pointer[1]);
	    }
	    nfrontp += strlen(nfrontp);
	    break;

	case TELOPT_NEW_ENVIRON:
	    sprintf(nfrontp, "NEW-ENVIRON ");
	    goto env_common1;
	case TELOPT_OLD_ENVIRON:
	    sprintf(nfrontp, "OLD-ENVIRON");
	env_common1:
	    nfrontp += strlen(nfrontp);
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		sprintf(nfrontp, "IS ");
		goto env_common;
	    case TELQUAL_SEND:
		sprintf(nfrontp, "SEND ");
		goto env_common;
	    case TELQUAL_INFO:
		sprintf(nfrontp, "INFO ");
	    env_common:
		nfrontp += strlen(nfrontp);
		{
		    register int noquote = 2;
		    for (i = 2; i < length; i++ ) {
			switch (pointer[i]) {
			case NEW_ENV_VAR:
			    sprintf(nfrontp, "\" VAR " + noquote);
			    nfrontp += strlen(nfrontp);
			    noquote = 2;
			    break;

			case NEW_ENV_VALUE:
			    sprintf(nfrontp, "\" VALUE " + noquote);
			    nfrontp += strlen(nfrontp);
			    noquote = 2;
			    break;

			case ENV_ESC:
			    sprintf(nfrontp, "\" ESC " + noquote);
			    nfrontp += strlen(nfrontp);
			    noquote = 2;
			    break;

			case ENV_USERVAR:
			    sprintf(nfrontp, "\" USERVAR " + noquote);
			    nfrontp += strlen(nfrontp);
			    noquote = 2;
			    break;

			default:
			def_case:
			    if (isprint(pointer[i]) && pointer[i] != '"') {
				if (noquote) {
				    *nfrontp++ = '"';
				    noquote = 0;
				}
				*nfrontp++ = pointer[i];
			    } else {
				sprintf(nfrontp, "\" %03o " + noquote,
							pointer[i]);
				nfrontp += strlen(nfrontp);
				noquote = 2;
			    }
			    break;
			}
		    }
		    if (!noquote)
			*nfrontp++ = '"';
		    break;
		}
	    }
	    break;

#if	defined(AUTHENTICATION)
	case TELOPT_AUTHENTICATION:
	    sprintf(nfrontp, "AUTHENTICATION");
	    nfrontp += strlen(nfrontp);

	    if (length < 2) {
		sprintf(nfrontp, " (empty suboption??\?)");
		nfrontp += strlen(nfrontp);
		break;
	    }
	    switch (pointer[1]) {
	    case TELQUAL_REPLY:
	    case TELQUAL_IS:
		sprintf(nfrontp, " %s ", (pointer[1] == TELQUAL_IS) ?
							"IS" : "REPLY");
		nfrontp += strlen(nfrontp);
		if (AUTHTYPE_NAME_OK(pointer[2]))
		    sprintf(nfrontp, "%s ", AUTHTYPE_NAME(pointer[2]));
		else
		    sprintf(nfrontp, "%d ", pointer[2]);
		nfrontp += strlen(nfrontp);
		if (length < 3) {
		    sprintf(nfrontp, "(partial suboption??\?)");
		    nfrontp += strlen(nfrontp);
		    break;
		}
		sprintf(nfrontp, "%s|%s",
			((pointer[3] & AUTH_WHO_MASK) == AUTH_WHO_CLIENT) ?
			"CLIENT" : "SERVER",
			((pointer[3] & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) ?
			"MUTUAL" : "ONE-WAY");
		nfrontp += strlen(nfrontp);

		auth_printsub(&pointer[1], length - 1, buf, sizeof(buf));
		sprintf(nfrontp, "%s", buf);
		nfrontp += strlen(nfrontp);
		break;

	    case TELQUAL_SEND:
		i = 2;
		sprintf(nfrontp, " SEND ");
		nfrontp += strlen(nfrontp);
		while (i < length) {
		    if (AUTHTYPE_NAME_OK(pointer[i]))
			sprintf(nfrontp, "%s ", AUTHTYPE_NAME(pointer[i]));
		    else
			sprintf(nfrontp, "%d ", pointer[i]);
		    nfrontp += strlen(nfrontp);
		    if (++i >= length) {
			sprintf(nfrontp, "(partial suboption??\?)");
			nfrontp += strlen(nfrontp);
			break;
		    }
		    sprintf(nfrontp, "%s|%s ",
			((pointer[i] & AUTH_WHO_MASK) == AUTH_WHO_CLIENT) ?
							"CLIENT" : "SERVER",
			((pointer[i] & AUTH_HOW_MASK) == AUTH_HOW_MUTUAL) ?
							"MUTUAL" : "ONE-WAY");
		    nfrontp += strlen(nfrontp);
		    ++i;
		}
		break;

	    case TELQUAL_NAME:
		i = 2;
		sprintf(nfrontp, " NAME \"");
		nfrontp += strlen(nfrontp);
		while (i < length)
		    *nfrontp += pointer[i++];
		*nfrontp += '"';
		break;

	    default:
		    for (i = 2; i < length; i++) {
			sprintf(nfrontp, " ?%d?", pointer[i]);
			nfrontp += strlen(nfrontp);
		    }
		    break;
	    }
	    break;
#endif

#ifdef	ENCRYPTION
	case TELOPT_ENCRYPT:
	    sprintf(nfrontp, "ENCRYPT");
	    nfrontp += strlen(nfrontp);
	    if (length < 2) {
		sprintf(nfrontp, " (empty suboption??\?)");
		nfrontp += strlen(nfrontp);
		break;
	    }
	    switch (pointer[1]) {
	    case ENCRYPT_START:
		sprintf(nfrontp, " START");
		nfrontp += strlen(nfrontp);
		break;

	    case ENCRYPT_END:
		sprintf(nfrontp, " END");
		nfrontp += strlen(nfrontp);
		break;

	    case ENCRYPT_REQSTART:
		sprintf(nfrontp, " REQUEST-START");
		nfrontp += strlen(nfrontp);
		break;

	    case ENCRYPT_REQEND:
		sprintf(nfrontp, " REQUEST-END");
		nfrontp += strlen(nfrontp);
		break;

	    case ENCRYPT_IS:
	    case ENCRYPT_REPLY:
		sprintf(nfrontp, " %s ", (pointer[1] == ENCRYPT_IS) ?
							"IS" : "REPLY");
		nfrontp += strlen(nfrontp);
		if (length < 3) {
		    sprintf(nfrontp, " (partial suboption??\?)");
		    nfrontp += strlen(nfrontp);
		    break;
		}
		if (ENCTYPE_NAME_OK(pointer[2]))
		    sprintf(nfrontp, "%s ", ENCTYPE_NAME(pointer[2]));
		else
		    sprintf(nfrontp, " %d (unknown)", pointer[2]);
		nfrontp += strlen(nfrontp);

		encrypt_printsub(&pointer[1], length - 1, buf, sizeof(buf));
		sprintf(nfrontp, "%s", buf);
		nfrontp += strlen(nfrontp);
		break;

	    case ENCRYPT_SUPPORT:
		i = 2;
		sprintf(nfrontp, " SUPPORT ");
		nfrontp += strlen(nfrontp);
		while (i < length) {
		    if (ENCTYPE_NAME_OK(pointer[i]))
			sprintf(nfrontp, "%s ", ENCTYPE_NAME(pointer[i]));
		    else
			sprintf(nfrontp, "%d ", pointer[i]);
		    nfrontp += strlen(nfrontp);
		    i++;
		}
		break;

	    case ENCRYPT_ENC_KEYID:
		sprintf(nfrontp, " ENC_KEYID", pointer[1]);
		nfrontp += strlen(nfrontp);
		goto encommon;

	    case ENCRYPT_DEC_KEYID:
		sprintf(nfrontp, " DEC_KEYID", pointer[1]);
		nfrontp += strlen(nfrontp);
		goto encommon;

	    default:
		sprintf(nfrontp, " %d (unknown)", pointer[1]);
		nfrontp += strlen(nfrontp);
	    encommon:
		for (i = 2; i < length; i++) {
		    sprintf(nfrontp, " %d", pointer[i]);
		    nfrontp += strlen(nfrontp);
		}
		break;
	    }
	    break;
#endif	/* ENCRYPTION */

	default:
	    if (TELOPT_OK(pointer[0]))
	        sprintf(nfrontp, "%s (unknown)", TELOPT(pointer[0]));
	    else
	        sprintf(nfrontp, "%d (unknown)", pointer[i]);
	    nfrontp += strlen(nfrontp);
	    for (i = 1; i < length; i++) {
		sprintf(nfrontp, " %d", pointer[i]);
		nfrontp += strlen(nfrontp);
	    }
	    break;
	}
	sprintf(nfrontp, "\r\n");
	nfrontp += strlen(nfrontp);
}

/*
 * Dump a data buffer in hex and ascii to the output data stream.
 */
	void
printdata(tag, ptr, cnt)
	register char *tag;
	register char *ptr;
	register int cnt;
{
	register int i;
	char xbuf[30];

	while (cnt) {
		/* flush net output buffer if no room for new data) */
		if ((&netobuf[BUFSIZ] - nfrontp) < 80) {
			netflush();
		}

		/* add a line of output */
		sprintf(nfrontp, "%s: ", tag);
		nfrontp += strlen(nfrontp);
		for (i = 0; i < 20 && cnt; i++) {
			sprintf(nfrontp, "%02x", *ptr);
			nfrontp += strlen(nfrontp);
			if (isprint(*ptr)) {
				xbuf[i] = *ptr;
			} else {
				xbuf[i] = '.';
			}
			if (i % 2) {
				*nfrontp = ' ';
				nfrontp++;
			}
			cnt--;
			ptr++;
		}
		xbuf[i] = '\0';
		sprintf(nfrontp, " %s\r\n", xbuf );
		nfrontp += strlen(nfrontp);
	}
}
#endif /* DIAGNOSTICS */
