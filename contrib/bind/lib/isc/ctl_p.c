#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ctl_p.c,v 8.8 2001/05/29 05:49:26 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1998,1999 by Internet Software Consortium.
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

/* Extern. */

#include "port_before.h"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <isc/assertions.h>
#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/ctl.h>

#include "ctl_p.h"

#include "port_after.h"

/* Constants. */

const char * const ctl_sevnames[] = {
	"debug", "warning", "error"
};

/* Public. */

/*
 * ctl_logger()
 *	if ctl_startup()'s caller didn't specify a logger, this one
 *	is used.  this pollutes stderr with all kinds of trash so it will
 *	probably never be used in real applications.
 */
void
ctl_logger(enum ctl_severity severity, const char *format, ...) {
	va_list ap;
	static const char me[] = "ctl_logger";

	fprintf(stderr, "%s(%s): ", me, ctl_sevnames[severity]);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fputc('\n', stderr);
}

int
ctl_bufget(struct ctl_buf *buf, ctl_logfunc logger) {
	static const char me[] = "ctl_bufget";

	REQUIRE(!allocated_p(*buf) && buf->used == 0);
	buf->text = memget(MAX_LINELEN);
	if (!allocated_p(*buf)) {
		(*logger)(ctl_error, "%s: getmem: %s", me, strerror(errno));
		return (-1);
	}
	buf->used = 0;
	return (0);
}

void
ctl_bufput(struct ctl_buf *buf) {

	REQUIRE(allocated_p(*buf));
	memput(buf->text, MAX_LINELEN);
	buf->text = NULL;
	buf->used = 0;
}

const char *
ctl_sa_ntop(const struct sockaddr *sa,
	    char *buf, size_t size,
	    ctl_logfunc logger)
{
	static const char me[] = "ctl_sa_ntop";
	static const char punt[] = "[0].-1";
	char tmp[INET6_ADDRSTRLEN];

	switch (sa->sa_family) {
	case AF_INET6: {
		const struct sockaddr_in6 *in6 =
					(const struct sockaddr_in6 *) sa;

		if (inet_ntop(in6->sin6_family, &in6->sin6_addr, tmp, sizeof tmp)
		    == NULL) {
			(*logger)(ctl_error, "%s: inet_ntop(%u %04x): %s",
				  me, in6->sin6_family,
				  in6->sin6_port, strerror(errno));
			return (punt);
		}
		if (strlen(tmp) + sizeof "[].65535" > size) {
			(*logger)(ctl_error, "%s: buffer overflow", me);
			return (punt);
		}
		(void) sprintf(buf, "[%s].%u", tmp, ntohs(in6->sin6_port));
		return (buf);
	    }
	case AF_INET: {
		const struct sockaddr_in *in =
					      (const struct sockaddr_in *) sa;

		if (inet_ntop(in->sin_family, &in->sin_addr, tmp, sizeof tmp)
		    == NULL) {
			(*logger)(ctl_error, "%s: inet_ntop(%u %04x %08x): %s",
				  me, in->sin_family,
				  in->sin_port, in->sin_addr.s_addr,
				  strerror(errno));
			return (punt);
		}
		if (strlen(tmp) + sizeof "[].65535" > size) {
			(*logger)(ctl_error, "%s: buffer overflow", me);
			return (punt);
		}
		(void) sprintf(buf, "[%s].%u", tmp, ntohs(in->sin_port));
		return (buf);
	    }
#ifndef NO_SOCKADDR_UN
	case AF_UNIX: {
		const struct sockaddr_un *un = 
					      (const struct sockaddr_un *) sa;
		unsigned int x = sizeof un->sun_path;

		if (x > size)
			x = size;
		strncpy(buf, un->sun_path, x - 1);
		buf[x - 1] = '\0';
		return (buf);
	    }
#endif
	default:
		return (punt);
	}
}

void
ctl_sa_copy(const struct sockaddr *src, struct sockaddr *dst) {
	switch (src->sa_family) {
	case AF_INET6:
		*((struct sockaddr_in6 *)dst) =
					 *((const struct sockaddr_in6 *)src);
		break;
	case AF_INET:
		*((struct sockaddr_in *)dst) =
					  *((const struct sockaddr_in *)src);
		break;
#ifndef NO_SOCKADDR_UN
	case AF_UNIX:
		*((struct sockaddr_un *)dst) =
					  *((const struct sockaddr_un *)src);
		break;
#endif
	default:
		*dst = *src;
		break;
	}
}
