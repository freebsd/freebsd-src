/*
 * Copyright (c) 1983, 1995 Eric P. Allman
 * Copyright (c) 1988, 1993
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
static char sccsid[] = "@(#)parseaddr.c	8.87 (Berkeley) 11/29/95";
#endif /* not lint */

# include "sendmail.h"

/*
**  PARSEADDR -- Parse an address
**
**	Parses an address and breaks it up into three parts: a
**	net to transmit the message on, the host to transmit it
**	to, and a user on that host.  These are loaded into an
**	ADDRESS header with the values squirreled away if necessary.
**	The "user" part may not be a real user; the process may
**	just reoccur on that machine.  For example, on a machine
**	with an arpanet connection, the address
**		csvax.bill@berkeley
**	will break up to a "user" of 'csvax.bill' and a host
**	of 'berkeley' -- to be transmitted over the arpanet.
**
**	Parameters:
**		addr -- the address to parse.
**		a -- a pointer to the address descriptor buffer.
**			If NULL, a header will be created.
**		flags -- describe detail for parsing.  See RF_ definitions
**			in sendmail.h.
**		delim -- the character to terminate the address, passed
**			to prescan.
**		delimptr -- if non-NULL, set to the location of the
**			delim character that was found.
**		e -- the envelope that will contain this address.
**
**	Returns:
**		A pointer to the address descriptor header (`a' if
**			`a' is non-NULL).
**		NULL on error.
**
**	Side Effects:
**		none
*/

/* following delimiters are inherent to the internal algorithms */
# define DELIMCHARS	"()<>,;\r\n"	/* default word delimiters */

ADDRESS *
parseaddr(addr, a, flags, delim, delimptr, e)
	char *addr;
	register ADDRESS *a;
	int flags;
	int delim;
	char **delimptr;
	register ENVELOPE *e;
{
	register char **pvp;
	auto char *delimptrbuf;
	bool queueup;
	char pvpbuf[PSBUFSIZE];
	extern ADDRESS *buildaddr();
	extern bool invalidaddr();
	extern void allocaddr __P((ADDRESS *, int, char *));

	/*
	**  Initialize and prescan address.
	*/

	e->e_to = addr;
	if (tTd(20, 1))
		printf("\n--parseaddr(%s)\n", addr);

	if (delimptr == NULL)
		delimptr = &delimptrbuf;

	pvp = prescan(addr, delim, pvpbuf, sizeof pvpbuf, delimptr, NULL);
	if (pvp == NULL)
	{
		if (tTd(20, 1))
			printf("parseaddr-->NULL\n");
		return (NULL);
	}

	if (invalidaddr(addr, delim == '\0' ? NULL : *delimptr))
	{
		if (tTd(20, 1))
			printf("parseaddr-->bad address\n");
		return NULL;
	}

	/*
	**  Save addr if we are going to have to.
	**
	**	We have to do this early because there is a chance that
	**	the map lookups in the rewriting rules could clobber
	**	static memory somewhere.
	*/

	if (bitset(RF_COPYPADDR, flags) && addr != NULL)
	{
		char savec = **delimptr;

		if (savec != '\0')
			**delimptr = '\0';
		e->e_to = addr = newstr(addr);
		if (savec != '\0')
			**delimptr = savec;
	}

	/*
	**  Apply rewriting rules.
	**	Ruleset 0 does basic parsing.  It must resolve.
	*/

	queueup = FALSE;
	if (rewrite(pvp, 3, 0, e) == EX_TEMPFAIL)
		queueup = TRUE;
	if (rewrite(pvp, 0, 0, e) == EX_TEMPFAIL)
		queueup = TRUE;


	/*
	**  Build canonical address from pvp.
	*/

	a = buildaddr(pvp, a, flags, e);

	/*
	**  Make local copies of the host & user and then
	**  transport them out.
	*/

	allocaddr(a, flags, addr);
	if (bitset(QBADADDR, a->q_flags))
		return a;

	/*
	**  If there was a parsing failure, mark it for queueing.
	*/

	if (queueup)
	{
		char *msg = "Transient parse error -- message queued for future delivery";

		if (e->e_sendmode == SM_DEFER)
			msg = "Deferring message until queue run";
		if (tTd(20, 1))
			printf("parseaddr: queuing message\n");
		message(msg);
		if (e->e_message == NULL && e->e_sendmode != SM_DEFER)
			e->e_message = newstr(msg);
		a->q_flags |= QQUEUEUP;
		a->q_status = "4.4.3";
	}

	/*
	**  Compute return value.
	*/

	if (tTd(20, 1))
	{
		printf("parseaddr-->");
		printaddr(a, FALSE);
	}

	return (a);
}
/*
**  INVALIDADDR -- check for address containing meta-characters
**
**	Parameters:
**		addr -- the address to check.
**
**	Returns:
**		TRUE -- if the address has any "wierd" characters
**		FALSE -- otherwise.
*/

bool
invalidaddr(addr, delimptr)
	register char *addr;
	char *delimptr;
{
	char savedelim = '\0';

	if (delimptr != NULL)
	{
		savedelim = *delimptr;
		if (savedelim != '\0')
			*delimptr = '\0';
	}
#if 0
	/* for testing.... */
	if (strcmp(addr, "INvalidADDR") == 0)
	{
		usrerr("553 INvalid ADDRess");
		goto addrfailure;
	}
#endif
	for (; *addr != '\0'; addr++)
	{
		if ((*addr & 0340) == 0200)
			break;
	}
	if (*addr == '\0')
	{
		if (delimptr != NULL && savedelim != '\0')
			*delimptr = savedelim;
		return FALSE;
	}
	setstat(EX_USAGE);
	usrerr("553 Address contained invalid control characters");
  addrfailure:
	if (delimptr != NULL && savedelim != '\0')
		*delimptr = savedelim;
	return TRUE;
}
/*
**  ALLOCADDR -- do local allocations of address on demand.
**
**	Also lowercases the host name if requested.
**
**	Parameters:
**		a -- the address to reallocate.
**		flags -- the copy flag (see RF_ definitions in sendmail.h
**			for a description).
**		paddr -- the printname of the address.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Copies portions of a into local buffers as requested.
*/

void
allocaddr(a, flags, paddr)
	register ADDRESS *a;
	int flags;
	char *paddr;
{
	if (tTd(24, 4))
		printf("allocaddr(flags=%x, paddr=%s)\n", flags, paddr);

	a->q_paddr = paddr;

	if (a->q_user == NULL)
		a->q_user = "";
	if (a->q_host == NULL)
		a->q_host = "";

	if (bitset(RF_COPYPARSE, flags))
	{
		a->q_host = newstr(a->q_host);
		if (a->q_user != a->q_paddr)
			a->q_user = newstr(a->q_user);
	}

	if (a->q_paddr == NULL)
		a->q_paddr = a->q_user;
}
/*
**  PRESCAN -- Prescan name and make it canonical
**
**	Scans a name and turns it into a set of tokens.  This process
**	deletes blanks and comments (in parentheses).
**
**	This routine knows about quoted strings and angle brackets.
**
**	There are certain subtleties to this routine.  The one that
**	comes to mind now is that backslashes on the ends of names
**	are silently stripped off; this is intentional.  The problem
**	is that some versions of sndmsg (like at LBL) set the kill
**	character to something other than @ when reading addresses;
**	so people type "csvax.eric\@berkeley" -- which screws up the
**	berknet mailer.
**
**	Parameters:
**		addr -- the name to chomp.
**		delim -- the delimiter for the address, normally
**			'\0' or ','; \0 is accepted in any case.
**			If '\t' then we are reading the .cf file.
**		pvpbuf -- place to put the saved text -- note that
**			the pointers are static.
**		pvpbsize -- size of pvpbuf.
**		delimptr -- if non-NULL, set to the location of the
**			terminating delimiter.
**		toktab -- if set, a token table to use for parsing.
**			If NULL, use the default table.
**
**	Returns:
**		A pointer to a vector of tokens.
**		NULL on error.
*/

