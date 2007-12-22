/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: nis_pw.c,v 1.3.18.1 2005/04/27 05:01:04 sra Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#if !defined(WANT_IRS_PW) || !defined(WANT_IRS_NIS)
static int __bind_irs_pw_unneeded;
#else

#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <isc/memcluster.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <isc/memcluster.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "nis_p.h"

/* Definitions */

struct pvt {
	int		needrewind;
	char *		nis_domain;
	char *		curkey_data;
	int		curkey_len;
	char *		curval_data;
	int		curval_len;
	struct passwd 	passwd;
	char *		pwbuf;
};

enum do_what { do_none = 0x0, do_key = 0x1, do_val = 0x2, do_all = 0x3 };

static /*const*/ char passwd_byname[] =	"passwd.byname";
static /*const*/ char passwd_byuid[] =	"passwd.byuid";

/* Forward */

static void			pw_close(struct irs_pw *);
static struct passwd *		pw_next(struct irs_pw *);
static struct passwd *		pw_byname(struct irs_pw *, const char *);
static struct passwd *		pw_byuid(struct irs_pw *, uid_t);
static void			pw_rewind(struct irs_pw *);
static void			pw_minimize(struct irs_pw *);

static struct passwd *		makepasswdent(struct irs_pw *);
static void			nisfree(struct pvt *, enum do_what);

/* Public */

struct irs_pw *
irs_nis_pw(struct irs_acc *this) {
	struct irs_pw *pw;
	struct pvt *pvt;
		 
        if (!(pw = memget(sizeof *pw))) {
                errno = ENOMEM;
                return (NULL);
        }
        memset(pw, 0x5e, sizeof *pw);
        if (!(pvt = memget(sizeof *pvt))) {
                memput(pw, sizeof *pw);
                errno = ENOMEM;
                return (NULL);
        }
        memset(pvt, 0, sizeof *pvt);
	pvt->needrewind = 1;
	pvt->nis_domain = ((struct nis_p *)this->private)->domain;
	pw->private = pvt;
	pw->close = pw_close;
	pw->next = pw_next;
	pw->byname = pw_byname;
	pw->byuid = pw_byuid;
	pw->rewind = pw_rewind;
	pw->minimize = pw_minimize;
	pw->res_get = NULL;
	pw->res_set = NULL;
	return (pw);
}

/* Methods */

static void
pw_close(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (pvt->pwbuf)
		free(pvt->pwbuf);
	nisfree(pvt, do_all);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct passwd *
pw_next(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct passwd *rval;
	int r;

	do {
		if (pvt->needrewind) {
			nisfree(pvt, do_all);
			r = yp_first(pvt->nis_domain, passwd_byname,
				     &pvt->curkey_data, &pvt->curkey_len,
				     &pvt->curval_data, &pvt->curval_len);
			pvt->needrewind = 0;
		} else {
			char *newkey_data;
			int newkey_len;

			nisfree(pvt, do_val);
			r = yp_next(pvt->nis_domain, passwd_byname,
				    pvt->curkey_data, pvt->curkey_len,
				    &newkey_data, &newkey_len,
				    &pvt->curval_data, &pvt->curval_len);
			nisfree(pvt, do_key);
			pvt->curkey_data = newkey_data;
			pvt->curkey_len = newkey_len;
		}
		if (r != 0) {
			errno = ENOENT;
			return (NULL);
		}
		rval = makepasswdent(this);
	} while (rval == NULL);
	return (rval);
}

static struct passwd *
pw_byname(struct irs_pw *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	int r;
	char *tmp;

	nisfree(pvt, do_val);
	DE_CONST(name, tmp);
	r = yp_match(pvt->nis_domain, passwd_byname, tmp, strlen(tmp),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		errno = ENOENT;
		return (NULL);
	}
	return (makepasswdent(this));
}

static struct passwd *
pw_byuid(struct irs_pw *this, uid_t uid) {
	struct pvt *pvt = (struct pvt *)this->private;
	char tmp[sizeof "4294967295"];
	int r;

	nisfree(pvt, do_val);
	(void) sprintf(tmp, "%u", (unsigned int)uid);
	r = yp_match(pvt->nis_domain, passwd_byuid, tmp, strlen(tmp),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		errno = ENOENT;
		return (NULL);
	}
	return (makepasswdent(this));
}

static void
pw_rewind(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pvt->needrewind = 1;
}

static void
pw_minimize(struct irs_pw *this) {
	UNUSED(this);
	/* NOOP */
}

/* Private */

static struct passwd *
makepasswdent(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *cp;

	memset(&pvt->passwd, 0, sizeof pvt->passwd);
	if (pvt->pwbuf)
		free(pvt->pwbuf);
	pvt->pwbuf = pvt->curval_data;
	pvt->curval_data = NULL;

	cp = pvt->pwbuf;
	pvt->passwd.pw_name = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
#ifdef HAS_PW_CLASS
	pvt->passwd.pw_class = cp;	/*%< Needs to point at a \0. */
#endif
	*cp++ = '\0';

	pvt->passwd.pw_passwd = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';
	
	pvt->passwd.pw_uid = atoi(cp);
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';

	pvt->passwd.pw_gid = atoi(cp);
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';

	pvt->passwd.pw_gecos = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';

	pvt->passwd.pw_dir = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';

	pvt->passwd.pw_shell = cp;

	if ((cp = strchr(cp, '\n')) != NULL)
		*cp = '\0';

	return (&pvt->passwd);
	
 cleanup:
	free(pvt->pwbuf);
	pvt->pwbuf = NULL;
	return (NULL);
}

static void
nisfree(struct pvt *pvt, enum do_what do_what) {
	if ((do_what & do_key) && pvt->curkey_data) {
		free(pvt->curkey_data);
		pvt->curkey_data = NULL;
	}
	if ((do_what & do_val) && pvt->curval_data) {
		free(pvt->curval_data);
		pvt->curval_data = NULL;
	}
}

#endif /* WANT_IRS_PW && WANT_IRS_NIS */
/*! \file */
