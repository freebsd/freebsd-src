/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods.
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
 * 	$Id: locate.bigram.c,v 1.4 1996/08/22 18:46:11 wosch Exp $
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)locate.bigram.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 *  bigram < sorted_file_names | sort -nr | 
 *  	awk 'NR <= 128 { printf $2 }' > bigrams
 *
 * List bigrams for 'updatedb' script.
 * Use 'code' to encode a file using this output.
 */

#include <stdio.h>
#include <sys/param.h>			/* for MAXPATHLEN */
#include "locate.h"

u_char buf1[MAXPATHLEN] = " ";
u_char buf2[MAXPATHLEN];
u_int bigram[UCHAR_MAX][UCHAR_MAX];

int
main(void)
{
  	register u_char *cp;
	register u_char *oldpath = buf1, *path = buf2;
	register u_int i, j;

     	while (fgets(path, sizeof(buf2), stdin) != NULL) {

	    	/* skip empty lines */
		if (*path == '\n')
			continue;

		/* Squelch characters that would botch the decoding. */
		for (cp = path; *cp != NULL; cp++) {
			/* chop newline */
			if (*cp == '\n')
				*cp = NULL;
			/* range */
			else if (*cp < ASCII_MIN || *cp > ASCII_MAX)
				*cp = '?';
		}

		/* skip longest common prefix */
		for (cp = path; *cp == *oldpath && *cp != NULL; cp++, oldpath++);

		while (*cp != NULL && *(cp+1) != NULL) {
			bigram[*cp][*(cp+1)]++;
			cp += 2;
		}

		/* swap pointers */
		if (path == buf1) { 
			path = buf2;
			oldpath = buf1;
		} else {
			path = buf1;
			oldpath = buf2;
		}
   	}

	/* output, (paranoid) boundary check */
	for (i = ASCII_MIN; i <= ASCII_MAX; i++)
		for (j = ASCII_MIN; j <= ASCII_MAX; j++)
			if (bigram[i][j] != 0)
				printf("%4u %c%c\n", bigram[i][j], i, j);

	exit(0);
}
