/*
 * Portions Copyright (c) 1996,1998 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: irp_ho.c,v 8.3 2001/05/29 05:48:59 marka Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports. */

#include "port_before.h"

#include <syslog.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <irs.h>
#include <irp.h>
#include <isc/irpmarshall.h>
#include <isc/memcluster.h>

#include "irs_p.h"
#include "dns_p.h"
#include "irp_p.h"

#include "port_after.h"

/* Definitions. */

#define	MAXALIASES	35
#define	MAXADDRS	35
#define	Max(a,b)	((a) > (b) ? (a) : (b))


struct pvt {
	struct irp_p	       *girpdata;
	int			warned;
	struct hostent		host;
};

/* Forward. */

static void		ho_close(struct irs_ho *this);
static struct hostent *	ho_byname(struct irs_ho *this, const char *name);
static struct hostent *	ho_byname2(struct irs_ho *this, const char *name,
				   int af);
static struct hostent *	ho_byaddr(struct irs_ho *this, const void *addr,
				  int len, int af);
static struct hostent *	ho_next(struct irs_ho *this);
static void		ho_rewind(struct irs_ho *this);
static void		ho_minimize(struct irs_ho *this);

static void		free_host(struct hostent *ho);
static struct addrinfo * ho_addrinfo(struct irs_ho *this, const char *name,
				     const struct addrinfo *pai);

/* Public. */



/*
 * struct irs_ho * irs_irp_ho(struct irs_acc *this)
 *
 * Notes:
 *
 *	Initializes the irp_ho module.
 *
 */

struct irs_ho *
irs_irp_ho(struct irs_acc *this) {
	struct irs_ho *ho;
	struct pvt *pvt;

	if (!(ho = memget(sizeof *ho))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(ho, 0x0, sizeof *ho);

	if (!(pvt = memget(sizeof *pvt))) {
		memput(ho, sizeof *ho);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->girpdata = this->private;

	ho->private = pvt;
	ho->close = ho_close;
	ho->byname = ho_byname;
	ho->byname2 = ho_byname2;
	ho->byaddr = ho_byaddr;
	ho->next = ho_next;
	ho->rewind = ho_rewind;
	ho->minimize = ho_minimize;
	ho->addrinfo = ho_addrinfo;

	return (ho);
}

/* Methods. */



/*
 * void ho_close(struct irs_ho *this)
 *
 * Notes:
 *
 *	Closes down the module.
 *
 */

static void
ho_close(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	ho_minimize(this);

	free_host(&pvt->host);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}



/*
 * struct hostent * ho_byname(struct irs_ho *this, const char *name)
 *
 */

static struct hostent *
ho_byname(struct irs_ho *this, const char *name) {
	return (ho_byname2(this, name, AF_INET));
}





/*
 * struct hostent * ho_byname2(struct irs_ho *this, const char *name, int af)
 *
 */

static struct hostent *
ho_byname2(struct irs_ho *this, const char *name, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *ho = &pvt->host;
	char *body = NULL;
	size_t bodylen;
	int code;
	char text[256];

	if (ho->h_name != NULL &&
	    strcmp(name, ho->h_name) == 0 &&
	    af == ho->h_addrtype) {
		return (ho);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "gethostbyname2 %s %s",
				 name, ADDR_T_STR(af)) != 0)
		return (NULL);

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETHOST_OK) {
		free_host(ho);
		if (irp_unmarshall_ho(ho, body) != 0) {
			ho = NULL;
		}
	} else {
		ho = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (ho);
}



/*
 * struct hostent * ho_byaddr(struct irs_ho *this, const void *addr,
 *			   int len, int af)
 *
 */

static struct hostent *
ho_byaddr(struct irs_ho *this, const void *addr, int len, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *ho = &pvt->host;
	char *body = NULL;
	size_t bodylen;
	int code;
	char **p;
	char paddr[MAXPADDRSIZE];
	char text[256];

	if (ho->h_name != NULL &&
	    af == ho->h_addrtype &&
	    len == ho->h_length) {
		for (p = ho->h_addr_list ; *p != NULL ; p++) {
			if (memcmp(*p, addr, len) == 0)
				return (ho);
		}
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (inet_ntop(af, addr, paddr, sizeof paddr) == NULL) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "gethostbyaddr %s %s",
				 paddr, ADDR_T_STR(af)) != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETHOST_OK) {
		free_host(ho);
		if (irp_unmarshall_ho(ho, body) != 0) {
			ho = NULL;
		}
	} else {
		ho = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (ho);
}





/*
 * struct hostent * ho_next(struct irs_ho *this)
 *
 * Notes:
 *
 *	The implementation for gethostent(3). The first time it's
 *	called all the data is pulled from the remote(i.e. what
 *	the maximum number of gethostent(3) calls would return)
 *	and that data is cached.
 *
 */

static struct hostent *
ho_next(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *ho = &pvt->host;
	char *body;
	size_t bodylen;
	int code;
	char text[256];

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "gethostent") != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETHOST_OK) {
		free_host(ho);
		if (irp_unmarshall_ho(ho, body) != 0) {
			ho = NULL;
		}
	} else {
		ho = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (ho);
}





/*
 * void ho_rewind(struct irs_ho *this)
 *
 */

static void
ho_rewind(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char text[256];
	int code;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return;
	}

	if (irs_irp_send_command(pvt->girpdata, "sethostent") != 0) {
		return;
	}

	code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
	if (code != IRPD_GETHOST_SETOK) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "sethostent failed: %s", text);
		}
	}

	return;
}




/*
 * void ho_minimize(struct irs_ho *this)
 *
 */

static void
ho_minimize(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	free_host(&pvt->host);

	irs_irp_disconnect(pvt->girpdata);
}




/*
 * void free_host(struct hostent *ho)
 *
 */

static void
free_host(struct hostent *ho) {
	char **p;

	if (ho == NULL) {
		return;
	}

	if (ho->h_name != NULL)
		free(ho->h_name);

	if (ho->h_aliases != NULL) {
		for (p = ho->h_aliases ; *p != NULL ; p++)
			free(*p);
		free(ho->h_aliases);
	}

	if (ho->h_addr_list != NULL) {
		for (p = ho->h_addr_list ; *p != NULL ; p++)
			free(*p);
		free(ho->h_addr_list);
	}
}

/* dummy */
static struct addrinfo *
ho_addrinfo(struct irs_ho *this, const char *name, const struct addrinfo *pai)
{
	UNUSED(this);
	UNUSED(name);
	UNUSED(pai);
	return(NULL);
}
