/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
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

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: irp_ng.c,v 8.2 1999/10/13 16:39:31 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include <irs.h>
#include <irp.h>
#include <isc/memcluster.h>
#include <isc/irpmarshall.h>

#include "irs_p.h"
#include "irp_p.h"

#include "port_after.h"

/* Definitions */

struct pvt {
	struct irp_p	       *girpdata;
	int			warned;
};


/* Forward */

static void		ng_rewind(struct irs_ng *, const char*);
static void		ng_close(struct irs_ng *);
static int		ng_next(struct irs_ng *, char **, char **, char **);
static int		ng_test(struct irs_ng *, const char *,
				const char *, const char *,
				const char *);
static void		ng_minimize(struct irs_ng *);


/* Public */



/*
 * struct irs_ng * irs_irp_ng(struct irs_acc *this)
 *
 * Notes:
 *
 *	Intialize the irp netgroup module.
 *
 */

struct irs_ng *
irs_irp_ng(struct irs_acc *this) {
	struct irs_ng *ng;
	struct pvt *pvt;

	if (!(ng = memget(sizeof *ng))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(ng, 0x5e, sizeof *ng);

	if (!(pvt = memget(sizeof *pvt))) {
		memput(ng, sizeof *ng);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->girpdata = this->private;

	ng->private = pvt;
	ng->close = ng_close;
	ng->next = ng_next;
	ng->test = ng_test;
	ng->rewind = ng_rewind;
	ng->minimize = ng_minimize;
	return (ng);
}

/* Methods */



/*
 * void ng_close(struct irs_ng *this)
 *
 */

static void
ng_close(struct irs_ng *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	ng_minimize(this);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}




/*
 * void ng_rewind(struct irs_ng *this, const char *group)
 *
 *
 */

static void
ng_rewind(struct irs_ng *this, const char *group) {
	struct pvt *pvt = (struct pvt *)this->private;
	char text[256];
	int code;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return;
	}

	if (irs_irp_send_command(pvt->girpdata,
				 "setnetgrent %s", group) != 0) {
		return;
	}

	code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
	if (code != IRPD_GETNETGR_SETOK) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "setnetgrent(%s) failed: %s",
			       group, text);
		}
	}

	return;
}




/*
 * int ng_next(struct irs_ng *this, char **host, char **user, char **domain)
 *
 * Notes:
 *
 *	Get the next netgroup item from the cache.
 *
 */

static int
ng_next(struct irs_ng *this, char **host, char **user, char **domain) {
	struct pvt *pvt = (struct pvt *)this->private;
	int code;
	char *body = NULL;
	size_t bodylen;
	int rval = 0;
	char text[256];

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (0);
	}

	if (irs_irp_send_command(pvt->girpdata, "getnetgrent") != 0)
		return (0);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (0);
	}

	if (code == IRPD_GETNETGR_OK) {
		if (irp_unmarshall_ng(host, user, domain, body) == 0) {
			rval = 1;
		}
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (rval);
}



/*
 * int ng_test(struct irs_ng *this, const char *name, const char *host,
 *		const char *user, const char *domain)
 *
 * Notes:
 *
 *	Search for a match in a netgroup.
 *
 */

static int
ng_test(struct irs_ng *this, const char *name,
	const char *host, const char *user, const char *domain)
{
	struct pvt *pvt = (struct pvt *)this->private;
	char *body = NULL;
	size_t bodylen = 0;
	int code;
	char text[256];
	int rval = 0;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (0);
	}

	if (irp_marshall_ng(host, user, domain, &body, &bodylen) != 0) {
		return (0);
	}

	if (irs_irp_send_command(pvt->girpdata, "innetgr %s", body) == 0) {
		memput(body, bodylen);

		code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
		if (code == IRPD_GETNETGR_MATCHES) {
			rval = 1;
		}
	}

	return (rval);
}




/*
 * void ng_minimize(struct irs_ng *this)
 *
 */

static void
ng_minimize(struct irs_ng *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	irs_irp_disconnect(pvt->girpdata);
}




/* Private */

