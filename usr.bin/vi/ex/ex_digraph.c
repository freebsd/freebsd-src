/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)ex_digraph.c	8.6 (Berkeley) 3/25/94";
#endif /* not lint */

#ifndef NO_DIGRAPH
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

static void do_digraph __P((SCR *, EXF *, int, u_char *));

/* This stuff is used to build the default digraphs table. */
static u_char digtable[][4] = {
# ifdef CS_IBMPC
	"C,\200",	"u\"\1",	"e'\2",		"a^\3",
	"a\"\4",	"a`\5",		"a@\6",		"c,\7",
	"e^\10",	"e\"\211",	"e`\12",	"i\"\13",
	"i^\14",	"i`\15",	"A\"\16",	"A@\17",
	"E'\20",	"ae\21",	"AE\22",	"o^\23",
	"o\"\24",	"o`\25",	"u^\26",	"u`\27",
	"y\"\30",	"O\"\31",	"U\"\32",	"a'\240",
	"i'!",		"o'\"",		"u'#",		"n~$",
	"N~%",		"a-&",		"o-'",		"~?(",
	"~!-",		"\"<.",		"\">/",
#  ifdef CS_SPECIAL
	"2/+",		"4/,",		"^+;",		"^q<",
	"^c=",		"^r>",		"^t?",		"pp]",
	"^^^",		"oo_",		"*a`",		"*ba",
	"*pc",		"*Sd",		"*se",		"*uf",
	"*tg",		"*Ph",		"*Ti",		"*Oj",
	"*dk",		"*Hl",		"*hm",		"*En",
	"*No",		"eqp",		"pmq",		"ger",
	"les",		"*It",		"*iu",		"*/v",
	"*=w",		"sq{",		"^n|",		"^2}",
	"^3~",		"^_\377",
#  endif /* CS_SPECIAL */
# endif /* CS_IBMPC */
# ifdef CS_LATIN1
	"~!!",		"a-*",		"\">+",		"o-:",
	"\"<>",		"~??",

	"A`@",		"A'A",		"A^B",		"A~C",
	"A\"D",		"A@E",		"AEF",		"C,G",
	"E`H",		"E'I",		"E^J",		"E\"K",
	"I`L",		"I'M",		"I^N",		"I\"O",
	"-DP",		"N~Q",		"O`R",		"O'S",
	"O^T",		"O~U",		"O\"V",		"O/X",
	"U`Y",		"U'Z",		"U^[",		"U\"\\",
	"Y'_",

	"a``",		"a'a",		"a^b",		"a~c",
	"a\"d",		"a@e",		"aef",		"c,g",
	"e`h",		"e'i",		"e^j",		"e\"k",
	"i`l",		"i'm",		"i^n",		"i\"o",
	"-dp",		"n~q",		"o`r",		"o's",
	"o^t",		"o~u",		"o\"v",		"o/x",
	"u`y",		"u'z",		"u^{",		"u\"|",
	"y'~",
# endif /* CS_LATIN1 */
	""
};

int
digraph_init(sp)
	SCR *sp;
{
	int	i;

	for (i = 0; *digtable[i]; i++)
		do_digraph(sp, NULL, 0, digtable[i]);
	do_digraph(sp, NULL, 0, NULL);
	return (0);
}

int
ex_digraph(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	do_digraph(sp, ep, F_ISSET(cmdp, E_FORCE), cmdp->argv[0]->bp);
	return (0);
}

static struct _DIG
{
	struct _DIG	*next;
	char		key1;
	char		key2;
	char		dig;
	char		save;
} *digs;