/* states and character types */
# define OPR		0	/* operator */
# define ATM		1	/* atom */
# define QST		2	/* in quoted string */
# define SPC		3	/* chewing up spaces */
# define ONE		4	/* pick up one character */
# define ILL		5	/* illegal character */

# define NSTATES	6	/* number of states */
# define TYPE		017	/* mask to select state type */

/* meta bits for table */
# define M		020	/* meta character; don't pass through */
# define B		040	/* cause a break */
# define MB		M|B	/* meta-break */

static short StateTab[NSTATES][NSTATES] =
{
   /*	oldst	chtype>	OPR	ATM	QST	SPC	ONE	ILL	*/
	/*OPR*/		OPR|B,	ATM|B,	QST|B,	SPC|MB,	ONE|B,	ILL|MB,
	/*ATM*/		OPR|B,	ATM,	QST|B,	SPC|MB,	ONE|B,	ILL|MB,
	/*QST*/		QST,	QST,	OPR,	QST,	QST,	QST,
	/*SPC*/		OPR,	ATM,	QST,	SPC|M,	ONE,	ILL|MB,
	/*ONE*/		OPR,	OPR,	OPR,	OPR,	OPR,	ILL|MB,
	/*ILL*/		OPR|B,	ATM|B,	QST|B,	SPC|MB,	ONE|B,	ILL|M,
};

/* token type table -- it gets modified with $o characters */
static u_char	TokTypeTab[256] =
{
    /*	nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si   */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,SPC,SPC,SPC,SPC,SPC,ATM,ATM,
    /*	dle dc1 dc2 dc3 dc4 nak syn etb  can em  sub esc fs  gs  rs  us   */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  sp  !   "   #   $   %   &   '    (   )   *   +   ,   -   .   /    */
	SPC,ATM,QST,ATM,ATM,ATM,ATM,ATM, ATM,SPC,ATM,ATM,ATM,ATM,ATM,ATM,
    /*	0   1   2   3   4   5   6   7    8   9   :   ;   <   =   >   ?    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*	@   A   B   C   D   E   F   G    H   I   J   K   L   M   N   O    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  P   Q   R   S   T   U   V   W    X   Y   Z   [   \   ]   ^   _    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*	`   a   b   c   d   e   f   g    h   i   j   k   l   m   n   o    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  p   q   r   s   t   u   v   w    x   y   z   {   |   }   ~   del  */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,

    /*	nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si   */
	OPR,OPR,ONE,OPR,OPR,OPR,OPR,OPR, OPR,OPR,OPR,OPR,OPR,OPR,OPR,OPR,
    /*	dle dc1 dc2 dc3 dc4 nak syn etb  can em  sub esc fs  gs  rs  us   */
	OPR,OPR,OPR,ONE,ONE,ONE,OPR,OPR, OPR,OPR,OPR,OPR,OPR,OPR,OPR,OPR,
    /*  sp  !   "   #   $   %   &   '    (   )   *   +   ,   -   .   /    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*	0   1   2   3   4   5   6   7    8   9   :   ;   <   =   >   ?    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*	@   A   B   C   D   E   F   G    H   I   J   K   L   M   N   O    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  P   Q   R   S   T   U   V   W    X   Y   Z   [   \   ]   ^   _    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*	`   a   b   c   d   e   f   g    h   i   j   k   l   m   n   o    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  p   q   r   s   t   u   v   w    x   y   z   {   |   }   ~   del  */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
};

/* token type table for MIME parsing */
u_char	MimeTokenTab[256] =
{
    /*	nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si   */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,SPC,SPC,SPC,SPC,SPC,ILL,ILL,
    /*	dle dc1 dc2 dc3 dc4 nak syn etb  can em  sub esc fs  gs  rs  us   */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*  sp  !   "   #   $   %   &   '    (   )   *   +   ,   -   .   /    */
	SPC,ATM,QST,ATM,ATM,ATM,ATM,ATM, ATM,SPC,ATM,ATM,OPR,ATM,ATM,OPR,
    /*	0   1   2   3   4   5   6   7    8   9   :   ;   <   =   >   ?    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,OPR,OPR,OPR,OPR,OPR,OPR,
    /*	@   A   B   C   D   E   F   G    H   I   J   K   L   M   N   O    */
	OPR,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  P   Q   R   S   T   U   V   W    X   Y   Z   [   \   ]   ^   _    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,OPR,OPR,OPR,ATM,ATM,
    /*	`   a   b   c   d   e   f   g    h   i   j   k   l   m   n   o    */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  p   q   r   s   t   u   v   w    x   y   z   {   |   }   ~   del  */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,

    /*	nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si   */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*	dle dc1 dc2 dc3 dc4 nak syn etb  can em  sub esc fs  gs  rs  us   */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*  sp  !   "   #   $   %   &   '    (   )   *   +   ,   -   .   /    */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*	0   1   2   3   4   5   6   7    8   9   :   ;   <   =   >   ?    */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*	@   A   B   C   D   E   F   G    H   I   J   K   L   M   N   O    */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*  P   Q   R   S   T   U   V   W    X   Y   Z   [   \   ]   ^   _    */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*	`   a   b   c   d   e   f   g    h   i   j   k   l   m   n   o    */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
    /*  p   q   r   s   t   u   v   w    x   y   z   {   |   }   ~   del  */
	ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL, ILL,ILL,ILL,ILL,ILL,ILL,ILL,ILL,
};


# define NOCHAR		-1	/* signal nothing in lookahead token */

