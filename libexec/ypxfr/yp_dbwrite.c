/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yp_dbwrite.c,v 1.1.1.1 1995/12/25 03:07:13 wpaul Exp $
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <db.h>
#include <sys/stat.h>
#include <errno.h>
#include <paths.h>
#include "yp.h"
#include "ypxfr_extern.h"

#ifndef lint
static const char rcsid[] = "$Id: yp_dbwrite.c,v 1.1.1.1 1995/12/25 03:07:13 wpaul Exp $";
#endif

#define PERM_SECURE (S_IRUSR|S_IWUSR)

/*
 * Open a DB database read/write
 */
DB *yp_open_db_rw(domain, map)
	const char *domain;
	const char *map;
{
	DB *dbp;
	char buf[1025];


	yp_errno = YP_TRUE;

	if (map[0] == '.' || strchr(map, '/')) {
		yp_errno = YP_BADARGS;
		return (NULL);
	}

	snprintf(buf, sizeof(buf), "%s/%s/%s", yp_dir, domain, map);

	dbp = dbopen(buf,O_RDWR|O_EXLOCK|O_EXCL|O_CREAT, PERM_SECURE, DB_HASH, &openinfo);

	if (dbp == NULL) {
		switch(errno) {
		case ENOENT:
			yp_errno = YP_NOMAP;
			break;
		case EFTYPE:
			yp_errno = YP_BADDB;
			break;
		default:
			yp_errno = YP_YPERR;
			break;
		}
	}

	return (dbp);
}

int yp_put_record(dbp,key,data)
	DB *dbp;
	DBT *key;
	DBT *data;
{

	if ((dbp->put)(dbp,key,data,0)) {
		(void)(dbp->close)(dbp);
		return(YP_BADDB);
	}

	return(YP_TRUE);
}