int
digraph(sp, key1, key2)
	SCR *sp;
	char	key1;	/* the underlying character */
	char	key2;	/* the second character */
{
	int		new_key;
	register struct _DIG	*dp;

	/* if digraphs are disabled, then just return the new char */
	if (O_ISSET(sp, O_DIGRAPH))
	{
		return key2;
	}

	/* remember the new key, so we can return it if this isn't a digraph */
	new_key = key2;

	/* sort key1 and key2, so that their original order won't matter */
	if (key1 > key2)
	{
		key2 = key1;
		key1 = new_key;
	}

	/* scan through the digraph chart */
	for (dp = digs;
	     dp && (dp->key1 != key1 || dp->key2 != key2);
	     dp = dp->next)
	{
	}

	/* if this combination isn't in there, just use the new key */
	if (!dp)
	{
		return new_key;
	}

	/* else use the digraph key */
	return dp->dig;
}

/* this function lists or defines digraphs */
static void
do_digraph(sp, ep, bang, extra)
	SCR *sp;
	EXF *ep;
	int	bang;
	u_char	*extra;
{
	int		dig;
	register struct _DIG	*dp;
	struct _DIG	*prev;
	static int	user_defined = 0; /* boolean: are all later digraphs user-defined? */
	char		listbuf[8];

	/* if "extra" is NULL, then we've reached the end of the built-ins */
	if (!extra)
	{
		user_defined = 1;
		return;
	}

	/* if no args, then display the existing digraphs */
	if (*extra < ' ')
	{
		listbuf[0] = listbuf[1] = listbuf[2] = listbuf[5] = ' ';
		listbuf[7] = '\0';
		for (dig = 0, dp = digs; dp; dp = dp->next)
		{
			if (dp->save || bang)
			{
				dig += 7;
				if (dig >= sp->cno)
				{
					addch('\n');
					refresh();
					dig = 7;
				}
				listbuf[3] = dp->key1;
				listbuf[4] = dp->key2;
				listbuf[6] = dp->dig;
				addstr(listbuf);
			}
		}
		addch('\n');
		refresh();
		return;
	}

	/* make sure we have at least two characters */
	if (!extra[1])
	{
		msgq(sp, M_ERR,
		    "Digraphs must be composed of two characters");
		return;
	}

	/* sort key1 and key2, so that their original order won't matter */
	if (extra[0] > extra[1])
	{
		dig = extra[0];
		extra[0] = extra[1];
		extra[1] = dig;
	}

	/* locate the new digraph character */
	for (dig = 2; extra[dig] == ' ' || extra[dig] == '\t'; dig++)
	{
	}
	dig = extra[dig];
	if (!bang && dig)
	{
		dig |= 0x80;
	}

	/* search for the digraph */
	for (prev = (struct _DIG *)0, dp = digs;
	     dp && (dp->key1 != extra[0] || dp->key2 != extra[1]);
	     prev = dp, dp = dp->next)
	{
	}

	/* deleting the digraph? */
	if (!dig)
	{
		if (!dp)
		{
#ifndef CRUNCH
			msgq(sp, M_ERR,
			    "%c%c not a digraph", extra[0], extra[1]);
#endif
			return;
		}
		if (prev)
			prev->next = dp->next;
		else
			digs = dp->next;
		free(dp);
		return;
	}

	/* if necessary, create a new digraph struct for the new digraph */
	if (dig && !dp)
	{
		MALLOC(sp, dp, struct _DIG *, sizeof(struct _DIG));
		if (dp == NULL)
			return;
		if (prev)
			prev->next = dp;
		else
			digs = dp;
		dp->next = (struct _DIG *)0;
	}

	/* assign it the new digraph value */
	dp->key1 = extra[0];
	dp->key2 = extra[1];
	dp->dig = dig;
	dp->save = user_defined;
}

void
digraph_save(sp, fd)
	SCR *sp;
	int fd;
{
	static char	buf[] = "digraph! XX Y\n";
	register struct _DIG	*dp;

	for (dp = digs; dp; dp = dp->next)
	{
		if (dp->save)
		{
			buf[9] = dp->key1;
			buf[10] = dp->key2;
			buf[12] = dp->dig;
			write(fd, buf, (unsigned)14);
		}
	}
}
#endif