char **
prescan(addr, delim, pvpbuf, pvpbsize, delimptr, toktab)
	char *addr;
	int delim;
	char pvpbuf[];
	int pvpbsize;
	char **delimptr;
	u_char *toktab;
{
	register char *p;
	register char *q;
	register int c;
	char **avp;
	bool bslashmode;
	int cmntcnt;
	int anglecnt;
	char *tok;
	int state;
	int newstate;
	char *saveto = CurEnv->e_to;
	static char *av[MAXATOM+1];
	static char firsttime = TRUE;
	extern int errno;

	if (firsttime)
	{
		/* initialize the token type table */
		char obuf[50];

		firsttime = FALSE;
		if (OperatorChars == NULL)
		{
			if (ConfigLevel < 7)
				OperatorChars = macvalue('o', CurEnv);
			if (OperatorChars == NULL)
				OperatorChars = ".:@[]";
		}
		expand(OperatorChars, obuf, sizeof obuf - sizeof DELIMCHARS, CurEnv);
		strcat(obuf, DELIMCHARS);
		for (p = obuf; *p != '\0'; p++)
		{
			if (TokTypeTab[*p & 0xff] == ATM)
				TokTypeTab[*p & 0xff] = OPR;
		}
	}
	if (toktab == NULL)
		toktab = TokTypeTab;

	/* make sure error messages don't have garbage on them */
	errno = 0;

	q = pvpbuf;
	bslashmode = FALSE;
	cmntcnt = 0;
	anglecnt = 0;
	avp = av;
	state = ATM;
	c = NOCHAR;
	p = addr;
	CurEnv->e_to = p;
	if (tTd(22, 11))
	{
		printf("prescan: ");
		xputs(p);
		(void) putchar('\n');
	}

	do
	{
		/* read a token */
		tok = q;
		for (;;)
		{
			/* store away any old lookahead character */
			if (c != NOCHAR && !bslashmode)
			{
				/* see if there is room */
				if (q >= &pvpbuf[pvpbsize - 5])
				{
					usrerr("553 Address too long");
					if (strlen(addr) > MAXNAME)
						addr[MAXNAME] = '\0';
	returnnull:
					if (delimptr != NULL)
						*delimptr = p;
					CurEnv->e_to = saveto;
					return (NULL);
				}

				/* squirrel it away */
				*q++ = c;
			}

			/* read a new input character */
			c = *p++;
			if (c == '\0')
			{
				/* diagnose and patch up bad syntax */
				if (state == QST)
				{
					usrerr("653 Unbalanced '\"'");
					c = '"';
				}
				else if (cmntcnt > 0)
				{
					usrerr("653 Unbalanced '('");
					c = ')';
				}
				else if (anglecnt > 0)
				{
					c = '>';
					usrerr("653 Unbalanced '<'");
				}
				else
					break;

				p--;
			}
			else if (c == delim && anglecnt <= 0 &&
					cmntcnt <= 0 && state != QST)
				break;

			if (tTd(22, 101))
				printf("c=%c, s=%d; ", c, state);

			/* chew up special characters */
			*q = '\0';
			if (bslashmode)
			{
				bslashmode = FALSE;

				/* kludge \! for naive users */
				if (cmntcnt > 0)
				{
					c = NOCHAR;
					continue;
				}
				else if (c != '!' || state == QST)
				{
					*q++ = '\\';
					continue;
				}
			}

			if (c == '\\')
			{
				bslashmode = TRUE;
			}
			else if (state == QST)
			{
				/* do nothing, just avoid next clauses */
			}
			else if (c == '(')
			{
				cmntcnt++;
				c = NOCHAR;
			}
			else if (c == ')')
			{
				if (cmntcnt <= 0)
				{
					usrerr("653 Unbalanced ')'");
					c = NOCHAR;
				}
				else
					cmntcnt--;
			}
			else if (cmntcnt > 0)
				c = NOCHAR;
			else if (c == '<')
				anglecnt++;
			else if (c == '>')
			{
				if (anglecnt <= 0)
				{
					usrerr("653 Unbalanced '>'");
					c = NOCHAR;
				}
				else
					anglecnt--;
			}
			else if (delim == ' ' && isascii(c) && isspace(c))
				c = ' ';

			if (c == NOCHAR)
				continue;

			/* see if this is end of input */
			if (c == delim && anglecnt <= 0 && state != QST)
				break;

			newstate = StateTab[state][toktab[c & 0xff]];
			if (tTd(22, 101))
				printf("ns=%02o\n", newstate);
			state = newstate & TYPE;
			if (state == ILL)
			{
				if (isascii(c) && isprint(c))
					usrerr("653 Illegal character %c", c);
				else
					usrerr("653 Illegal character 0x%02x", c);
			}
			if (bitset(M, newstate))
				c = NOCHAR;
			if (bitset(B, newstate))
				break;
		}

		/* new token */
		if (tok != q)
		{
			*q++ = '\0';
			if (tTd(22, 36))
			{
				printf("tok=");
				xputs(tok);
				(void) putchar('\n');
			}
			if (avp >= &av[MAXATOM])
			{
				syserr("553 prescan: too many tokens");
				goto returnnull;
			}
			if (q - tok > MAXNAME)
			{
				syserr("553 prescan: token too long");
				goto returnnull;
			}
			*avp++ = tok;
		}
	} while (c != '\0' && (c != delim || anglecnt > 0));
	*avp = NULL;
	p--;
	if (delimptr != NULL)
		*delimptr = p;
	if (tTd(22, 12))
	{
		printf("prescan==>");
		printav(av);
	}
	CurEnv->e_to = saveto;
	if (av[0] == NULL)
	{
		if (tTd(22, 1))
			printf("prescan: null leading token\n");
		return (NULL);
	}
	return (av);
}
/*
**  REWRITE -- apply rewrite rules to token vector.
**
**	This routine is an ordered production system.  Each rewrite
**	rule has a LHS (called the pattern) and a RHS (called the
**	rewrite); 'rwr' points the the current rewrite rule.
**
**	For each rewrite rule, 'avp' points the address vector we
**	are trying to match against, and 'pvp' points to the pattern.
**	If pvp points to a special match value (MATCHZANY, MATCHANY,
**	MATCHONE, MATCHCLASS, MATCHNCLASS) then the address in avp
**	matched is saved away in the match vector (pointed to by 'mvp').
**
**	When a match between avp & pvp does not match, we try to
**	back out.  If we back up over MATCHONE, MATCHCLASS, or MATCHNCLASS
**	we must also back out the match in mvp.  If we reach a
**	MATCHANY or MATCHZANY we just extend the match and start
**	over again.
**
**	When we finally match, we rewrite the address vector
**	and try over again.
**
**	Parameters:
**		pvp -- pointer to token vector.
**		ruleset -- the ruleset to use for rewriting.
**		reclevel -- recursion level (to catch loops).
**		e -- the current envelope.
**
**	Returns:
**		A status code.  If EX_TEMPFAIL, higher level code should
**			attempt recovery.
**
**	Side Effects:
**		pvp is modified.
*/

struct match
{
	char	**first;	/* first token matched */
	char	**last;		/* last token matched */
	char	**pattern;	/* pointer to pattern */
};

# define MAXMATCH	9	/* max params per rewrite */


