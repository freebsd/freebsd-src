/*-
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)testdb.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <errno.h>
#include <limits.h>
#include <kvm.h>
#include <db.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <paths.h>

#include "extern.h"

/* Return true if the db file is valid, else false */
int
testdb()
{
	register DB *db;
	register int cc, kd, ret, dbversionlen;
	register char *cp, *uf;
	DBT rec;
	struct nlist nitem;
	char dbname[MAXPATHLEN], dbversion[_POSIX2_LINE_MAX];
	char kversion[_POSIX2_LINE_MAX];

	ret = 0;
	db = NULL;

	if ((kd = open(_PATH_KMEM, O_RDONLY, 0)) < 0)
		goto close;

	uf = _PATH_UNIX;
	if ((cp = rindex(uf, '/')) != 0)
		uf = cp + 1;
	(void) snprintf(dbname, sizeof(dbname), "%skvm_%s.db", _PATH_VARDB, uf);
	if ((db = dbopen(dbname, O_RDONLY, 0, DB_HASH, NULL)) == NULL)
		goto close;

	/* Read the version out of the database */
	rec.data = VRS_KEY;
	rec.size = sizeof(VRS_KEY) - 1;
	if ((db->get)(db, &rec, &rec, 0))
		goto close;
	if (rec.data == 0 || rec.size > sizeof(dbversion))
		goto close;
	bcopy(rec.data, dbversion, rec.size);
	dbversionlen = rec.size;

	/* Read version string from kernel memory */
	rec.data = VRS_SYM;
	rec.size = sizeof(VRS_SYM) - 1;
	if ((db->get)(db, &rec, &rec, 0))
		goto close;
	if (rec.data == 0 || rec.size != sizeof(struct nlist))
		goto close;
	bcopy(rec.data, &nitem, sizeof(nitem));
	/*
	 * Theoretically possible for lseek to be seeking to -1.  Not
	 * that it's something to lie awake nights about, however.
	 */
	errno = 0;
	if (lseek(kd, (off_t)nitem.n_value, SEEK_SET) == -1 && errno != 0)
		goto close;
	cc = read(kd, kversion, sizeof(kversion));
	if (cc < 0 || cc != sizeof(kversion))
		goto close;

	/* If they match, we win */
	ret = bcmp(dbversion, kversion, dbversionlen) == 0;

close:	if (kd >= 0)
		(void)close(kd);
	if (db)
		(void)(db->close)(db);
	return (ret);
}
