/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
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
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tcreat3.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include "db-int.h"

#define INITIAL	25000
#define MAXWORDS    25000	       /* # of elements in search table */

char	wp1[8192];
char	wp2[8192];
main(argc, argv)
char **argv;
{
	DBT item, key;
	DB	*dbp;
	HASHINFO ctl;
	FILE *fp;
	int	trash;

	int i = 0;

	argv++;
	ctl.hash = NULL;
	ctl.bsize = atoi(*argv++);
	ctl.ffactor = atoi(*argv++);
	ctl.nelem = atoi(*argv++);
	ctl.lorder = 0;
	if (!(dbp = dbopen( "hashtest",
	    O_CREAT|O_TRUNC|O_RDWR|O_BINARY, 0600, DB_HASH, &ctl))){
		/* create table */
		fprintf(stderr, "cannot create: hash table (size %d)\n",
			INITIAL);
		exit(1);
	}

	key.data = wp1;
	item.data = wp2;
	while ( fgets(wp1, 8192, stdin) &&
		fgets(wp2, 8192, stdin) &&
		i++ < MAXWORDS) {
/*
* put info in structure, and structure in the item
*/
		key.size = strlen(wp1);
		item.size = strlen(wp2);

/*
 * enter key/data pair into the table
 */
		if ((dbp->put)(dbp, &key, &item, R_NOOVERWRITE) != NULL) {
			fprintf(stderr, "cannot enter: key %s\n",
				item.data);
			exit(1);
		}
	}

	(dbp->close)(dbp);
	exit(0);
}