int
rewrite(pvp, ruleset, reclevel, e)
	char **pvp;
	int ruleset;
	int reclevel;
	register ENVELOPE *e;
{
	register char *ap;		/* address pointer */
	register char *rp;		/* rewrite pointer */
	register char **avp;		/* address vector pointer */
	register char **rvp;		/* rewrite vector pointer */
	register struct match *mlp;	/* cur ptr into mlist */
	register struct rewrite *rwr;	/* pointer to current rewrite rule */
	int ruleno;			/* current rule number */
	int rstat = EX_OK;		/* return status */
	int loopcount;
	struct match mlist[MAXMATCH];	/* stores match on LHS */
	char *npvp[MAXATOM+1];		/* temporary space for rebuild */

	if (OpMode == MD_TEST || tTd(21, 1))
	{
		printf("rewrite: ruleset %2d   input:", ruleset);
		printav(pvp);
	}
	if (ruleset < 0 || ruleset >= MAXRWSETS)
	{
		syserr("554 rewrite: illegal ruleset number %d", ruleset);
		return EX_CONFIG;
	}
	if (reclevel++ > MaxRuleRecursion)
	{
		syserr("rewrite: excessive recursion (max %d), ruleset %d",
			MaxRuleRecursion, ruleset);
		return EX_CONFIG;
	}
	if (pvp == NULL)
		return EX_USAGE;

	/*
	**  Run through the list of rewrite rules, applying
	**	any that match.
	*/

	ruleno = 1;
	loopcount = 0;
	for (rwr = RewriteRules[ruleset]; rwr != NULL; )
	{
		if (tTd(21, 12))
		{
			printf("-----trying rule:");
			printav(rwr->r_lhs);
		}

		/* try to match on this rule */
		mlp = mlist;
		rvp = rwr->r_lhs;
		avp = pvp;
		if (++loopcount > 100)
		{
			syserr("554 Infinite loop in ruleset %d, rule %d",
				ruleset, ruleno);
			if (tTd(21, 1))
			{
				printf("workspace: ");
				printav(pvp);
			}
			break;
		}

		while ((ap = *avp) != NULL || *rvp != NULL)
		{
			rp = *rvp;
			if (tTd(21, 35))
			{
				printf("ADVANCE rp=");
				xputs(rp);
				printf(", ap=");
				xputs(ap);
				printf("\n");
			}
			if (rp == NULL)
			{
				/* end-of-pattern before end-of-address */
				goto backup;
			}
			if (ap == NULL && (*rp & 0377) != MATCHZANY &&
			    (*rp & 0377) != MATCHZERO)
			{
				/* end-of-input with patterns left */
				goto backup;
			}

			switch (*rp & 0377)
			{
				char buf[MAXLINE];

			  case MATCHCLASS:
				/* match any phrase in a class */
				mlp->pattern = rvp;
				mlp->first = avp;
	extendclass:
				ap = *avp;
				if (ap == NULL)
					goto backup;
				mlp->last = avp++;
				cataddr(mlp->first, mlp->last, buf, sizeof buf, '\0');
				if (!wordinclass(buf, rp[1]))
				{
					if (tTd(21, 36))
					{
						printf("EXTEND  rp=");
						xputs(rp);
						printf(", ap=");
						xputs(ap);
						printf("\n");
					}
					goto extendclass;
				}
				if (tTd(21, 36))
					printf("CLMATCH\n");
				mlp++;
				break;

			  case MATCHNCLASS:
				/* match any token not in a class */
				if (wordinclass(ap, rp[1]))
					goto backup;

				/* fall through */

			  case MATCHONE:
			  case MATCHANY:
				/* match exactly one token */
				mlp->pattern = rvp;
				mlp->first = avp;
				mlp->last = avp++;
				mlp++;
				break;

			  case MATCHZANY:
				/* match zero or more tokens */
				mlp->pattern = rvp;
				mlp->first = avp;
				mlp->last = avp - 1;
				mlp++;
				break;

			  case MATCHZERO:
				/* match zero tokens */
				break;

			  case MACRODEXPAND:
				/*
				**  Match against run-time macro.
				**  This algorithm is broken for the
				**  general case (no recursive macros,
				**  improper tokenization) but should
				**  work for the usual cases.
				*/

				ap = macvalue(rp[1], e);
				mlp->first = avp;
				if (tTd(21, 2))
					printf("rewrite: LHS $&%s => \"%s\"\n",
						macname(rp[1]),
						ap == NULL ? "(NULL)" : ap);

				if (ap == NULL)
					break;
				while (*ap != '\0')
				{
					if (*avp == NULL ||
					    strncasecmp(ap, *avp, strlen(*avp)) != 0)
					{
						/* no match */
						avp = mlp->first;
						goto backup;
					}
					ap += strlen(*avp++);
				}

				/* match */
				break;

			  default:
				/* must have exact match */
				if (strcasecmp(rp, ap))
					goto backup;
				avp++;
				break;
			}

			/* successful match on this token */
			rvp++;
			continue;

	  backup:
			/* match failed -- back up */
			while (--mlp >= mlist)
			{
				rvp = mlp->pattern;
				rp = *rvp;
				avp = mlp->last + 1;
				ap = *avp;

				if (tTd(21, 36))
				{
					printf("BACKUP  rp=");
					xputs(rp);
					printf(", ap=");
					xputs(ap);
					printf("\n");
				}

				if (ap == NULL)
				{
					/* run off the end -- back up again */
					continue;
				}
				if ((*rp & 0377) == MATCHANY ||
				    (*rp & 0377) == MATCHZANY)
				{
					/* extend binding and continue */
					mlp->last = avp++;
					rvp++;
					mlp++;
					break;
				}
				if ((*rp & 0377) == MATCHCLASS)
				{
					/* extend binding and try again */
					mlp->last = avp;
					goto extendclass;
				}
			}

			if (mlp < mlist)
			{
				/* total failure to match */
				break;
			}
		}

		/*
		**  See if we successfully matched
		*/

		if (mlp < mlist || *rvp != NULL)
		{
			if (tTd(21, 10))
				printf("----- rule fails\n");
			rwr = rwr->r_next;
			ruleno++;
			loopcount = 0;
			continue;
		}

		rvp = rwr->r_rhs;
		if (tTd(21, 12))
		{
			printf("-----rule matches:");
			printav(rvp);
		}

		rp = *rvp;
		if ((*rp & 0377) == CANONUSER)
		{
			rvp++;
			rwr = rwr->r_next;
			ruleno++;
			loopcount = 0;
		}
		else if ((*rp & 0377) == CANONHOST)
		{
			rvp++;
			rwr = NULL;
		}
		else if ((*rp & 0377) == CANONNET)
			rwr = NULL;

		/* substitute */
		for (avp = npvp; *rvp != NULL; rvp++)
		{
			register struct match *m;
			register char **pp;

			rp = *rvp;
			if ((*rp & 0377) == MATCHREPL)
			{
				/* substitute from LHS */
				m = &mlist[rp[1] - '1'];
				if (m < mlist || m >= mlp)
				{
					syserr("554 rewrite: ruleset %d: replacement $%c out of bounds",
						ruleset, rp[1]);
					return EX_CONFIG;
				}
				if (tTd(21, 15))
				{
					printf("$%c:", rp[1]);
					pp = m->first;
					while (pp <= m->last)
					{
						printf(" %x=\"", *pp);
						(void) fflush(stdout);
						printf("%s\"", *pp++);
					}
					printf("\n");
				}
				pp = m->first;
				while (pp <= m->last)
				{
					if (avp >= &npvp[MAXATOM])
					{
						syserr("554 rewrite: expansion too long");
						return EX_DATAERR;
					}
					*avp++ = *pp++;
				}
			}
			else
			{
				/* vanilla replacement */
				if (avp >= &npvp[MAXATOM])
				{
	toolong:
					syserr("554 rewrite: expansion too long");
					return EX_DATAERR;
				}
				if ((*rp & 0377) != MACRODEXPAND)
					*avp++ = rp;
				else
				{
					*avp = macvalue(rp[1], e);
					if (tTd(21, 2))
						printf("rewrite: RHS $&%s => \"%s\"\n",
							macname(rp[1]),
							*avp == NULL ? "(NULL)" : *avp);
					if (*avp != NULL)
						avp++;
				}
			}
		}
		*avp++ = NULL;

		/*
		**  Check for any hostname/keyword lookups.
		*/

		for (rvp = npvp; *rvp != NULL; rvp++)
		{
			char **hbrvp;
			char **xpvp;
			int trsize;
			char *replac;
			int endtoken;
			STAB *map;
			char *mapname;
			char **key_rvp;
			char **arg_rvp;
			char **default_rvp;
			char buf[MAXNAME + 1];
			char *pvpb1[MAXATOM + 1];
			char *argvect[10];
			char pvpbuf[PSBUFSIZE];
			char *nullpvp[1];

			if ((**rvp & 0377) != HOSTBEGIN &&
			    (**rvp & 0377) != LOOKUPBEGIN)
				continue;

			/*
			**  Got a hostname/keyword lookup.
			**
			**	This could be optimized fairly easily.
			*/

			hbrvp = rvp;
			if ((**rvp & 0377) == HOSTBEGIN)
			{
				endtoken = HOSTEND;
				mapname = "host";
			}
			else
			{
				endtoken = LOOKUPEND;
				mapname = *++rvp;
			}
			map = stab(mapname, ST_MAP, ST_FIND);
			if (map == NULL)
				syserr("554 rewrite: map %s not found", mapname);

			/* extract the match part */
			key_rvp = ++rvp;
			default_rvp = NULL;
			arg_rvp = argvect;
			xpvp = NULL;
			replac = pvpbuf;
			while (*rvp != NULL && (**rvp & 0377) != endtoken)
			{
				int nodetype = **rvp & 0377;

				if (nodetype != CANONHOST && nodetype != CANONUSER)
				{
					rvp++;
					continue;
				}

				*rvp++ = NULL;

				if (xpvp != NULL)
				{
					cataddr(xpvp, NULL, replac,
						&pvpbuf[sizeof pvpbuf] - replac,
						'\0');
					*++arg_rvp = replac;
					replac += strlen(replac) + 1;
					xpvp = NULL;
				}
				switch (nodetype)
				{
				  case CANONHOST:
					xpvp = rvp;
					break;

				  case CANONUSER:
					default_rvp = rvp;
					break;
				}
			}
			if (*rvp != NULL)
				*rvp++ = NULL;
			if (xpvp != NULL)
			{
				cataddr(xpvp, NULL, replac,
					&pvpbuf[sizeof pvpbuf] - replac, 
					'\0');
				*++arg_rvp = replac;
			}
			*++arg_rvp = NULL;

			/* save the remainder of the input string */
			trsize = (int) (avp - rvp + 1) * sizeof *rvp;
			bcopy((char *) rvp, (char *) pvpb1, trsize);

			/* look it up */
			cataddr(key_rvp, NULL, buf, sizeof buf, '\0');
			argvect[0] = buf;
			if (e->e_sendmode == SM_DEFER)
			{
				/* don't do any map lookups */
				if (tTd(60, 1))
					printf("map_lookup(%s, %s) => DEFERRED\n",
						mapname, buf);
				replac = NULL;
				rstat = EX_TEMPFAIL;
			}
			else if (map != NULL && bitset(MF_OPEN, map->s_map.map_mflags))
			{
				auto int stat = EX_OK;

				if (!bitset(MF_KEEPQUOTES, map->s_map.map_mflags))
					stripquotes(buf);

				/* XXX should try to auto-open the map here */

				if (tTd(60, 1))
					printf("map_lookup(%s, %s) => ",
						mapname, buf);
				replac = (*map->s_map.map_class->map_lookup)(&map->s_map,
						buf, argvect, &stat);
				if (tTd(60, 1))
					printf("%s (%d)\n",
						replac ? replac : "NOT FOUND",
						stat);

				/* should recover if stat == EX_TEMPFAIL */
				if (stat == EX_TEMPFAIL)
				{
					rstat = EX_TEMPFAIL;
					if (tTd(60, 1))
						printf("map_lookup(%s, %s) tempfail: errno=%d\n",
							mapname, buf, errno);
					if (e->e_message == NULL)
					{
						char mbuf[300];

						sprintf(mbuf, "%.80s map: lookup (%s): deferred",
							mapname,
							shortenstring(buf, 203));
						e->e_message = newstr(mbuf);
					}
				}
			}
			else
				replac = NULL;

			/* if no replacement, use default */
			if (replac == NULL && default_rvp != NULL)
			{
				/* create the default */
				cataddr(default_rvp, NULL, buf, sizeof buf, '\0');
				replac = buf;
			}

			if (replac == NULL)
			{
				xpvp = key_rvp;
			}
			else if (*replac == '\0')
			{
				/* null replacement */
				nullpvp[0] = NULL;
				xpvp = nullpvp;
			}
			else
			{
				/* scan the new replacement */
				xpvp = prescan(replac, '\0', pvpbuf,
					       sizeof pvpbuf, NULL, NULL);
				if (xpvp == NULL)
				{
					/* prescan already printed error */
					return EX_DATAERR;
				}
			}

			/* append it to the token list */
			for (avp = hbrvp; *xpvp != NULL; xpvp++)
			{
				*avp++ = newstr(*xpvp);
				if (avp >= &npvp[MAXATOM])
					goto toolong;
			}

			/* restore the old trailing information */
			for (xpvp = pvpb1; (*avp++ = *xpvp++) != NULL; )
				if (avp >= &npvp[MAXATOM])
					goto toolong;

			break;
		}

		/*
		**  Check for subroutine calls.
		*/

		if (*npvp != NULL && (**npvp & 0377) == CALLSUBR)
		{
			int stat;

			if (npvp[1] == NULL)
			{
				syserr("parseaddr: NULL subroutine call in ruleset %d, rule %d",
					ruleset, ruleno);
				*pvp = NULL;
			}
			else
			{
				int ruleset;
				STAB *s;

				bcopy((char *) &npvp[2], (char *) pvp,
					(int) (avp - npvp - 2) * sizeof *avp);
				if (tTd(21, 3))
					printf("-----callsubr %s\n", npvp[1]);
				ruleset = strtorwset(npvp[1], NULL, ST_FIND);
				stat = rewrite(pvp, ruleset, reclevel, e);
				if (rstat == EX_OK || stat == EX_TEMPFAIL)
					rstat = stat;
				if (*pvp != NULL && (**pvp & 0377) == CANONNET)
				rwr = NULL;
			}
		}
		else
		{
			bcopy((char *) npvp, (char *) pvp,
				(int) (avp - npvp) * sizeof *avp);
		}
		if (tTd(21, 4))
		{
			printf("rewritten as:");
			printav(pvp);
		}
	}

	if (OpMode == MD_TEST || tTd(21, 1))
	{
		printf("rewrite: ruleset %2d returns:", ruleset);
		printav(pvp);
	}

	return rstat;
}
/*
**  BUILDADDR -- build address from token vector.
**
**	Parameters:
**		tv -- token vector.
**		a -- pointer to address descriptor to fill.
**			If NULL, one will be allocated.
**		flags -- info regarding whether this is a sender or
**			a recipient.
**		e -- the current envelope.
**
**	Returns:
**		NULL if there was an error.
**		'a' otherwise.
**
**	Side Effects:
**		fills in 'a'
*/

