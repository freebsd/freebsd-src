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
static const char rcsid[] = "$Id: irp_nw.c,v 1.1.206.2 2006/03/10 00:17:21 marka Exp $";
#endif /* LIBC_SCCS and not lint */

#if 0

#endif

/* Imports */

#include "port_before.h"

#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <irs.h>
#include <irp.h>
#include <isc/irpmarshall.h>

#include <isc/memcluster.h>
#include <isc/misc.h>

#include "irs_p.h"
#include "lcl_p.h"
#include "irp_p.h"

#include "port_after.h"

#define MAXALIASES 35
#define MAXADDRSIZE 4

struct pvt {
	struct irp_p	       *girpdata;
	int			warned;
	struct nwent		net;
};

/* Forward */

static void		nw_close(struct irs_nw *);
static struct nwent *	nw_byname(struct irs_nw *, const char *, int);
static struct nwent *	nw_byaddr(struct irs_nw *, void *, int, int);
static struct nwent *	nw_next(struct irs_nw *);
static void		nw_rewind(struct irs_nw *);
static void		nw_minimize(struct irs_nw *);

static void		free_nw(struct nwent *nw);


/* Public */



/*
 * struct irs_nw * irs_irp_nw(struct irs_acc *this) 
 *
 */

struct irs_nw *
irs_irp_nw(struct irs_acc *this) {
	struct irs_nw *nw;
	struct pvt *pvt;

	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);

	if (!(nw = memget(sizeof *nw))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(nw, 0x0, sizeof *nw);
	pvt->girpdata = this->private;

	nw->private = pvt;
	nw->close = nw_close;
	nw->byname = nw_byname;
	nw->byaddr = nw_byaddr;
	nw->next = nw_next;
	nw->rewind = nw_rewind;
	nw->minimize = nw_minimize;
	return (nw);
}

/* Methods */



/*
 * void nw_close(struct irs_nw *this) 
 *
 */

static void
nw_close(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	nw_minimize(this);

	free_nw(&pvt->net);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}




/*
 * struct nwent * nw_byaddr(struct irs_nw *this, void *net, 
 * 				int length, int type) 
 *
 */

static struct nwent *
nw_byaddr(struct irs_nw *this, void *net, int length, int type) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct nwent *nw = &pvt->net;
	char *body = NULL;
	size_t bodylen;
	int code;
	char paddr[24];			/* bigenough for ip4 w/ cidr spec. */
	char text[256];

	if (inet_net_ntop(type, net, length, paddr, sizeof paddr) == NULL) {
		return (NULL);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getnetbyaddr %s %s",
				 paddr, ADDR_T_STR(type)) != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETNET_OK) {
		free_nw(nw);
		if (irp_unmarshall_nw(nw, body) != 0) {
			nw = NULL;
		}
	} else {
		nw = NULL;
	}
		
	if (body != NULL) {
		memput(body, bodylen);
	}
	
	return (nw);
}




/*
 * struct nwent * nw_byname(struct irs_nw *this, const char *name, int type) 
 *
 */

static struct nwent *
nw_byname(struct irs_nw *this, const char *name, int type) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct nwent *nw = &pvt->net;
	char *body = NULL;
	size_t bodylen;
	int code;
	char text[256];

	if (nw->n_name != NULL &&
	    strcmp(name, nw->n_name) == 0 &&
	    nw->n_addrtype == type) {
		return (nw);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getnetbyname %s", name) != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETNET_OK) {
		free_nw(nw);
		if (irp_unmarshall_nw(nw, body) != 0) {
			nw = NULL;
		}
	} else {
		nw = NULL;
	}
	
	if (body != NULL) {
		memput(body, bodylen);
	}
	
	return (nw);
}




/*
 * void nw_rewind(struct irs_nw *this) 
 *
 */

static void
nw_rewind(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char text[256];
	int code;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return;
	}

	if (irs_irp_send_command(pvt->girpdata, "setnetent") != 0) {
		return;
	}

	code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
	if (code != IRPD_GETNET_SETOK) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "setnetent failed: %s", text);
		}
	}
	
	return;
}






/*
 * struct nwent * nw_next(struct irs_nw *this) 
 *
 * Notes:
 * 	
 * 	Prepares the cache if necessary and returns the first, or 
 * 	next item from it.
 */

static struct nwent *
nw_next(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct nwent *nw = &pvt->net;
	char *body;
	size_t bodylen;
	int code;
	char text[256];

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getnetent") != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETNET_OK) {
		free_nw(nw);
		if (irp_unmarshall_nw(nw, body) != 0) {
			nw = NULL;
		}
	} else {
		nw = NULL;
	}

	if (body != NULL)
		memput(body, bodylen);
	return (nw);
}






/*
 * void nw_minimize(struct irs_nw *this) 
 *
 */

static void
nw_minimize(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	irs_irp_disconnect(pvt->girpdata);
}




/* private. */



/*
 * static void free_passwd(struct passwd *pw);
 *
 *	deallocate all the memory irp_unmarshall_pw allocated.
 *
 */

static void
free_nw(struct nwent *nw) {
	char **p;

	if (nw == NULL)
		return;

	if (nw->n_name != NULL)
		free(nw->n_name);

	if (nw->n_aliases != NULL) {
		for (p = nw->n_aliases ; *p != NULL ; p++) {
			free(*p);
		}
		free(nw->n_aliases);
	}

	if (nw->n_addr != NULL)
		free(nw->n_addr);
}
