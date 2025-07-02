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
static char sccsid[] = "@(#)driver2.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

/*
 * Test driver, to try to tackle the large ugly-split problem.
 */

#include <sys/file.h>
#include <stdio.h>
#include "ndbm.h"

int my_hash(key, len)
	char	*key;
	int	len;
{
	return(17);		/* So I'm cruel... */
}

main(argc, argv)
	int	argc;
{
	DB	*db;
	DBT	key, content;
	char	keybuf[2049];
	char	contentbuf[2049];
	char	buf[256];
	int	i;
	HASHINFO	info;

	info.bsize = 1024;
	info.ffactor = 5;
	info.nelem = 1;
	info.cachesize = NULL;
#ifdef HASH_ID_PROGRAM_SPECIFIED
	info.hash_id = HASH_ID_PROGRAM_SPECIFIED;
	info.hash_func = my_hash;
#else
	info.hash = my_hash;
#endif
	info.lorder = 0;
	if (!(db = dbopen("bigtest", O_RDWR | O_CREAT | O_BINARY, 0644, DB_HASH, &info))) {
		snprintf(buf, sizeof(buf), "dbopen: failed on file bigtest");
		perror(buf);
		exit(1);
	}
	srand(17);
	key.data = keybuf;
	content.data = contentbuf;
	memset(keybuf, '\0', sizeof(keybuf));
	memset(contentbuf, '\0', sizeof(contentbuf));
	for (i=1; i <= 500; i++) {
		key.size = 128 + (rand()&1023);
		content.size = 128 + (rand()&1023);
/*		printf("%d: Key size %d, data size %d\n", i, key.size,
		       content.size); */
		snprintf(keybuf, sizeof(keybuf), "Key #%d", i);
		snprintf(contentbuf, sizeof(contentbuf), "Contents #%d", i);
		if ((db->put)(db, &key, &content, R_NOOVERWRITE)) {
			snprintf(buf, sizeof(buf), "dbm_store #%d", i);
			perror(buf);
		}
	}
	if ((db->close)(db)) {
		perror("closing hash file");
		exit(1);
	}
	exit(0);
}