struct errcodes
{
	char	*ec_name;		/* name of error code */
	int	ec_code;		/* numeric code */
} ErrorCodes[] =
{
	"usage",	EX_USAGE,
	"nouser",	EX_NOUSER,
	"nohost",	EX_NOHOST,
	"unavailable",	EX_UNAVAILABLE,
	"software",	EX_SOFTWARE,
	"tempfail",	EX_TEMPFAIL,
	"protocol",	EX_PROTOCOL,
#ifdef EX_CONFIG
	"config",	EX_CONFIG,
#endif
	NULL,		EX_UNAVAILABLE,
};

ADDRESS *
buildaddr(tv, a, flags, e)
	register char **tv;
	register ADDRESS *a;
	int flags;
	register ENVELOPE *e;
{
	struct mailer **mp;
	register struct mailer *m;
	register char *p;
	char *mname;
	char **hostp;
	char hbuf[MAXNAME + 1];
	static MAILER errormailer;
	static char *errorargv[] = { "ERROR", NULL };
	static char ubuf[MAXNAME + 1];

	if (tTd(24, 5))
	{
		printf("buildaddr, flags=%x, tv=", flags);
		printav(tv);
	}

	if (a == NULL)
		a = (ADDRESS *) xalloc(sizeof *a);
	bzero((char *) a, sizeof *a);

	/* set up default error return flags */
	a->q_flags |= QPINGONFAILURE|QPINGONDELAY;

	/* figure out what net/mailer to use */
	if (*tv == NULL || (**tv & 0377) != CANONNET)
	{
		syserr("554 buildaddr: no mailer in parsed address");
badaddr:
		a->q_flags |= QBADADDR;
		a->q_mailer = &errormailer;
		if (errormailer.m_name == NULL)
		{
			/* initialize the bogus mailer */
			errormailer.m_name = "*error*";
			errormailer.m_mailer = "ERROR";
			errormailer.m_argv = errorargv;
		}
		return a;
	}
	mname = *++tv;

	/* extract host and user portions */
	if ((**++tv & 0377) == CANONHOST)
		hostp = ++tv;
	else
		hostp = NULL;
	while (*tv != NULL && (**tv & 0377) != CANONUSER)
		tv++;
	if (*tv == NULL)
	{
		syserr("554 buildaddr: no user");
		goto badaddr;
	}
	if (tv == hostp)
		hostp = NULL;
	else if (hostp != NULL)
		cataddr(hostp, tv - 1, hbuf, sizeof hbuf, '\0');
	cataddr(++tv, NULL, ubuf, sizeof ubuf, ' ');

	/* save away the host name */
	if (strcasecmp(mname, "error") == 0)
	{
		if (hostp != NULL)
		{
			register struct errcodes *ep;

			if (strchr(hbuf, '.') != NULL)
			{
				a->q_status = newstr(hbuf);
				setstat(dsntoexitstat(hbuf));
			}
			else if (isascii(hbuf[0]) && isdigit(hbuf[0]))
			{
				setstat(atoi(hbuf));
			}
			else
			{
				for (ep = ErrorCodes; ep->ec_name != NULL; ep++)
					if (strcasecmp(ep->ec_name, hbuf) == 0)
						break;
				setstat(ep->ec_code);
			}
		}
		else
			setstat(EX_UNAVAILABLE);
		stripquotes(ubuf);
		if (isascii(ubuf[0]) && isdigit(ubuf[0]) &&
		    isascii(ubuf[1]) && isdigit(ubuf[1]) &&
		    isascii(ubuf[2]) && isdigit(ubuf[2]) &&
		    ubuf[3] == ' ')
		{
			char fmt[10];

			strncpy(fmt, ubuf, 3);
			strcpy(&fmt[3], " %s");
			usrerr(fmt, ubuf + 4);

			/*
			**  If this is a 4xx code and we aren't running
			**  SMTP on our input, bounce this message;
			**  otherwise it disappears without a trace.
			*/

			if (fmt[0] == '4' && OpMode != MD_SMTP &&
			    OpMode != MD_DAEMON)
			{
				e->e_flags |= EF_FATALERRS;
			}
		}
		else
		{
			usrerr("553 %s", ubuf);
		}
		goto badaddr;
	}

	for (mp = Mailer; (m = *mp++) != NULL; )
	{
		if (strcasecmp(m->m_name, mname) == 0)
			break;
	}
	if (m == NULL)
	{
		syserr("554 buildaddr: unknown mailer %s", mname);
		goto badaddr;
	}
	a->q_mailer = m;

	/* figure out what host (if any) */
	if (hostp == NULL)
	{
		if (!bitnset(M_LOCALMAILER, m->m_flags))
		{
			syserr("554 buildaddr: no host");
			goto badaddr;
		}
		a->q_host = NULL;
	}
	else
		a->q_host = newstr(hbuf);

	/* figure out the user */
	p = ubuf;
	if (bitnset(M_CHECKUDB, m->m_flags) && *p == '@')
	{
		p++;
		tv++;
		a->q_flags |= QNOTREMOTE;
	}

	/* do special mapping for local mailer */
	if (*p == '"')
		p++;
	if (*p == '|' && bitnset(M_CHECKPROG, m->m_flags))
		a->q_mailer = m = ProgMailer;
	else if (*p == '/' && bitnset(M_CHECKFILE, m->m_flags))
		a->q_mailer = m = FileMailer;
	else if (*p == ':' && bitnset(M_CHECKINCLUDE, m->m_flags))
	{
		/* may be :include: */
		stripquotes(ubuf);
		if (strncasecmp(ubuf, ":include:", 9) == 0)
		{
			/* if :include:, don't need further rewriting */
			a->q_mailer = m = InclMailer;
			a->q_user = newstr(&ubuf[9]);
			return a;
		}
	}

	/* rewrite according recipient mailer rewriting rules */
	define('h', a->q_host, e);
	if (!bitset(RF_SENDERADDR|RF_HEADERADDR, flags))
	{
		/* sender addresses done later */
		(void) rewrite(tv, 2, 0, e);
		if (m->m_re_rwset > 0)
		       (void) rewrite(tv, m->m_re_rwset, 0, e);
	}
	(void) rewrite(tv, 4, 0, e);

	/* save the result for the command line/RCPT argument */
	cataddr(tv, NULL, ubuf, sizeof ubuf, '\0');
	a->q_user = ubuf;

	/*
	**  Do mapping to lower case as requested by mailer
	*/

	if (a->q_host != NULL && !bitnset(M_HST_UPPER, m->m_flags))
		makelower(a->q_host);
	if (!bitnset(M_USR_UPPER, m->m_flags))
		makelower(a->q_user);

	if (tTd(24, 6))
	{
		printf("buildaddr => ");
		printaddr(a, FALSE);
	}
	return a;
}
/*
**  CATADDR -- concatenate pieces of addresses (putting in <LWSP> subs)
**
**	Parameters:
**		pvp -- parameter vector to rebuild.
**		evp -- last parameter to include.  Can be NULL to
**			use entire pvp.
**		buf -- buffer to build the string into.
**		sz -- size of buf.
**		spacesub -- the space separator character; if null,
**			use SpaceSub.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Destroys buf.
*/

