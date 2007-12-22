/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (c) 1996,1998 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: irp_sv.c,v 1.2.18.1 2005/04/27 05:01:01 sra Exp $";
#endif /* LIBC_SCCS and not lint */

/* extern */

#include "port_before.h"

#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef IRS_LCL_SV_DB
#include <db.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>

#include <irs.h>
#include <irp.h>
#include <isc/irpmarshall.h>
#include <isc/memcluster.h>

#include "irs_p.h"
#include "lcl_p.h"
#include "irp_p.h"

#include "port_after.h"

/* Types */

struct pvt {
	struct irp_p	       *girpdata;
	int			warned;
	struct servent		service;
};

/* Forward */

static void			sv_close(struct irs_sv*);
static struct servent *		sv_next(struct irs_sv *);
static struct servent *		sv_byname(struct irs_sv *, const char *,
					  const char *);
static struct servent *		sv_byport(struct irs_sv *, int, const char *);
static void			sv_rewind(struct irs_sv *);
static void			sv_minimize(struct irs_sv *);

static void			free_service(struct servent *sv);



/* Public */

/*%
 * struct irs_sv * irs_irp_sv(struct irs_acc *this)
 *
 */

struct irs_sv *
irs_irp_sv(struct irs_acc *this) {
	struct irs_sv *sv;
	struct pvt *pvt;

	if ((sv = memget(sizeof *sv)) == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(sv, 0x0, sizeof *sv);

	if ((pvt = memget(sizeof *pvt)) == NULL) {
		memput(sv, sizeof *sv);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->girpdata = this->private;

	sv->private = pvt;
	sv->close = sv_close;
	sv->next = sv_next;
	sv->byname = sv_byname;
	sv->byport = sv_byport;
	sv->rewind = sv_rewind;
	sv->minimize = sv_minimize;

	return (sv);
}

/* Methods */

/*%
 * void sv_close(struct irs_sv *this)
 *
 */

static void
sv_close(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	sv_minimize(this);

	free_service(&pvt->service);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

/*%
 *	Fills the cache if necessary and returns the next item from it.
 *
 */

static struct servent *
sv_next(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct servent *sv = &pvt->service;
	char *body;
	size_t bodylen;
	int code;
	char text[256];

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getservent") != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETSERVICE_OK) {
		free_service(sv);
		if (irp_unmarshall_sv(sv, body) != 0) {
			sv = NULL;
		}
	} else {
		sv = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (sv);
}

/*%
 * struct servent * sv_byname(struct irs_sv *this, const char *name,
 *				const char *proto)
 *
 */

static struct servent *
sv_byname(struct irs_sv *this, const char *name, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct servent *sv = &pvt->service;
	char *body;
	char text[256];
	size_t bodylen;
	int code;

	if (sv->s_name != NULL &&
	    strcmp(name, sv->s_name) == 0 &&
	    strcasecmp(proto, sv->s_proto) == 0) {
		return (sv);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getservbyname %s %s",
				 name, proto) != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETSERVICE_OK) {
		free_service(sv);
		if (irp_unmarshall_sv(sv, body) != 0) {
			sv = NULL;
		}
	} else {
		sv = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (sv);
}

/*%
 * struct servent * sv_byport(struct irs_sv *this, int port,
 *				const char *proto)
 *
 */

static struct servent *
sv_byport(struct irs_sv *this, int port, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct servent *sv = &pvt->service;
	char *body;
	size_t bodylen;
	char text[256];
	int code;

	if (sv->s_name != NULL &&
	    port == sv->s_port &&
	    strcasecmp(proto, sv->s_proto) == 0) {
		return (sv);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getservbyport %d %s",
				 ntohs((short)port), proto) != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETSERVICE_OK) {
		free_service(sv);
		if (irp_unmarshall_sv(sv, body) != 0) {
			sv = NULL;
		}
	} else {
		sv = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (sv);
}

/*%
 * void sv_rewind(struct irs_sv *this)
 *
 */

static void
sv_rewind(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char text[256];
	int code;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return;
	}

	if (irs_irp_send_command(pvt->girpdata, "setservent") != 0) {
		return;
	}

	code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
	if (code != IRPD_GETSERVICE_SETOK) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "setservent failed: %s", text);
		}
	}

	return;
}

/*%
 * void sv_minimize(struct irs_sv *this)
 *
 */

static void
sv_minimize(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	irs_irp_disconnect(pvt->girpdata);
}






static void
free_service(struct servent *sv) {
	char **p;

	if (sv == NULL) {
		return;
	}

	if (sv->s_name != NULL) {
		free(sv->s_name);
	}

	for (p = sv->s_aliases ; p != NULL && *p != NULL ; p++) {
		free(*p);
	}

	if (sv->s_proto != NULL) {
		free(sv->s_proto);
	}
}



/*! \file */
