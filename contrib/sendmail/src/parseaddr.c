/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: parseaddr.c,v 8.234.4.1 2000/05/25 18:56:16 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>

static void	allocaddr __P((ADDRESS *, int, char *));
static int	callsubr __P((char**, int, ENVELOPE *));
static char	*map_lookup __P((STAB *, char *, char **, int *, ENVELOPE *));
static ADDRESS	*buildaddr __P((char **, ADDRESS *, int, ENVELOPE *));

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
#define DELIMCHARS	"()<>,;\r\n"	/* default word delimiters */

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
	bool qup;
	char pvpbuf[PSBUFSIZE];

	/*
	**  Initialize and prescan address.
	*/

	e->e_to = addr;
	if (tTd(20, 1))
		dprintf("\n--parseaddr(%s)\n", addr);

	if (delimptr == NULL)
		delimptr = &delimptrbuf;

	pvp = prescan(addr, delim, pvpbuf, sizeof pvpbuf, delimptr, NULL);
	if (pvp == NULL)
	{
		if (tTd(20, 1))
			dprintf("parseaddr-->NULL\n");
		return NULL;
	}

	if (invalidaddr(addr, delim == '\0' ? NULL : *delimptr))
	{
		if (tTd(20, 1))
			dprintf("parseaddr-->bad address\n");
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

	qup = FALSE;
	if (rewrite(pvp, 3, 0, e) == EX_TEMPFAIL)
		qup = TRUE;
	if (rewrite(pvp, 0, 0, e) == EX_TEMPFAIL)
		qup = TRUE;


	/*
	**  Build canonical address from pvp.
	*/

	a = buildaddr(pvp, a, flags, e);

	/*
	**  Make local copies of the host & user and then
	**  transport them out.
	*/

	allocaddr(a, flags, addr);
	if (QS_IS_BADADDR(a->q_state))
		return a;

	/*
	**  If there was a parsing failure, mark it for queueing.
	*/

	if (qup && OpMode != MD_INITALIAS)
	{
		char *msg = "Transient parse error -- message queued for future delivery";

		if (e->e_sendmode == SM_DEFER)
			msg = "Deferring message until queue run";
		if (tTd(20, 1))
			dprintf("parseaddr: queuing message\n");
		message(msg);
		if (e->e_message == NULL && e->e_sendmode != SM_DEFER)
			e->e_message = newstr(msg);
		a->q_state = QS_QUEUEUP;
		a->q_status = "4.4.3";
	}

	/*
	**  Compute return value.
	*/

	if (tTd(20, 1))
	{
		dprintf("parseaddr-->");
		printaddr(a, FALSE);
	}

	return a;
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
	if (strlen(addr) > MAXNAME - 1)
	{
		usrerr("553 5.1.1 Address too long (%d bytes max)",
		       MAXNAME - 1);
		goto failure;
	}
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
	usrerr("553 5.1.1 Address contained invalid control characters");
failure:
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

static void
allocaddr(a, flags, paddr)
	register ADDRESS *a;
	int flags;
	char *paddr;
{
	if (tTd(24, 4))
		dprintf("allocaddr(flags=%x, paddr=%s)\n", flags, paddr);

	a->q_paddr = paddr;

	if (a->q_user == NULL)
		a->q_user = newstr("");
	if (a->q_host == NULL)
		a->q_host = newstr("");

	if (bitset(RF_COPYPARSE, flags))
	{
		a->q_host = newstr(a->q_host);
		if (a->q_user != a->q_paddr)
			a->q_user = newstr(a->q_user);
	}

	if (a->q_paddr == NULL)
		a->q_paddr = newstr(a->q_user);
}
/*
**  PRESCAN -- Prescan name and make it canonical
**
**	Scans a name and turns it into a set of tokens.  This process
**	deletes blanks and comments (in parentheses) (if the token type
**	for left paren is SPC).
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
#define OPR		0	/* operator */
#define ATM		1	/* atom */
#define QST		2	/* in quoted string */
#define SPC		3	/* chewing up spaces */
#define ONE		4	/* pick up one character */
#define ILL		5	/* illegal character */

#define NSTATES	6	/* number of states */
#define TYPE		017	/* mask to select state type */

/* meta bits for table */
#define M		020	/* meta character; don't pass through */
#define B		040	/* cause a break */
#define MB		M|B	/* meta-break */

static short StateTab[NSTATES][NSTATES] =
{
   /*	oldst	chtype>	OPR	ATM	QST	SPC	ONE	ILL	*/
	/*OPR*/	{	OPR|B,	ATM|B,	QST|B,	SPC|MB,	ONE|B,	ILL|MB	},
	/*ATM*/	{	OPR|B,	ATM,	QST|B,	SPC|MB,	ONE|B,	ILL|MB	},
	/*QST*/	{	QST,	QST,	OPR,	QST,	QST,	QST	},
	/*SPC*/	{	OPR,	ATM,	QST,	SPC|M,	ONE,	ILL|MB	},
	/*ONE*/	{	OPR,	OPR,	OPR,	OPR,	OPR,	ILL|MB	},
	/*ILL*/	{	OPR|B,	ATM|B,	QST|B,	SPC|MB,	ONE|B,	ILL|M	},
};

/* token type table -- it gets modified with $o characters */
static u_char	TokTypeTab[256] =
{
    /*	nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si   */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,SPC,SPC,SPC,SPC,SPC,ATM,ATM,
    /*	dle dc1 dc2 dc3 dc4 nak syn etb  can em  sub esc fs  gs  rs  us   */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  sp  !   "   #   $   %   &   '    (   )   *   +   ,   -   .   /    */
	SPC,ATM,QST,ATM,ATM,ATM,ATM,ATM, SPC,SPC,ATM,ATM,ATM,ATM,ATM,ATM,
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
	SPC,ATM,QST,ATM,ATM,ATM,ATM,ATM, SPC,SPC,ATM,ATM,OPR,ATM,ATM,OPR,
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

/* token type table: don't strip comments */
u_char	TokTypeNoC[256] =
{
    /*	nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si   */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,SPC,SPC,SPC,SPC,SPC,ATM,ATM,
    /*	dle dc1 dc2 dc3 dc4 nak syn etb  can em  sub esc fs  gs  rs  us   */
	ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM, ATM,ATM,ATM,ATM,ATM,ATM,ATM,ATM,
    /*  sp  !   "   #   $   %   &   '    (   )   *   +   ,   -   .   /    */
	SPC,ATM,QST,ATM,ATM,ATM,ATM,ATM, OPR,OPR,ATM,ATM,ATM,ATM,ATM,ATM,
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


#define NOCHAR		-1	/* signal nothing in lookahead token */

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
	bool route_syntax;
	int cmntcnt;
	int anglecnt;
	char *tok;
	int state;
	int newstate;
	char *saveto = CurEnv->e_to;
	static char *av[MAXATOM + 1];
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
		expand(OperatorChars, obuf, sizeof obuf - sizeof DELIMCHARS,
		       CurEnv);
		(void) strlcat(obuf, DELIMCHARS, sizeof obuf);
		for (p = obuf; *p != '\0'; p++)
		{
			if (TokTypeTab[*p & 0xff] == ATM)
				TokTypeTab[*p & 0xff] = OPR;
			if (TokTypeNoC[*p & 0xff] == ATM)
				TokTypeNoC[*p & 0xff] = OPR;
		}
	}
	if (toktab == NULL)
		toktab = TokTypeTab;

	/* make sure error messages don't have garbage on them */
	errno = 0;

	q = pvpbuf;
	bslashmode = FALSE;
	route_syntax = FALSE;
	cmntcnt = 0;
	anglecnt = 0;
	avp = av;
	state = ATM;
	c = NOCHAR;
	p = addr;
	CurEnv->e_to = p;
	if (tTd(22, 11))
	{
		dprintf("prescan: ");
		xputs(p);
		dprintf("\n");
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
					usrerr("553 5.1.1 Address too long");
					if (strlen(addr) > (SIZE_T) MAXNAME)
						addr[MAXNAME] = '\0';
	returnnull:
					if (delimptr != NULL)
						*delimptr = p;
					CurEnv->e_to = saveto;
					return NULL;
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
			else if (c == delim && cmntcnt <= 0 && state != QST)
			{
				if (anglecnt <= 0)
					break;

				/* special case for better error management */
				if (delim == ',' && !route_syntax)
				{
					usrerr("653 Unbalanced '<'");
					c = '>';
					p--;
				}
			}

			if (tTd(22, 101))
				dprintf("c=%c, s=%d; ", c, state);

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
				/* EMPTY */
				/* do nothing, just avoid next clauses */
			}
			else if (c == '(' && toktab['('] == SPC)
			{
				cmntcnt++;
				c = NOCHAR;
			}
			else if (c == ')' && toktab['('] == SPC)
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
			{
				c = NOCHAR;
			}
			else if (c == '<')
			{
				char *ptr = p;

				anglecnt++;
				while (isascii(*ptr) && isspace(*ptr))
					ptr++;
				if (*ptr == '@')
					route_syntax = TRUE;
			}
			else if (c == '>')
			{
				if (anglecnt <= 0)
				{
					usrerr("653 Unbalanced '>'");
					c = NOCHAR;
				}
				else
					anglecnt--;
				route_syntax = FALSE;
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
				dprintf("ns=%02o\n", newstate);
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
				dprintf("tok=");
				xputs(tok);
				dprintf("\n");
			}
			if (avp >= &av[MAXATOM])
			{
				usrerr("553 5.1.0 prescan: too many tokens");
				goto returnnull;
			}
			if (q - tok > MAXNAME)
			{
				usrerr("553 5.1.0 prescan: token too long");
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
		dprintf("prescan==>");
		printav(av);
	}
	CurEnv->e_to = saveto;
	if (av[0] == NULL)
	{
		if (tTd(22, 1))
			dprintf("prescan: null leading token\n");
		return NULL;
	}
	return av;
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
	char	**match_first;		/* first token matched */
	char	**match_last;		/* last token matched */
	char	**match_pattern;	/* pointer to pattern */
};

#define MAXMATCH	9	/* max params per rewrite */


int
rewrite(pvp, ruleset, reclevel, e)
	char **pvp;
	int ruleset;
	int reclevel;
	register ENVELOPE *e;
{
	register char *ap;		/* address pointer */
	register char *rp;		/* rewrite pointer */
	register char *rulename;	/* ruleset name */
	register char *prefix;
	register char **avp;		/* address vector pointer */
	register char **rvp;		/* rewrite vector pointer */
	register struct match *mlp;	/* cur ptr into mlist */
	register struct rewrite *rwr;	/* pointer to current rewrite rule */
	int ruleno;			/* current rule number */
	int rstat = EX_OK;		/* return status */
	int loopcount;
	struct match mlist[MAXMATCH];	/* stores match on LHS */
	char *npvp[MAXATOM + 1];	/* temporary space for rebuild */
	char buf[MAXLINE];
	char name[6];

	if (ruleset < 0 || ruleset >= MAXRWSETS)
	{
		syserr("554 5.3.5 rewrite: illegal ruleset number %d", ruleset);
		return EX_CONFIG;
	}
	rulename = RuleSetNames[ruleset];
	if (rulename == NULL)
	{
		snprintf(name, sizeof name, "%d", ruleset);
		rulename = name;
	}
	if (OpMode == MD_TEST)
		prefix = "";
	else
		prefix = "rewrite: ruleset ";
	if (OpMode == MD_TEST)
	{
		printf("%s%-16.16s   input:", prefix, rulename);
		printav(pvp);
	}
	else if (tTd(21, 1))
	{
		dprintf("%s%-16.16s   input:", prefix, rulename);
		printav(pvp);
	}
	if (reclevel++ > MaxRuleRecursion)
	{
		syserr("rewrite: excessive recursion (max %d), ruleset %s",
			MaxRuleRecursion, rulename);
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
		int status;

		/* if already canonical, quit now */
		if (pvp[0] != NULL && (pvp[0][0] & 0377) == CANONNET)
			break;

		if (tTd(21, 12))
		{
			if (tTd(21, 15))
				dprintf("-----trying rule (line %d):",
				       rwr->r_line);
			else
				dprintf("-----trying rule:");
			printav(rwr->r_lhs);
		}

		/* try to match on this rule */
		mlp = mlist;
		rvp = rwr->r_lhs;
		avp = pvp;
		if (++loopcount > 100)
		{
			syserr("554 5.3.5 Infinite loop in ruleset %s, rule %d",
				rulename, ruleno);
			if (tTd(21, 1))
			{
				dprintf("workspace: ");
				printav(pvp);
			}
			break;
		}

		while ((ap = *avp) != NULL || *rvp != NULL)
		{
			rp = *rvp;
			if (tTd(21, 35))
			{
				dprintf("ADVANCE rp=");
				xputs(rp);
				dprintf(", ap=");
				xputs(ap);
				dprintf("\n");
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
			  case MATCHCLASS:
				/* match any phrase in a class */
				mlp->match_pattern = rvp;
				mlp->match_first = avp;
	extendclass:
				ap = *avp;
				if (ap == NULL)
					goto backup;
				mlp->match_last = avp++;
				cataddr(mlp->match_first, mlp->match_last,
					buf, sizeof buf, '\0');
				if (!wordinclass(buf, rp[1]))
				{
					if (tTd(21, 36))
					{
						dprintf("EXTEND  rp=");
						xputs(rp);
						dprintf(", ap=");
						xputs(ap);
						dprintf("\n");
					}
					goto extendclass;
				}
				if (tTd(21, 36))
					dprintf("CLMATCH\n");
				mlp++;
				break;

			  case MATCHNCLASS:
				/* match any token not in a class */
				if (wordinclass(ap, rp[1]))
					goto backup;

				/* FALLTHROUGH */

			  case MATCHONE:
			  case MATCHANY:
				/* match exactly one token */
				mlp->match_pattern = rvp;
				mlp->match_first = avp;
				mlp->match_last = avp++;
				mlp++;
				break;

			  case MATCHZANY:
				/* match zero or more tokens */
				mlp->match_pattern = rvp;
				mlp->match_first = avp;
				mlp->match_last = avp - 1;
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
				mlp->match_first = avp;
				if (tTd(21, 2))
					dprintf("rewrite: LHS $&%s => \"%s\"\n",
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
						avp = mlp->match_first;
						goto backup;
					}
					ap += strlen(*avp++);
				}

				/* match */
				break;

			  default:
				/* must have exact match */
				if (sm_strcasecmp(rp, ap))
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
				rvp = mlp->match_pattern;
				rp = *rvp;
				avp = mlp->match_last + 1;
				ap = *avp;

				if (tTd(21, 36))
				{
					dprintf("BACKUP  rp=");
					xputs(rp);
					dprintf(", ap=");
					xputs(ap);
					dprintf("\n");
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
					mlp->match_last = avp++;
					rvp++;
					mlp++;
					break;
				}
				if ((*rp & 0377) == MATCHCLASS)
				{
					/* extend binding and try again */
					mlp->match_last = avp;
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
				dprintf("----- rule fails\n");
			rwr = rwr->r_next;
			ruleno++;
			loopcount = 0;
			continue;
		}

		rvp = rwr->r_rhs;
		if (tTd(21, 12))
		{
			dprintf("-----rule matches:");
			printav(rvp);
		}

		rp = *rvp;
		if (rp != NULL)
		{
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
		}

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
					syserr("554 5.3.5 rewrite: ruleset %s: replacement $%c out of bounds",
						rulename, rp[1]);
					return EX_CONFIG;
				}
				if (tTd(21, 15))
				{
					dprintf("$%c:", rp[1]);
					pp = m->match_first;
					while (pp <= m->match_last)
					{
						dprintf(" %lx=\"",
							(u_long) *pp);
						(void) dflush();
						dprintf("%s\"", *pp++);
					}
					dprintf("\n");
				}
				pp = m->match_first;
				while (pp <= m->match_last)
				{
					if (avp >= &npvp[MAXATOM])
					{
						syserr("554 5.3.0 rewrite: expansion too long");
						return EX_DATAERR;
					}
					*avp++ = *pp++;
				}
			}
			else
			{
				/* some sort of replacement */
				if (avp >= &npvp[MAXATOM])
				{
	toolong:
					syserr("554 5.3.0 rewrite: expansion too long");
					return EX_DATAERR;
				}
				if ((*rp & 0377) != MACRODEXPAND)
				{
					/* vanilla replacement */
					*avp++ = rp;
				}
				else
				{
					/* $&x replacement */
					char *mval = macvalue(rp[1], e);
					char **xpvp;
					int trsize = 0;
					static size_t pvpb1_size = 0;
					static char **pvpb1 = NULL;
					char pvpbuf[PSBUFSIZE];

					if (tTd(21, 2))
						dprintf("rewrite: RHS $&%s => \"%s\"\n",
							macname(rp[1]),
							mval == NULL ? "(NULL)" : mval);
					if (mval == NULL || *mval == '\0')
						continue;

					/* save the remainder of the input */
					for (xpvp = pvp; *xpvp != NULL; xpvp++)
						trsize += sizeof *xpvp;
					if ((size_t) trsize > pvpb1_size)
					{
						if (pvpb1 != NULL)
							free(pvpb1);
						pvpb1 = (char **)xalloc(trsize);
						pvpb1_size = trsize;
					}

					memmove((char *) pvpb1,
						(char *) pvp,
						trsize);

					/* scan the new replacement */
					xpvp = prescan(mval, '\0', pvpbuf,
						       sizeof pvpbuf, NULL,
						       NULL);
					if (xpvp == NULL)
					{
						/* prescan pre-printed error */
						return EX_DATAERR;
					}

					/* insert it into the output stream */
					while (*xpvp != NULL)
					{
						if (tTd(21, 19))
							dprintf(" ... %s\n",
								*xpvp);
						*avp++ = newstr(*xpvp);
						if (avp >= &npvp[MAXATOM])
							goto toolong;
						xpvp++;
					}
					if (tTd(21, 19))
						dprintf(" ... DONE\n");

					/* restore the old trailing input */
					memmove((char *) pvp,
						(char *) pvpb1,
						trsize);
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
			char cbuf[MAXNAME + 1];
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
				syserr("554 5.3.0 rewrite: map %s not found", mapname);

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
			memmove((char *) pvpb1, (char *) rvp, trsize);

			/* look it up */
			cataddr(key_rvp, NULL, cbuf, sizeof cbuf,
				map == NULL ? '\0' : map->s_map.map_spacesub);
			argvect[0] = cbuf;
			replac = map_lookup(map, cbuf, argvect, &rstat, e);

			/* if no replacement, use default */
			if (replac == NULL && default_rvp != NULL)
			{
				/* create the default */
				cataddr(default_rvp, NULL, cbuf, sizeof cbuf, '\0');
				replac = cbuf;
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
			rvp = avp - 1;
			for (xpvp = pvpb1; (*avp++ = *xpvp++) != NULL; )
				if (avp >= &npvp[MAXATOM])
					goto toolong;
		}

		/*
		**  Check for subroutine calls.
		*/

		status = callsubr(npvp, reclevel, e);
		if (rstat == EX_OK || status == EX_TEMPFAIL)
			rstat = status;

		/* copy vector back into original space. */
		for (avp = npvp; *avp++ != NULL;)
			continue;
		memmove((char *) pvp, (char *) npvp,
		      (int) (avp - npvp) * sizeof *avp);

		if (tTd(21, 4))
		{
			dprintf("rewritten as:");
			printav(pvp);
		}
	}

	if (OpMode == MD_TEST)
	{
		printf("%s%-16.16s returns:", prefix, rulename);
		printav(pvp);
	}
	else if (tTd(21, 1))
	{
		dprintf("%s%-16.16s returns:", prefix, rulename);
		printav(pvp);
	}
	return rstat;
}
/*
**  CALLSUBR -- call subroutines in rewrite vector
**
**	Parameters:
**		pvp -- pointer to token vector.
**		reclevel -- the current recursion level.
**		e -- the current envelope.
**
**	Returns:
**		The status from the subroutine call.
**
**	Side Effects:
**		pvp is modified.
*/

static int
callsubr(pvp, reclevel, e)
	char **pvp;
	int reclevel;
	ENVELOPE *e;
{
	char **avp;
	char **rvp;
	register int i;
	int subr;
	int status;
	int rstat = EX_OK;
	char *tpvp[MAXATOM + 1];

	for (avp = pvp; *avp != NULL; avp++)
	{
		if ((**avp & 0377) == CALLSUBR && avp[1] != NULL)
		{
			stripquotes(avp[1]);
			subr = strtorwset(avp[1], NULL, ST_FIND);
			if (subr < 0)
			{
				syserr("Unknown ruleset %s", avp[1]);
				return EX_CONFIG;
			}

			if (tTd(21, 3))
				dprintf("-----callsubr %s (%d)\n",
					avp[1], subr);

			/*
			**  Take care of possible inner calls first.
			**  use a full size temporary buffer to avoid
			**  overflows in rewrite, but strip off the
			**  subroutine call.
			*/

			for (i = 2; avp[i] != NULL; i++)
				tpvp[i - 2] = avp[i];
			tpvp[i - 2] = NULL;

			status = callsubr(tpvp, reclevel, e);
			if (rstat == EX_OK || status == EX_TEMPFAIL)
				rstat = status;

			/*
			**  Now we need to call the ruleset specified for
			**  the subroutine. we can do this with the
			**  temporary buffer that we set up earlier,
			**  since it has all the data we want to rewrite.
			*/

			status = rewrite(tpvp, subr, reclevel, e);
			if (rstat == EX_OK || status == EX_TEMPFAIL)
				rstat = status;

			/*
			**  Find length of tpvp and current offset into
			**  pvp, if the total is greater than MAXATOM,
			**  then it would overflow the buffer if we copied
			**  it back in to pvp, in which case we throw a
			**  fit.
			*/

			for (rvp = tpvp; *rvp != NULL; rvp++)
				continue;
			if (((rvp - tpvp) + (avp - pvp)) > MAXATOM)
			{
				syserr("554 5.3.0 callsubr: expansion too long");
				return EX_DATAERR;
			}

			/*
			**  Now we can copy the rewritten code over
			**  the initial subroutine call in the buffer.
			*/

			for (i = 0; tpvp[i] != NULL; i++)
				avp[i] = tpvp[i];
			avp[i] = NULL;

			/*
			**  If we got this far, we've processed the left
			**  most subroutine, and recursively called ourselves
			**  to handle any other subroutines.  We're done.
			*/

			break;
		}
	}
	return rstat;
}
/*
**  MAP_LOOKUP -- do lookup in map
**
**	Parameters:
**		map -- the map to use for the lookup.
**		key -- the key to look up.
**		argvect -- arguments to pass to the map lookup.
**		pstat -- a pointer to an integer in which to store the
**			status from the lookup.
**		e -- the current envelope.
**
**	Returns:
**		The result of the lookup.
**		NULL -- if there was no data for the given key.
*/

static char *
map_lookup(smap, key, argvect, pstat, e)
	STAB *smap;
	char key[];
	char **argvect;
	int *pstat;
	ENVELOPE *e;
{
	auto int status = EX_OK;
	MAP *map;
	char *replac;

	if (smap == NULL)
		return NULL;

	map = &smap->s_map;
	DYNOPENMAP(map);

	if (e->e_sendmode == SM_DEFER &&
	    bitset(MF_DEFER, map->map_mflags))
	{
		/* don't do any map lookups */
		if (tTd(60, 1))
			dprintf("map_lookup(%s, %s) => DEFERRED\n",
				smap->s_name, key);
		*pstat = EX_TEMPFAIL;
		return NULL;
	}

	if (!bitset(MF_KEEPQUOTES, map->map_mflags))
		stripquotes(key);

	if (tTd(60, 1))
	{
		dprintf("map_lookup(%s, %s", smap->s_name, key);
		if (tTd(60, 5))
		{
			int i;

			for (i = 0; argvect[i] != NULL; i++)
				dprintf(", %%%d=%s", i, argvect[i]);
		}
		dprintf(") => ");
	}
	replac = (*map->map_class->map_lookup)(map, key, argvect, &status);
	if (tTd(60, 1))
		dprintf("%s (%d)\n",
			replac != NULL ? replac : "NOT FOUND",
			status);

	/* should recover if status == EX_TEMPFAIL */
	if (status == EX_TEMPFAIL && !bitset(MF_NODEFER, map->map_mflags))
	{
		*pstat = EX_TEMPFAIL;
		if (tTd(60, 1))
			dprintf("map_lookup(%s, %s) tempfail: errno=%d\n",
				smap->s_name, key, errno);
		if (e->e_message == NULL)
		{
			char mbuf[320];

			snprintf(mbuf, sizeof mbuf,
				"%.80s map: lookup (%s): deferred",
				smap->s_name,
				shortenstring(key, MAXSHORTSTR));
			e->e_message = newstr(mbuf);
		}
	}
	if (status == EX_TEMPFAIL && map->map_tapp != NULL)
	{
		size_t i = strlen(key) + strlen(map->map_tapp) + 1;
		static char *rwbuf = NULL;
		static size_t rwbuflen = 0;

		if (i > rwbuflen)
		{
			if (rwbuf != NULL)
				free(rwbuf);
			rwbuflen = i;
			rwbuf = (char *) xalloc(rwbuflen);
		}
		snprintf(rwbuf, rwbuflen, "%s%s", key, map->map_tapp);
		if (tTd(60, 4))
			dprintf("map_lookup tempfail: returning \"%s\"\n",
				rwbuf);
		return rwbuf;
	}
	return replac;
}
/*
**  INITERRMAILERS -- initialize error and discard mailers
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		initializes error and discard mailers.
*/

static MAILER discardmailer;
static MAILER errormailer;
static char *discardargv[] = { "DISCARD", NULL };
static char *errorargv[] = { "ERROR", NULL };

void
initerrmailers()
{
	if (discardmailer.m_name == NULL)
	{
		/* initialize the discard mailer */
		discardmailer.m_name = "*discard*";
		discardmailer.m_mailer = "DISCARD";
		discardmailer.m_argv = discardargv;
	}
	if (errormailer.m_name == NULL)
	{
		/* initialize the bogus mailer */
		errormailer.m_name = "*error*";
		errormailer.m_mailer = "ERROR";
		errormailer.m_argv = errorargv;
	}
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

static struct errcodes
{
	char	*ec_name;		/* name of error code */
	int	ec_code;		/* numeric code */
} ErrorCodes[] =
{
	{ "usage",		EX_USAGE	},
	{ "nouser",		EX_NOUSER	},
	{ "nohost",		EX_NOHOST	},
	{ "unavailable",	EX_UNAVAILABLE	},
	{ "software",		EX_SOFTWARE	},
	{ "tempfail",		EX_TEMPFAIL	},
	{ "protocol",		EX_PROTOCOL	},
#ifdef EX_CONFIG
	{ "config",		EX_CONFIG	},
#endif /* EX_CONFIG */
	{ NULL,			EX_UNAVAILABLE	}
};


static ADDRESS *
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
	static char ubuf[MAXNAME + 2];

	if (tTd(24, 5))
	{
		dprintf("buildaddr, flags=%x, tv=", flags);
		printav(tv);
	}

	if (a == NULL)
		a = (ADDRESS *) xalloc(sizeof *a);
	memset((char *) a, '\0', sizeof *a);
	hbuf[0] = '\0';

	/* set up default error return flags */
	a->q_flags |= DefaultNotify;

	/* figure out what net/mailer to use */
	if (*tv == NULL || (**tv & 0377) != CANONNET)
	{
		syserr("554 5.3.5 buildaddr: no mailer in parsed address");
badaddr:
		if (ExitStat == EX_TEMPFAIL)
			a->q_state = QS_QUEUEUP;
		else
		{
			a->q_state = QS_BADADDR;
			a->q_mailer = &errormailer;
		}
		return a;
	}
	mname = *++tv;

	/* extract host and user portions */
	if (*++tv != NULL && (**tv & 0377) == CANONHOST)
		hostp = ++tv;
	else
		hostp = NULL;
	while (*tv != NULL && (**tv & 0377) != CANONUSER)
		tv++;
	if (*tv == NULL)
	{
		syserr("554 5.3.5 buildaddr: no user");
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
		/* Set up triplet for use by -bv */
		a->q_mailer = &errormailer;
		a->q_user = newstr(ubuf);

		if (hostp != NULL)
		{
			register struct errcodes *ep;

			a->q_host = newstr(hbuf);
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
		{
			a->q_host = NULL;
			setstat(EX_UNAVAILABLE);
		}
		stripquotes(ubuf);
		if (ISSMTPCODE(ubuf) && ubuf[3] == ' ')
		{
			char fmt[16];
			int off;

			if ((off = isenhsc(ubuf + 4, ' ')) > 0)
			{
				ubuf[off + 4] = '\0';
				off += 5;
			}
			else
			{
				off = 4;
				ubuf[3] = '\0';
			}
			(void) snprintf(fmt, sizeof fmt, "%s %%s", ubuf);
			if (off > 4)
				usrerr(fmt, ubuf + off);
			else if (isenhsc(hbuf, '\0') > 0)
				usrerrenh(hbuf, fmt, ubuf + off);
			else
				usrerr(fmt, ubuf + off);
			/* XXX ubuf[off - 1] = ' '; */
		}
		else
		{
			usrerr("553 5.3.0 %s", ubuf);
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
		syserr("554 5.3.5 buildaddr: unknown mailer %s", mname);
		goto badaddr;
	}
	a->q_mailer = m;

	/* figure out what host (if any) */
	if (hostp == NULL)
	{
		if (!bitnset(M_LOCALMAILER, m->m_flags))
		{
			syserr("554 5.3.5 buildaddr: no host");
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

#if _FFR_ADDR_TYPE
	/*
	**  Note, change the 9 to a 10 before removing #if FFR check
	**  in a future version.
	*/

	if (ConfigLevel >= 9 ||
	    !bitset(RF_SENDERADDR|RF_HEADERADDR, flags))
#else /* _FFR_ADDR_TYPE */
	if (!bitset(RF_SENDERADDR|RF_HEADERADDR, flags))
#endif /* _FFR_ADDR_TYPE */
	{
		/* sender addresses done later */
		(void) rewrite(tv, 2, 0, e);
		if (m->m_re_rwset > 0)
		       (void) rewrite(tv, m->m_re_rwset, 0, e);
	}
	(void) rewrite(tv, 4, 0, e);

	/* save the result for the command line/RCPT argument */
	cataddr(tv, NULL, ubuf, sizeof ubuf, '\0');
	a->q_user = newstr(ubuf);

	/*
	**  Do mapping to lower case as requested by mailer
	*/

	if (a->q_host != NULL && !bitnset(M_HST_UPPER, m->m_flags))
		makelower(a->q_host);
	if (!bitnset(M_USR_UPPER, m->m_flags))
		makelower(a->q_user);

	if (tTd(24, 6))
	{
		dprintf("buildaddr => ");
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

	if (sz <= 0)
		return;

	if (spacesub == '\0')
		spacesub = SpaceSub;

	if (pvp == NULL)
	{
		*buf = '\0';
		return;
	}
	p = buf;
	sz -= 2;
	while (*pvp != NULL && (i = strlen(*pvp)) < sz - 1)
	{
		natomtok = (TokTypeTab[**pvp & 0xff] == ATM);
		if (oatomtok && natomtok)
		{
			*p++ = spacesub;
			--sz;
		}
		(void) strlcpy(p, *pvp, sz);
		oatomtok = natomtok;
		p += i;
		sz -= i;
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
		return FALSE;

	/* if the user isn't the same, we can drop out */
	if (strcmp(a->q_user, b->q_user) != 0)
		return FALSE;

	/* if we have good uids for both but they differ, these are different */
	if (a->q_mailer == ProgMailer)
	{
		ca = getctladdr(a);
		cb = getctladdr(b);
		if (ca != NULL && cb != NULL &&
		    bitset(QGOODUID, ca->q_flags & cb->q_flags) &&
		    ca->q_uid != cb->q_uid)
			return FALSE;
	}

	/* otherwise compare hosts (but be careful for NULL ptrs) */
	if (a->q_host == b->q_host)
	{
		/* probably both null pointers */
		return TRUE;
	}
	if (a->q_host == NULL || b->q_host == NULL)
	{
		/* only one is a null pointer */
		return FALSE;
	}
	if (strcmp(a->q_host, b->q_host) != 0)
		return FALSE;

	return TRUE;
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

static struct qflags	AddressFlags[] =
{
	{ "QGOODUID",		QGOODUID	},
	{ "QPRIMARY",		QPRIMARY	},
	{ "QNOTREMOTE",		QNOTREMOTE	},
	{ "QSELFREF",		QSELFREF	},
	{ "QBOGUSSHELL",	QBOGUSSHELL	},
	{ "QUNSAFEADDR",	QUNSAFEADDR	},
	{ "QPINGONSUCCESS",	QPINGONSUCCESS	},
	{ "QPINGONFAILURE",	QPINGONFAILURE	},
	{ "QPINGONDELAY",	QPINGONDELAY	},
	{ "QHASNOTIFY",		QHASNOTIFY	},
	{ "QRELAYED",		QRELAYED	},
	{ "QEXPANDED",		QEXPANDED	},
	{ "QDELIVERED",		QDELIVERED	},
	{ "QDELAYED",		QDELAYED	},
	{ "QTHISPASS",		QTHISPASS	},
	{ "QRCPTOK",		QRCPTOK		},
	{ NULL }
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
		printf("%lx=", (u_long) a);
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
		printf("\tstate=");
		switch (a->q_state)
		{
		  case QS_OK:
			printf("OK");
			break;

		  case QS_DONTSEND:
			printf("DONTSEND");
			break;

		  case QS_BADADDR:
			printf("BADADDR");
			break;

		  case QS_QUEUEUP:
			printf("QUEUEUP");
			break;

		  case QS_SENT:
			printf("SENT");
			break;

		  case QS_VERIFIED:
			printf("VERIFIED");
			break;

		  case QS_EXPANDED:
			printf("EXPANDED");
			break;

		  case QS_SENDER:
			printf("SENDER");
			break;

		  case QS_CLONED:
			printf("CLONED");
			break;

		  case QS_DISCARDED:
			printf("DISCARDED");
			break;

		  case QS_REPLACED:
			printf("REPLACED");
			break;

		  case QS_REMOVED:
			printf("REMOVED");
			break;

		  case QS_DUPLICATE:
			printf("DUPLICATE");
			break;

		  case QS_INCLUDED:
			printf("INCLUDED");
			break;

		  default:
			printf("%d", a->q_state);
			break;
		}
		printf(", next=%lx, alias %lx, uid %d, gid %d\n",
		       (u_long) a->q_next, (u_long) a->q_alias,
		       (int) a->q_uid, (int) a->q_gid);
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
		printf("\torcpt=\"%s\", statmta=%s, status=%s\n",
		       a->q_orcpt == NULL ? "(none)" : a->q_orcpt,
		       a->q_statmta == NULL ? "(none)" : a->q_statmta,
		       a->q_status == NULL ? "(none)" : a->q_status);
		printf("\trstatus=\"%s\"\n",
		       a->q_rstatus == NULL ? "(none)" : a->q_rstatus);
		printf("\tspecificity=%d, statdate=%s\n",
		       a->q_specificity,
		       a->q_statdate == 0 ? "(none)" : ctime(&a->q_statdate));

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
#if _FFR_ADDR_TYPE
	char addrtype[4];
#endif /* _FFR_ADDR_TYPE */

	if (tTd(12, 1))
		dprintf("remotename(%s)\n", name);

	/* don't do anything if we are tagging it as special */
	if (bitset(RF_SENDERADDR, flags))
	{
		rwset = bitset(RF_HEADERADDR, flags) ? m->m_sh_rwset
						     : m->m_se_rwset;
#if _FFR_ADDR_TYPE
		addrtype[2] = 's';
#endif /* _FFR_ADDR_TYPE */
	}
	else
	{
		rwset = bitset(RF_HEADERADDR, flags) ? m->m_rh_rwset
						     : m->m_re_rwset;
#if _FFR_ADDR_TYPE
		addrtype[2] = 'r';
#endif /* _FFR_ADDR_TYPE */
	}
	if (rwset < 0)
		return name;
#if _FFR_ADDR_TYPE
	addrtype[1] = ' ';
	addrtype[3] = '\0';
	addrtype[0] = bitset(RF_HEADERADDR, flags) ? 'h' : 'e';
	define(macid("{addr_type}", NULL), addrtype, e);
#endif /* _FFR_ADDR_TYPE */

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
		return name;
	if (rewrite(pvp, 3, 0, e) == EX_TEMPFAIL)
		*pstat = EX_TEMPFAIL;
	if (bitset(RF_ADDDOMAIN, flags) && e->e_fromdomain != NULL)
	{
		/* append from domain to this address */
		register char **pxp = pvp;
		int l = MAXATOM;	/* size of buffer for pvp */

		/* see if there is an "@domain" in the current name */
		while (*pxp != NULL && strcmp(*pxp, "@") != 0)
		{
			pxp++;
			--l;
		}
		if (*pxp == NULL)
		{
			/* no.... append the "@domain" from the sender */
			register char **qxq = e->e_fromdomain;

			while ((*pxp++ = *qxq++) != NULL)
			{
				if (--l <= 0)
				{
					*--pxp = NULL;
					usrerr("553 5.1.0 remotename: too many tokens");
					*pstat = EX_UNAVAILABLE;
					break;
				}
			}
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
		dprintf("remotename => `%s'\n", buf);
	return buf;
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

#define Q_COPYFLAGS	(QPRIMARY|QBOGUSSHELL|QUNSAFEADDR|\
			 Q_PINGFLAGS|QHASNOTIFY|\
			 QRELAYED|QEXPANDED|QDELIVERED|QDELAYED)

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
		dprintf("maplocaluser: ");
		printaddr(a, FALSE);
	}
	pvp = prescan(a->q_user, '\0', pvpbuf, sizeof pvpbuf, &delimptr, NULL);
	if (pvp == NULL)
	{
		if (tTd(29, 9))
			dprintf("maplocaluser: cannot prescan %s\n",
				a->q_user);
		return;
	}

	define('h', a->q_host, e);
	define('u', a->q_user, e);
	define('z', a->q_home, e);

#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), "e r", e);
#endif /* _FFR_ADDR_TYPE */
	if (rewrite(pvp, 5, 0, e) == EX_TEMPFAIL)
	{
		if (tTd(29, 9))
			dprintf("maplocaluser: rewrite tempfail\n");
		a->q_state = QS_QUEUEUP;
		a->q_status = "4.4.3";
		return;
	}
	if (pvp[0] == NULL || (pvp[0][0] & 0377) != CANONNET)
	{
		if (tTd(29, 9))
			dprintf("maplocaluser: doesn't resolve\n");
		return;
	}

	/* if non-null, mailer destination specified -- has it changed? */
	a1 = buildaddr(pvp, NULL, 0, e);
	if (a1 == NULL || sameaddr(a, a1))
	{
		if (tTd(29, 9))
			dprintf("maplocaluser: address unchanged\n");
		if (a1 != NULL)
			free(a1);
		return;
	}

	/* make new address take on flags and print attributes of old */
	a1->q_flags &= ~Q_COPYFLAGS;
	a1->q_flags |= a->q_flags & Q_COPYFLAGS;
	a1->q_paddr = newstr(a->q_paddr);
	a1->q_orcpt = a->q_orcpt;

	/* mark old address as dead; insert new address */
	a->q_state = QS_REPLACED;
	if (tTd(29, 5))
	{
		dprintf("maplocaluser: QS_REPLACED ");
		printaddr(a, FALSE);
	}
	a1->q_alias = a;
	allocaddr(a1, RF_COPYALL, newstr(a->q_paddr));
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

	/* there is no check whether there is really an argument */
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

		  case 'D':
			map->map_mflags |= MF_DEFER;
			break;

		  case 'S':
		  case 's':
			map->map_spacesub = *++p;
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

/* ARGSUSED2 */
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
	char spacesub = map->map_spacesub;

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
		  case '\t':
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
	return map_rewrite(map, name, strlen(name), NULL);
}
/*
**  RSCHECK -- check string(s) for validity using rewriting sets
**
**	Parameters:
**		rwset -- the rewriting set to use.
**		p1 -- the first string to check.
**		p2 -- the second string to check -- may be null.
**		e -- the current envelope.
**		rmcomm -- remove comments?
**		cnt -- count rejections (statistics)?
**		logl -- logging level
**
**	Returns:
**		EX_OK -- if the rwset doesn't resolve to $#error
**		else -- the failure status (message printed)
*/

int
rscheck(rwset, p1, p2, e, rmcomm, cnt, logl)
	char *rwset;
	char *p1;
	char *p2;
	ENVELOPE *e;
	bool rmcomm, cnt;
	int logl;
{
	char *buf;
	int bufsize;
	int saveexitstat;
	int rstat = EX_OK;
	char **pvp;
	int rsno;
	bool discard = FALSE;
	auto ADDRESS a1;
	bool saveQuickAbort = QuickAbort;
	bool saveSuprErrs = SuprErrs;
	char buf0[MAXLINE];
	char pvpbuf[PSBUFSIZE];
	extern char MsgBuf[];

	if (tTd(48, 2))
		dprintf("rscheck(%s, %s, %s)\n", rwset, p1,
			p2 == NULL ? "(NULL)" : p2);

	rsno = strtorwset(rwset, NULL, ST_FIND);
	if (rsno < 0)
		return EX_OK;

	if (p2 != NULL)
	{
		bufsize = strlen(p1) + strlen(p2) + 2;
		if (bufsize > sizeof buf0)
			buf = xalloc(bufsize);
		else
		{
			buf = buf0;
			bufsize = sizeof buf0;
		}
		(void) snprintf(buf, bufsize, "%s%c%s", p1, CONDELSE, p2);
	}
	else
	{
		bufsize = strlen(p1) + 1;
		if (bufsize > sizeof buf0)
			buf = xalloc(bufsize);
		else
		{
			buf = buf0;
			bufsize = sizeof buf0;
		}
		(void) snprintf(buf, bufsize, "%s", p1);
	}
	SuprErrs = TRUE;
	QuickAbort = FALSE;
	pvp = prescan(buf, '\0', pvpbuf, sizeof pvpbuf, NULL,
		      rmcomm ? NULL : TokTypeNoC);
	SuprErrs = saveSuprErrs;
	if (pvp == NULL)
	{
		if (tTd(48, 2))
			dprintf("rscheck: cannot prescan input\n");
/*
		syserr("rscheck: cannot prescan input: \"%s\"",
			shortenstring(buf, MAXSHORTSTR));
		rstat = EX_DATAERR;
*/
		goto finis;
	}
	(void) rewrite(pvp, rsno, 0, e);
	if (pvp[0] == NULL || (pvp[0][0] & 0377) != CANONNET ||
	    pvp[1] == NULL || (strcmp(pvp[1], "error") != 0 &&
			       strcmp(pvp[1], "discard") != 0))
	{
		goto finis;
	}

	if (strcmp(pvp[1], "discard") == 0)
	{
		if (tTd(48, 2))
			dprintf("rscheck: discard mailer selected\n");
		e->e_flags |= EF_DISCARD;
		discard = TRUE;
	}
	else
	{
		int savelogusrerrs = LogUsrErrs;
		static bool logged = FALSE;

		/* got an error -- process it */
		saveexitstat = ExitStat;
		LogUsrErrs = FALSE;
		(void) buildaddr(pvp, &a1, 0, e);
		LogUsrErrs = savelogusrerrs;
		rstat = ExitStat;
		ExitStat = saveexitstat;
		if (!logged)
		{
			if (cnt)
				markstats(e, &a1, TRUE);
			logged = TRUE;
		}
	}

	if (LogLevel >= logl)
	{
		char *relay;
		char *p;
		char lbuf[MAXLINE];

		p = lbuf;
		if (p2 != NULL)
		{
			snprintf(p, SPACELEFT(lbuf, p),
				", arg2=%s",
				p2);
			p += strlen(p);
		}
		if ((relay = macvalue('_', e)) != NULL)
		{
			snprintf(p, SPACELEFT(lbuf, p),
				", relay=%s", relay);
			p += strlen(p);
		}
		*p = '\0';
		if (discard)
			sm_syslog(LOG_NOTICE, e->e_id,
				  "ruleset=%s, arg1=%s%s, discard",
				  rwset, p1, lbuf);
		else
			sm_syslog(LOG_NOTICE, e->e_id,
				  "ruleset=%s, arg1=%s%s, reject=%s",
				  rwset, p1, lbuf, MsgBuf);
	}

 finis:
	/* clean up */
	QuickAbort = saveQuickAbort;
	setstat(rstat);
	if (buf != buf0)
		free(buf);

	if (rstat != EX_OK && QuickAbort)
		longjmp(TopFrame, 2);
	return rstat;
}