void
cataddr(pvp, evp, buf, sz, spacesub)
	char **pvp;
	char **evp;
	char *buf;
	register int sz;
	int spacesub;
{
	bool oatomtok = FALSE;
	bool natomtok = FALSE;
	register int i;
	register char *p;

	if (spacesub == '\0')
		spacesub = SpaceSub;

	if (pvp == NULL)
	{
		(void) strcpy(buf, "");
		return;
	}
	p = buf;
	sz -= 2;
	while (*pvp != NULL && (i = strlen(*pvp)) < sz)
	{
		natomtok = (TokTypeTab[**pvp & 0xff] == ATM);
		if (oatomtok && natomtok)
			*p++ = spacesub;
		(void) strcpy(p, *pvp);
		oatomtok = natomtok;
		p += i;
		sz -= i + 1;
		if (pvp++ == evp)
			break;
	}
	*p = '\0';
}
/*
**  SAMEADDR -- Determine if two addresses are the same
**
**	This is not just a straight comparison -- if the mailer doesn't
**	care about the host we just ignore it, etc.
**
**	Parameters:
**		a, b -- pointers to the internal forms to compare.
**
**	Returns:
**		TRUE -- they represent the same mailbox.
**		FALSE -- they don't.
**
**	Side Effects:
**		none.
*/

