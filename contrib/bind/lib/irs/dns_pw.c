/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: dns_pw.c,v 1.19 2001/05/29 05:48:32 marka Exp $";
#endif

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_pw_unneeded;
#else

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <isc/memcluster.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "hesiod.h"
#include "dns_p.h"

/* Types. */

struct pvt {
	struct dns_p *	dns;
	struct passwd	passwd;
	char *		pwbuf;
};

/* Forward. */

static void 			pw_close(struct irs_pw *);
static struct passwd *		pw_byname(struct irs_pw *, const char *);
static struct passwd *		pw_byuid(struct irs_pw *, uid_t);
static struct passwd *		pw_next(struct irs_pw *);
static void 			pw_rewind(struct irs_pw *);
static void			pw_minimize(struct irs_pw *);
static struct __res_state *	pw_res_get(struct irs_pw *);
static void			pw_res_set(struct irs_pw *,
					   struct __res_state *,
					   void (*)(void *));

static struct passwd *		getpwcommon(struct irs_pw *, const char *,
					    const char *);

/* Public. */

struct irs_pw *
irs_dns_pw(struct irs_acc *this) {
	struct dns_p *dns = (struct dns_p *)this->private;
	struct irs_pw *pw;
	struct pvt *pvt;

	if (!dns || !dns->hes_ctx) {
		errno = ENODEV;
		return (NULL);
	}
	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->dns = dns;
	if (!(pw = memget(sizeof *pw))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pw, 0x5e, sizeof *pw);
	pw->private = pvt;
	pw->close = pw_close;
	pw->byname = pw_byname;
	pw->byuid = pw_byuid;
	pw->next = pw_next;
	pw->rewind = pw_rewind;
	pw->minimize = pw_minimize;
	pw->res_get = pw_res_get;
	pw->res_set = pw_res_set;
	return (pw);
}

/* Methods. */

static void
pw_close(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->pwbuf)
		free(pvt->pwbuf);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct passwd *
pw_byname(struct irs_pw *this, const char *nam) {
	return (getpwcommon(this, nam, "passwd"));
}

static struct passwd *
pw_byuid(struct irs_pw *this, uid_t uid) {
	char uidstr[16];

	sprintf(uidstr, "%lu", (u_long)uid);
	return (getpwcommon(this, uidstr, "uid"));
}

static struct passwd *
pw_next(struct irs_pw *this) {
	UNUSED(this);
	errno = ENODEV;
	return (NULL);
}

static void
pw_rewind(struct irs_pw *this) {
	UNUSED(this);
	/* NOOP */
}

static void
pw_minimize(struct irs_pw *this) {
	UNUSED(this);
	/* NOOP */
}

static struct __res_state *
pw_res_get(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	return (__hesiod_res_get(dns->hes_ctx));
}

static void
pw_res_set(struct irs_pw *this, struct __res_state * res,
	   void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	__hesiod_res_set(dns->hes_ctx, res, free_res);
}

/* Private. */

static struct passwd *
getpwcommon(struct irs_pw *this, const char *arg, const char *type) {
	struct pvt *pvt = (struct pvt *)this->private;
	char **hes_list, *cp;

	if (!(hes_list = hesiod_resolve(pvt->dns->hes_ctx, arg, type)))
		return (NULL);
	if (!*hes_list) {
		hesiod_free_list(pvt->dns->hes_ctx, hes_list);
		errno = ENOENT;
		return (NULL);
	}

	memset(&pvt->passwd, 0, sizeof pvt->passwd);
	if (pvt->pwbuf)
		free(pvt->pwbuf);
	pvt->pwbuf = strdup(*hes_list);
	hesiod_free_list(pvt->dns->hes_ctx, hes_list);

	cp = pvt->pwbuf;
	pvt->passwd.pw_name = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
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
	return (&pvt->passwd);
	
 cleanup:
	free(pvt->pwbuf);
	pvt->pwbuf = NULL;
	return (NULL);
}

#endif /* WANT_IRS_PW */
