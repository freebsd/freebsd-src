/*
 * Portions Copyright(c) 1996, 1998 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: irp_gr.c,v 8.1 1999/01/18 07:46:53 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

/* extern */

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_gr_unneeded;
#else

#include <syslog.h>
#include <sys/param.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
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
#include "lcl_p.h"
#include "irp_p.h"

#include "port_after.h"


/* Types. */

/*
 * Module for the getnetgrent(3) family to use when connected to a
 * remote irp daemon.
 *
 * See irpd.c for justification of caching done here.
 *
 */

struct pvt {
	struct irp_p   *girpdata;	/* global IRP data */
	int		warned;
	struct group	group;
};

/* Forward. */

static void		gr_close(struct irs_gr *);
static struct group *	gr_next(struct irs_gr *);
static struct group *	gr_byname(struct irs_gr *, const char *);
static struct group *	gr_bygid(struct irs_gr *, gid_t);
static void		gr_rewind(struct irs_gr *);
static void		gr_minimize(struct irs_gr *);

/* Private */
static void		free_group(struct group *gr);


/* Public. */





/*
 * struct irs_gr * irs_irp_gr(struct irs_acc *this)
 *
 * Notes:
 *
 *	Initialize the group sub-module.
 *
 * Notes:
 *
 *	Module data.
 *
 */

struct irs_gr *
irs_irp_gr(struct irs_acc *this) {
	struct irs_gr *gr;
	struct pvt *pvt;

	if (!(gr = memget(sizeof *gr))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(gr, 0x0, sizeof *gr);

	if (!(pvt = memget(sizeof *pvt))) {
		memput(gr, sizeof *gr);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0x0, sizeof *pvt);
	pvt->girpdata = this->private;

	gr->private = pvt;
	gr->close = gr_close;
	gr->next = gr_next;
	gr->byname = gr_byname;
	gr->bygid = gr_bygid;
	gr->rewind = gr_rewind;
	gr->list = make_group_list;
	gr->minimize = gr_minimize;
	return (gr);
}

/* Methods. */



/*
 * void gr_close(struct irs_gr *this)
 *
 * Notes:
 *
 *	Close the sub-module.
 *
 */

static void
gr_close(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	gr_minimize(this);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}




/*
 * struct group * gr_next(struct irs_gr *this)
 *
 * Notes:
 *
 *	Gets the next group out of the cached data and returns it.
 *
 */

static struct group *
gr_next(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct group *gr = &pvt->group;
	char *body;
	size_t bodylen;
	int code;
	char text[256];

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getgrent") != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "getgrent failed: %s", text);
		}
		return (NULL);
	}

	if (code == IRPD_GETGROUP_OK) {
		free_group(gr);
		if (irp_unmarshall_gr(gr, body) != 0) {
			gr = NULL;
		}
	} else {
		gr = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (gr);
}





/*
 * struct group * gr_byname(struct irs_gr *this, const char *name)
 *
 * Notes:
 *
 *	Gets a group by name from irpd and returns it.
 *
 */

static struct group *
gr_byname(struct irs_gr *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct group *gr = &pvt->group;
	char *body;
	size_t bodylen;
	int code;
	char text[256];


	if (gr->gr_name != NULL && strcmp(name, gr->gr_name) == 0) {
		return (gr);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getgrnam %s", name) != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETGROUP_OK) {
		free_group(gr);
		if (irp_unmarshall_gr(gr, body) != 0) {
			gr = NULL;
		}
	} else {
		gr = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (gr);
}





/*
 * struct group * gr_bygid(struct irs_gr *this, gid_t gid)
 *
 * Notes:
 *
 *	Gets a group by gid from irpd and returns it.
 *
 */

static struct group *
gr_bygid(struct irs_gr *this, gid_t gid) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct group *gr = &pvt->group;
	char *body;
	size_t bodylen;
	int code;
	char text[256];

	if (gr->gr_name != NULL && gr->gr_gid == gid) {
		return (gr);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getgrgid %d", gid) != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETGROUP_OK) {
		free_group(gr);
		if (irp_unmarshall_gr(gr, body) != 0) {
			gr = NULL;
		}
	} else {
		gr = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (gr);
}




/*
 * void gr_rewind(struct irs_gr *this)
 *
 */

static void
gr_rewind(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char text[256];
	int code;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return;
	}

	if (irs_irp_send_command(pvt->girpdata, "setgrent") != 0) {
		return;
	}

	code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
	if (code != IRPD_GETGROUP_SETOK) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "setgrent failed: %s", text);
		}
	}

	return;
}




/*
 * void gr_minimize(struct irs_gr *this)
 *
 * Notes:
 *
 *	Frees up cached data and disconnects(if necessary) from the remote.
 *
 */

static void
gr_minimize(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	free_group(&pvt->group);
	irs_irp_disconnect(pvt->girpdata);
}

/* Private. */



/*
 * static void free_group(struct group *gr);
 *
 *	Deallocate all the memory irp_unmarshall_gr allocated.
 *
 */

static void
free_group(struct group *gr) {
	char **p;

	if (gr == NULL)
		return;

	if (gr->gr_name != NULL)
		free(gr->gr_name);

	if (gr->gr_passwd != NULL)
		free(gr->gr_passwd);

	for (p = gr->gr_mem ; p != NULL && *p != NULL ; p++)
		free(*p);

	if (p != NULL)
		free(p);
}


#endif /* WANT_IRS_GR */
