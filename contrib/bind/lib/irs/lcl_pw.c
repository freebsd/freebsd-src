/*
 * Copyright (c) 1989, 1993, 1995
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

/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: lcl_pw.c,v 1.16 1998/02/13 01:10:42 halley Exp $";
#endif /* LIBC_SCCS and not lint */

/* Extern */

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_pw_unneeded;
#else

#include <sys/param.h>

#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <utmp.h>
#include <unistd.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "lcl_p.h"

/*
 * The lookup techniques and data extraction code here must be kept
 * in sync with that in `pwd_mkdb'.
 */
 

/* Types */

struct  pvt {
	struct passwd	passwd;		/* password structure */
	DB 		*pw_db;		/* password database */
	int		pw_keynum;	/* key counter */
	int		warned;
	u_int		max;
	char *		line;
};

/* Forward */

static void			pw_close(struct irs_pw *);
static struct passwd *		pw_next(struct irs_pw *);
static struct passwd *		pw_byname(struct irs_pw *, const char *);
static struct passwd *		pw_byuid(struct irs_pw *, uid_t);
static void			pw_rewind(struct irs_pw *);
static void			pw_minimize(struct irs_pw *);

static int			initdb(struct pvt *);
static int			hashpw(struct irs_pw *, DBT *);

/* Public */
struct irs_pw *
irs_lcl_pw(struct irs_acc *this) {
	struct irs_pw *pw;
	struct pvt *pvt;
		 
        if (!(pw = malloc(sizeof *pw))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pw, 0x5e, sizeof *pw);
	if (!(pvt = malloc(sizeof *pvt))) {
		free(pw);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pw->private = pvt;
	pw->close = pw_close;
	pw->next = pw_next;
	pw->byname = pw_byname;
	pw->byuid = pw_byuid;
	pw->rewind = pw_rewind;
	pw->minimize = pw_minimize;
	return (pw);
}

/* Methods */

static void
pw_close(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (pvt->pw_db) {
		(void)(pvt->pw_db->close)(pvt->pw_db);
		pvt->pw_db = NULL;
	}
	if (pvt->line)
		free(pvt->line);
	free(pvt);
	free(this);
}

static struct passwd *
pw_next(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	DBT key;
	char bf[sizeof(pvt->pw_keynum) + 1];
 
	if (!initdb(pvt))
		return (NULL);
 
	++pvt->pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	memcpy(bf + 1, (char *)&pvt->pw_keynum, sizeof(pvt->pw_keynum));
	key.data = (u_char *)bf;
	key.size = sizeof(pvt->pw_keynum) + 1;
	return (hashpw(this, &key) ? &pvt->passwd : NULL);
}
 
static struct passwd *
pw_byname(struct irs_pw *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	DBT key;
	int len, rval;
	char bf[UT_NAMESIZE + 1];
 
	if (!initdb(pvt))
		return (NULL);
 
	bf[0] = _PW_KEYBYNAME;
	len = strlen(name);
	memcpy(bf + 1, name, MIN(len, UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = len + 1;
	rval = hashpw(this, &key);
 
	return (rval ? &pvt->passwd : NULL);
}
 

static struct passwd *
pw_byuid(struct irs_pw *this, uid_t uid) {
	struct pvt *pvt = (struct pvt *)this->private;
	DBT key;
	int keyuid, rval;
	char bf[sizeof(keyuid) + 1];
 
	if (!initdb(pvt))
		return (NULL);
 
	bf[0] = _PW_KEYBYUID;
	keyuid = uid;
	memcpy(bf + 1, &keyuid, sizeof(keyuid));
	key.data = (u_char *)bf;
	key.size = sizeof(keyuid) + 1;
	rval = hashpw(this, &key);
 
	return (rval ? &pvt->passwd : NULL);
}

static void
pw_rewind(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pvt->pw_keynum = 0;
}

static void
pw_minimize(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->pw_db != NULL) {
		(void) (*pvt->pw_db->close)(pvt->pw_db);
		pvt->pw_db = NULL;
	}
}

/* Private. */

static int
initdb(struct pvt *pvt) {
	const char *p;

	if (pvt->pw_db) {
		if (lseek((*pvt->pw_db->fd)(pvt->pw_db), 0L, SEEK_CUR) >= 0L)
			return (1);
		else
			(void) (*pvt->pw_db->close)(pvt->pw_db);
	}
	pvt->pw_db = dbopen((p = _PATH_SMP_DB), O_RDONLY, 0, DB_HASH, NULL);
	if (!pvt->pw_db)
		pvt->pw_db = dbopen((p =_PATH_MP_DB), O_RDONLY,
				    0, DB_HASH, NULL);
	if (pvt->pw_db)
		return (1);
	if (!pvt->warned) {
		syslog(LOG_ERR, "%s: %m", p);
		pvt->warned++;
	}
	return (0);
}

static int
hashpw(struct irs_pw *this, DBT *key) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *p, *t, *l;
	DBT data;
 
	if ((pvt->pw_db->get)(pvt->pw_db, key, &data, 0))
		return (0);
	p = (char *)data.data;
	if (data.size > pvt->max &&
	    (pvt->line = realloc(pvt->line, pvt->max += 1024)) == NULL)
		return (0);
	/* THIS CODE MUST MATCH THAT IN pwd_mkdb. */
	t = pvt->line;
	l = pvt->line + pvt->max;
#define EXPAND(e) if ((e = t) == NULL) return (0); else \
		  do if (t >= l) return (0); while ((*t++ = *p++) != '\0')
#define SCALAR(v) if (t + sizeof v >= l) return (0); else \
		  (memmove(&(v), p, sizeof v), p += sizeof v)
	EXPAND(pvt->passwd.pw_name);
	EXPAND(pvt->passwd.pw_passwd);
	SCALAR(pvt->passwd.pw_uid);
	SCALAR(pvt->passwd.pw_gid);
	SCALAR(pvt->passwd.pw_change);
	EXPAND(pvt->passwd.pw_class);
	EXPAND(pvt->passwd.pw_gecos);
	EXPAND(pvt->passwd.pw_dir);
	EXPAND(pvt->passwd.pw_shell);
	SCALAR(pvt->passwd.pw_expire);
	return (1);
}

#endif /* WANT_IRS_PW */
