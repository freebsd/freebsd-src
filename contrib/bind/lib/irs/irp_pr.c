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
static const char rcsid[] = "$Id: irp_pr.c,v 8.1 1999/01/18 07:46:54 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

/* extern */

#include "port_before.h"

#include <syslog.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <syslog.h>

#include <irs.h>
#include <irp.h>
#include <isc/memcluster.h>
#include <isc/irpmarshall.h>

#include "irs_p.h"
#include "lcl_p.h"
#include "irp_p.h"

#include "port_after.h"


#define MAXALIASES	35

/* Types */

struct pvt {
	struct irp_p	       *girpdata;
	int			warned;
	struct protoent		proto;
};

/* Forward */

static void			pr_close(struct irs_pr *);
static struct protoent *	pr_next(struct irs_pr *);
static struct protoent *	pr_byname(struct irs_pr *, const char *);
static struct protoent *	pr_bynumber(struct irs_pr *, int);
static void			pr_rewind(struct irs_pr *);
static void			pr_minimize(struct irs_pr *);

static void			free_proto(struct protoent *pr);

/* Public */



/*
 * struct irs_pr * irs_irp_pr(struct irs_acc *this)
 *
 */

struct irs_pr *
irs_irp_pr(struct irs_acc *this) {
	struct irs_pr *pr;
	struct pvt *pvt;

	if (!(pr = memget(sizeof *pr))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pr, 0x0, sizeof *pr);

	if (!(pvt = memget(sizeof *pvt))) {
		memput(pr, sizeof *pr);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->girpdata = this->private;

	pr->private = pvt;
	pr->close = pr_close;
	pr->byname = pr_byname;
	pr->bynumber = pr_bynumber;
	pr->next = pr_next;
	pr->rewind = pr_rewind;
	pr->minimize = pr_minimize;
	return (pr);
}

/* Methods */



/*
 * void pr_close(struct irs_pr *this)
 *
 */

static void
pr_close(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pr_minimize(this);

	free_proto(&pvt->proto);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}



/*
 * struct protoent * pr_byname(struct irs_pr *this, const char *name)
 *
 */

static struct protoent *
pr_byname(struct irs_pr *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct protoent *pr = &pvt->proto;
	char *body = NULL;
	size_t bodylen;
	int code;
	int i;
	char text[256];

	if (pr->p_name != NULL && strcmp(name, pr->p_name) == 0) {
		return (pr);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	i = irs_irp_send_command(pvt->girpdata, "getprotobyname %s", name);
	if (i != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETPROTO_OK) {
		free_proto(pr);
		if (irp_unmarshall_pr(pr, body) != 0) {
			pr = NULL;
		}
	} else {
		pr = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (pr);
}



/*
 * struct protoent * pr_bynumber(struct irs_pr *this, int proto)
 *
 */

static struct protoent *
pr_bynumber(struct irs_pr *this, int proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct protoent *pr = &pvt->proto;
	char *body = NULL;
	size_t bodylen;
	int code;
	int i;
	char text[256];

	if (pr->p_name != NULL && proto == pr->p_proto) {
		return (pr);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	i = irs_irp_send_command(pvt->girpdata, "getprotobynumber %d", proto);
	if (i != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETPROTO_OK) {
		free_proto(pr);
		if (irp_unmarshall_pr(pr, body) != 0) {
			pr = NULL;
		}
	} else {
		pr = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (pr);
}




/*
 * void pr_rewind(struct irs_pr *this)
 *
 */

static void
pr_rewind(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char text[256];
	int code;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return;
	}

	if (irs_irp_send_command(pvt->girpdata, "setprotoent") != 0) {
		return;
	}

	code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
	if (code != IRPD_GETPROTO_SETOK) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "setprotoent failed: %s", text);
		}
	}

	return;
}




/*
 * struct protoent * pr_next(struct irs_pr *this)
 *
 * Notes:
 *
 *	Prepares the cache if necessary and returns the next item in it.
 *
 */

static struct protoent *
pr_next(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct protoent *pr = &pvt->proto;
	char *body;
	size_t bodylen;
	int code;
	char text[256];

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getprotoent") != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETPROTO_OK) {
		free_proto(pr);
		if (irp_unmarshall_pr(pr, body) != 0) {
			pr = NULL;
		}
	} else {
		pr = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (pr);
}




/*
 * void pr_minimize(struct irs_pr *this)
 *
 */

static void
pr_minimize(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	irs_irp_disconnect(pvt->girpdata);
}






/*
 * static void free_proto(struct protoent *pw);
 *
 *	Deallocate all the memory irp_unmarshall_pr allocated.
 *
 */

static void
free_proto(struct protoent *pr) {
	char **p;

	if (pr == NULL)
		return;

	if (pr->p_name != NULL)
		free(pr->p_name);

	for (p = pr->p_aliases ; p != NULL && *p != NULL ; p++)
		free(*p);
}
