/*
 * Copyright (c) 1983 Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)getword.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"hangman.h"

# if pdp11
#	define	RN	(((off_t) rand() << 16) | (off_t) rand())
# else
#	define	RN	rand()
# endif

/*
 * getword:
 *	Get a valid word out of the dictionary file
 */
getword()
{
	register FILE		*inf;
	register char		*wp, *gp;

	inf = Dict;
	for (;;) {
		fseek(inf, abs(RN % Dict_size), 0);
		if (fgets(Word, BUFSIZ, inf) == NULL)
			continue;
		if (fgets(Word, BUFSIZ, inf) == NULL)
			continue;
		Word[strlen(Word) - 1] = '\0';
		if (strlen(Word) < MINLEN)
			continue;
		for (wp = Word; *wp; wp++)
			if (!islower(*wp))
				goto cont;
		break;
cont:		;
	}
	gp = Known;
	wp = Word;
	while (*wp) {
		*gp++ = '-';
		wp++;
	}
	*gp = '\0';
}

/*
 * abs:
 *	Return the absolute value of an integer
 */
off_t
abs(i)
off_t	i;
{
	if (i < 0)
		return -(off_t) i;
	else
		return (off_t) i;
}
