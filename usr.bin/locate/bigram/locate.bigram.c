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
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)locate.bigram.c	8.2 (Berkeley) 4/28/95";
#endif /* not lint */

/*
 *  bigram < text > bigrams
 * 
 * List bigrams for 'updatedb' script.
 * Use 'code' to encode a file using this output.
 */

#include <stdio.h>
#include <sys/param.h>			/* for MAXPATHLEN */

char buf1[MAXPATHLEN] = " ";	
char buf2[MAXPATHLEN];

main ( )
{
  	register char *cp;
	register char *oldpath = buf1, *path = buf2;

     	while ( fgets ( path, sizeof(buf2), stdin ) != NULL ) {

		/* skip longest common prefix */
		for ( cp = path; *cp == *oldpath; cp++, oldpath++ )
			if ( *oldpath == '\0' )
				break;
		/*
		 * output post-residue bigrams only
		 */
		while ( *cp != '\0' && *(cp + 1) != '\0' ) {
			putchar ( *cp++ );
			putchar ( *cp++ );
			putchar ( '\n' );
		}
		if ( path == buf1 )		/* swap pointers */
			path = buf2, oldpath = buf1;
		else
			path = buf1, oldpath = buf2;
   	}
	return (0);
}