bool
sameaddr(a, b)
	register ADDRESS *a;
	register ADDRESS *b;
{
	register ADDRESS *ca, *cb;

	/* if they don't have the same mailer, forget it */
	if (a->q_mailer != b->q_mailer)
		return (FALSE);

	/* if the user isn't the same, we can drop out */
	if (strcmp(a->q_user, b->q_user) != 0)
		return (FALSE);

	/* if we have good uids for both but they differ, these are different */
	if (a->q_mailer == ProgMailer)
	{
		ca = getctladdr(a);
		cb = getctladdr(b);
		if (ca != NULL && cb != NULL &&
		    bitset(QGOODUID, ca->q_flags & cb->q_flags) &&
		    ca->q_uid != cb->q_uid)
			return (FALSE);
	}

	/* otherwise compare hosts (but be careful for NULL ptrs) */
	if (a->q_host == b->q_host)
	{
		/* probably both null pointers */
		return (TRUE);
	}
	if (a->q_host == NULL || b->q_host == NULL)
	{
		/* only one is a null pointer */
		return (FALSE);
	}
	if (strcmp(a->q_host, b->q_host) != 0)
		return (FALSE);

	return (TRUE);
}
/*
**  PRINTADDR -- print address (for debugging)
**
**	Parameters:
**		a -- the address to print
**		follow -- follow the q_next chain.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

struct qflags
{
	char	*qf_name;
	u_long	qf_bit;
};

struct qflags	AddressFlags[] =
{
	"QDONTSEND",		QDONTSEND,
	"QBADADDR",		QBADADDR,
	"QGOODUID",		QGOODUID,
	"QPRIMARY",		QPRIMARY,
	"QQUEUEUP",		QQUEUEUP,
	"QSENT",		QSENT,
	"QNOTREMOTE",		QNOTREMOTE,
	"QSELFREF",		QSELFREF,
	"QVERIFIED",		QVERIFIED,
	"QBOGUSSHELL",		QBOGUSSHELL,
	"QUNSAFEADDR",		QUNSAFEADDR,
	"QPINGONSUCCESS",	QPINGONSUCCESS,
	"QPINGONFAILURE",	QPINGONFAILURE,
	"QPINGONDELAY",		QPINGONDELAY,
	"QHASNOTIFY",		QHASNOTIFY,
	"QRELAYED",		QRELAYED,
	"QEXPANDED",		QEXPANDED,
	"QDELIVERED",		QDELIVERED,
	"QDELAYED",		QDELAYED,
	"QTHISPASS",		QTHISPASS,
	NULL
};

void
printaddr(a, follow)
	register ADDRESS *a;
	bool follow;
{
	register MAILER *m;
	MAILER pseudomailer;
	register struct qflags *qfp;
	bool firstone;

	if (a == NULL)
	{
		printf("[NULL]\n");
		return;
	}

	while (a != NULL)
	{
		printf("%x=", a);
		(void) fflush(stdout);

		/* find the mailer -- carefully */
		m = a->q_mailer;
		if (m == NULL)
		{
			m = &pseudomailer;
			m->m_mno = -1;
			m->m_name = "NULL";
		}

		printf("%s:\n\tmailer %d (%s), host `%s'\n",
		       a->q_paddr == NULL ? "<null>" : a->q_paddr,
		       m->m_mno, m->m_name,
		       a->q_host == NULL ? "<null>" : a->q_host);
		printf("\tuser `%s', ruser `%s'\n",
		       a->q_user,
		       a->q_ruser == NULL ? "<null>" : a->q_ruser);
		printf("\tnext=%x, alias %x, uid %d, gid %d\n",
		       a->q_next, a->q_alias, a->q_uid, a->q_gid);
		printf("\tflags=%lx<", a->q_flags);
		firstone = TRUE;
		for (qfp = AddressFlags; qfp->qf_name != NULL; qfp++)
		{
			if (!bitset(qfp->qf_bit, a->q_flags))
				continue;
			if (!firstone)
				printf(",");
			firstone = FALSE;
			printf("%s", qfp->qf_name);
		}
		printf(">\n");
		printf("\towner=%s, home=\"%s\", fullname=\"%s\"\n",
		       a->q_owner == NULL ? "(none)" : a->q_owner,
		       a->q_home == NULL ? "(none)" : a->q_home,
		       a->q_fullname == NULL ? "(none)" : a->q_fullname);
		printf("\torcpt=\"%s\", statmta=%s, rstatus=%s\n",
		       a->q_orcpt == NULL ? "(none)" : a->q_orcpt,
		       a->q_statmta == NULL ? "(none)" : a->q_statmta,
		       a->q_rstatus == NULL ? "(none)" : a->q_rstatus);

		if (!follow)
			return;
		a = a->q_next;
	}
}
/*
**  EMPTYADDR -- return TRUE if this address is empty (``<>'')
**
**	Parameters:
**		a -- pointer to the address
**
**	Returns:
**		TRUE -- if this address is "empty" (i.e., no one should
**			ever generate replies to it.
**		FALSE -- if it is a "regular" (read: replyable) address.
*/

bool
emptyaddr(a)
	register ADDRESS *a;
{
	return a->q_paddr == NULL || strcmp(a->q_paddr, "<>") == 0 ||
	       a->q_user == NULL || strcmp(a->q_user, "<>") == 0;
}
/*
**  REMOTENAME -- return the name relative to the current mailer
**
**	Parameters:
**		name -- the name to translate.
**		m -- the mailer that we want to do rewriting relative
**			to.
**		flags -- fine tune operations.
**		pstat -- pointer to status word.
**		e -- the current envelope.
**
**	Returns:
**		the text string representing this address relative to
**			the receiving mailer.
**
**	Side Effects:
**		none.
**
**	Warnings:
**		The text string returned is tucked away locally;
**			copy it if you intend to save it.
*/

