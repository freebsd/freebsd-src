/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *      This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	gtagsopen.c				20-Oct-97
 *
 */
#include <sys/param.h>

#include <ctype.h>
#include <db.h>
#include <fcntl.h>
#include <stdlib.h>

#include "dbio.h"
#include "dbname.h"
#include "die.h"
#include "gtagsopen.h"
#include "makepath.h"

#define	VERSIONKEY	" __.VERSION"
static int	support_version = 1;	/* accept this format version	*/

/*
 * gtagsopen: open global database.
 *
 *	i)	dbpath	dbpath directory
 *	i)	db	GTAGS, GRTAGS, GSYMS
 *	i)	mode	0: read only
 *			1: write only
 *			2: read and write
 *	r)		DB structure
 *
 * when error occurred, gtagopen doesn't return.
 */
DBIO	*
gtagsopen(dbpath, db, mode)
char	*dbpath;
int	db;
int	mode;
{
	DBIO	*dbio;
	int	version_number;
	char	*p;

	/*
	 * allow duplicate records.
	 */
	dbio = db_open(makepath(dbpath, dbname(db)), mode, 0644, DBIO_DUP);
	if (dbio == NULL) {
		if (mode == 1)
			die1("cannot make database (%s).", makepath(dbpath, dbname(db)));
		die1("database not found (%s).", makepath(dbpath, dbname(db)));
	}
	if (mode == 1) {
		/* nothing to do now */
	} else {
		/*
		 * recognize format version of GTAGS. 'format version record'
		 * is saved as a META record in GTAGS and GRTAGS.
		 * if 'format version record' is not found, it's assumed
		 * version 1.
		 */
		if ((p = db_get(dbio, VERSIONKEY)) != NULL) {
			for (p += strlen(VERSIONKEY); *p && isspace(*p); p++)
				;
			version_number = atoi(p);
		} else
			version_number = 1;
		if (version_number > support_version)
			die("GTAGS seems new format. Please install the latest GLOBAL.");
	}
	return dbio;
}