char *
remotename(name, m, flags, pstat, e)
	char *name;
	struct mailer *m;
	int flags;
	int *pstat;
	register ENVELOPE *e;
{
	register char **pvp;
	char *fancy;
	char *oldg = macvalue('g', e);
	int rwset;
	static char buf[MAXNAME + 1];
	char lbuf[MAXNAME + 1];
	char pvpbuf[PSBUFSIZE];
	extern char *crackaddr();

	if (tTd(12, 1))
		printf("remotename(%s)\n", name);

	/* don't do anything if we are tagging it as special */
	if (bitset(RF_SENDERADDR, flags))
		rwset = bitset(RF_HEADERADDR, flags) ? m->m_sh_rwset
						     : m->m_se_rwset;
	else
		rwset = bitset(RF_HEADERADDR, flags) ? m->m_rh_rwset
						     : m->m_re_rwset;
	if (rwset < 0)
		return (name);

	/*
	**  Do a heuristic crack of this name to extract any comment info.
	**	This will leave the name as a comment and a $g macro.
	*/

	if (bitset(RF_CANONICAL, flags) || bitnset(M_NOCOMMENT, m->m_flags))
		fancy = "\201g";
	else
		fancy = crackaddr(name);

	/*
	**  Turn the name into canonical form.
	**	Normally this will be RFC 822 style, i.e., "user@domain".
	**	If this only resolves to "user", and the "C" flag is
	**	specified in the sending mailer, then the sender's
	**	domain will be appended.
	*/

	pvp = prescan(name, '\0', pvpbuf, sizeof pvpbuf, NULL, NULL);
	if (pvp == NULL)
		return (name);
	if (rewrite(pvp, 3, 0, e) == EX_TEMPFAIL)
		*pstat = EX_TEMPFAIL;
	if (bitset(RF_ADDDOMAIN, flags) && e->e_fromdomain != NULL)
	{
		/* append from domain to this address */
		register char **pxp = pvp;

		/* see if there is an "@domain" in the current name */
		while (*pxp != NULL && strcmp(*pxp, "@") != 0)
			pxp++;
		if (*pxp == NULL)
		{
			/* no.... append the "@domain" from the sender */
			register char **qxq = e->e_fromdomain;

			while ((*pxp++ = *qxq++) != NULL)
				continue;
			if (rewrite(pvp, 3, 0, e) == EX_TEMPFAIL)
				*pstat = EX_TEMPFAIL;
		}
	}

	/*
	**  Do more specific rewriting.
	**	Rewrite using ruleset 1 or 2 depending on whether this is
	**		a sender address or not.
	**	Then run it through any receiving-mailer-specific rulesets.
	*/

	if (bitset(RF_SENDERADDR, flags))
	{
		if (rewrite(pvp, 1, 0, e) == EX_TEMPFAIL)
			*pstat = EX_TEMPFAIL;
	}
	else
	{
		if (rewrite(pvp, 2, 0, e) == EX_TEMPFAIL)
			*pstat = EX_TEMPFAIL;
	}
	if (rwset > 0)
	{
		if (rewrite(pvp, rwset, 0, e) == EX_TEMPFAIL)
			*pstat = EX_TEMPFAIL;
	}

	/*
	**  Do any final sanitation the address may require.
	**	This will normally be used to turn internal forms
	**	(e.g., user@host.LOCAL) into external form.  This
	**	may be used as a default to the above rules.
	*/

	if (rewrite(pvp, 4, 0, e) == EX_TEMPFAIL)
		*pstat = EX_TEMPFAIL;

	/*
	**  Now restore the comment information we had at the beginning.
	*/

	cataddr(pvp, NULL, lbuf, sizeof lbuf, '\0');
	define('g', lbuf, e);

	/* need to make sure route-addrs have <angle brackets> */
	if (bitset(RF_CANONICAL, flags) && lbuf[0] == '@')
		expand("<\201g>", buf, sizeof buf, e);
	else
		expand(fancy, buf, sizeof buf, e);

	define('g', oldg, e);

	if (tTd(12, 1))
		printf("remotename => `%s'\n", buf);
	return (buf);
}
/*
**  MAPLOCALUSER -- run local username through ruleset 5 for final redirection
**
**	Parameters:
**		a -- the address to map (but just the user name part).
**		sendq -- the sendq in which to install any replacement
**			addresses.
**		aliaslevel -- the alias nesting depth.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

void
maplocaluser(a, sendq, aliaslevel, e)
	register ADDRESS *a;
	ADDRESS **sendq;
	int aliaslevel;
	ENVELOPE *e;
{
	register char **pvp;
	register ADDRESS *a1 = NULL;
	auto char *delimptr;
	char pvpbuf[PSBUFSIZE];

	if (tTd(29, 1))
	{
		printf("maplocaluser: ");
		printaddr(a, FALSE);
	}
	pvp = prescan(a->q_user, '\0', pvpbuf, sizeof pvpbuf, &delimptr, NULL);
	if (pvp == NULL)
		return;

	(void) rewrite(pvp, 5, 0, e);
	if (pvp[0] == NULL || (pvp[0][0] & 0377) != CANONNET)
		return;

	/* if non-null, mailer destination specified -- has it changed? */
	a1 = buildaddr(pvp, NULL, 0, e);
	if (a1 == NULL || sameaddr(a, a1))
		return;

	/* mark old address as dead; insert new address */
	a->q_flags |= QDONTSEND;
	if (tTd(29, 5))
	{
		printf("maplocaluser: QDONTSEND ");
		printaddr(a, FALSE);
	}
	a1->q_alias = a;
	allocaddr(a1, RF_COPYALL, a->q_paddr);
	(void) recipient(a1, sendq, aliaslevel, e);
}
/*
**  DEQUOTE_INIT -- initialize dequote map
**
**	This is a no-op.
**
**	Parameters:
**		map -- the internal map structure.
**		args -- arguments.
**
**	Returns:
**		TRUE.
*/

bool
dequote_init(map, args)
	MAP *map;
	char *args;
{
	register char *p = args;

	map->map_mflags |= MF_KEEPQUOTES;
	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'a':
			map->map_app = ++p;
			break;

		  case 's':
			map->map_coldelim = *++p;
			break;
		}
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p = '\0';
	}
	if (map->map_app != NULL)
		map->map_app = newstr(map->map_app);

	return TRUE;
}
/*
**  DEQUOTE_MAP -- unquote an address
**
**	Parameters:
**		map -- the internal map structure (ignored).
**		name -- the name to dequote.
**		av -- arguments (ignored).
**		statp -- pointer to status out-parameter.
**
**	Returns:
**		NULL -- if there were no quotes, or if the resulting
**			unquoted buffer would not be acceptable to prescan.
**		else -- The dequoted buffer.
*/

char *
dequote_map(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	register char *p;
	register char *q;
	register char c;
	int anglecnt = 0;
	int cmntcnt = 0;
	int quotecnt = 0;
	int spacecnt = 0;
	bool quotemode = FALSE;
	bool bslashmode = FALSE;
	char spacesub = map->map_coldelim;

	for (p = q = name; (c = *p++) != '\0'; )
	{
		if (bslashmode)
		{
			bslashmode = FALSE;
			*q++ = c;
			continue;
		}

		if (c == ' ' && spacesub != '\0')
			c = spacesub;

		switch (c)
		{
		  case '\\':
			bslashmode = TRUE;
			break;

		  case '(':
			cmntcnt++;
			break;

		  case ')':
			if (cmntcnt-- <= 0)
				return NULL;
			break;

		  case ' ':
			spacecnt++;
			break;
		}

		if (cmntcnt > 0)
		{
			*q++ = c;
			continue;
		}

		switch (c)
		{
		  case '"':
			quotemode = !quotemode;
			quotecnt++;
			continue;

		  case '<':
			anglecnt++;
			break;

		  case '>':
			if (anglecnt-- <= 0)
				return NULL;
			break;
		}
		*q++ = c;
	}

	if (anglecnt != 0 || cmntcnt != 0 || bslashmode ||
	    quotemode || quotecnt <= 0 || spacecnt != 0)
		return NULL;
	*q++ = '\0';
	return name;
}
