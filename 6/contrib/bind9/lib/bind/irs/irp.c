/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
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

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: irp.c,v 1.3.2.1.10.5 2008/04/28 04:25:42 marka Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <syslog.h>
#include <ctype.h>
#include <unistd.h>

#include <isc/memcluster.h>

#include <irs.h>
#include <irp.h>

#include "irs_p.h"
#include "irp_p.h"

#include "port_after.h"

#ifdef VSPRINTF_CHAR
# define VSPRINTF(x) strlen(vsprintf/**/x)
#else
# define VSPRINTF(x) ((size_t)vsprintf x)
#endif

/* Forward. */

static void		irp_close(struct irs_acc *);

#define LINEINCR 128

#if !defined(SUN_LEN)
#define SUN_LEN(su) \
	(sizeof (*(su)) - sizeof ((su)->sun_path) + strlen((su)->sun_path))
#endif


/* Public */


/* send errors to syslog if true. */
int irp_log_errors = 1;

/*
 * This module handles the irp module connection to irpd.
 *
 * The client expects a synchronous interface to functions like
 * getpwnam(3), so we can't use the ctl_* i/o library on this end of
 * the wire (it's used in the server).
 */

/*
 * irs_acc *irs_irp_acc(const char *options);
 *
 *	Initialize the irp module.
 */
struct irs_acc *
irs_irp_acc(const char *options) {
	struct irs_acc *acc;
	struct irp_p *irp;

	UNUSED(options);

	if (!(acc = memget(sizeof *acc))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(acc, 0x5e, sizeof *acc);
	if (!(irp = memget(sizeof *irp))) {
		errno = ENOMEM;
		free(acc);
		return (NULL);
	}
	irp->inlast = 0;
	irp->incurr = 0;
	irp->fdCxn = -1;
	acc->private = irp;

#ifdef WANT_IRS_GR
	acc->gr_map = irs_irp_gr;
#else
	acc->gr_map = NULL;
#endif
#ifdef WANT_IRS_PW
	acc->pw_map = irs_irp_pw;
#else
	acc->pw_map = NULL;
#endif
	acc->sv_map = irs_irp_sv;
	acc->pr_map = irs_irp_pr;
	acc->ho_map = irs_irp_ho;
	acc->nw_map = irs_irp_nw;
	acc->ng_map = irs_irp_ng;
	acc->close = irp_close;
	return (acc);
}


int
irs_irp_connection_setup(struct irp_p *cxndata, int *warned) {
	if (irs_irp_is_connected(cxndata)) {
		return (0);
	} else if (irs_irp_connect(cxndata) != 0) {
		if (warned != NULL && !*warned) {
			syslog(LOG_ERR, "irpd connection failed: %m\n");
			(*warned)++;
		}

		return (-1);
	}

	return (0);
}


/*
 * int irs_irp_connect(void);
 *
 *	Sets up the connection to the remote irpd server.
 *
 * Returns:
 *
 *	0 on success, -1 on failure.
 *
 */
int
irs_irp_connect(struct irp_p *pvt) {
	int flags;
	struct sockaddr *addr;
	struct sockaddr_in iaddr;
#ifndef NO_SOCKADDR_UN
	struct sockaddr_un uaddr;
#endif
	long ipaddr;
	const char *irphost;
	int code;
	char text[256];
	int socklen = 0;

	if (pvt->fdCxn != -1) {
		perror("fd != 1");
		return (-1);
	}

#ifndef NO_SOCKADDR_UN
	memset(&uaddr, 0, sizeof uaddr);
#endif
	memset(&iaddr, 0, sizeof iaddr);

	irphost = getenv(IRPD_HOST_ENV);
	if (irphost == NULL) {
		irphost = "127.0.0.1";
	}

#ifndef NO_SOCKADDR_UN
	if (irphost[0] == '/') {
		addr = (struct sockaddr *)&uaddr;
		strncpy(uaddr.sun_path, irphost, sizeof uaddr.sun_path);
		uaddr.sun_family = AF_UNIX;
		socklen = SUN_LEN(&uaddr);
#ifdef HAVE_SA_LEN
		uaddr.sun_len = socklen;
#endif
	} else
#endif
	{
		if (inet_pton(AF_INET, irphost, &ipaddr) != 1) {
			errno = EADDRNOTAVAIL;
			perror("inet_pton");
			return (-1);
		}

		addr = (struct sockaddr *)&iaddr;
		socklen = sizeof iaddr;
#ifdef HAVE_SA_LEN
		iaddr.sin_len = socklen;
#endif
		iaddr.sin_family = AF_INET;
		iaddr.sin_port = htons(IRPD_PORT);
		iaddr.sin_addr.s_addr = ipaddr;
	}


	pvt->fdCxn = socket(addr->sa_family, SOCK_STREAM, PF_UNSPEC);
	if (pvt->fdCxn < 0) {
		perror("socket");
		return (-1);
	}

	if (connect(pvt->fdCxn, addr, socklen) != 0) {
		perror("connect");
		return (-1);
	}

	flags = fcntl(pvt->fdCxn, F_GETFL, 0);
	if (flags < 0) {
		close(pvt->fdCxn);
		perror("close");
		return (-1);
	}

#if 0
	flags |= O_NONBLOCK;
	if (fcntl(pvt->fdCxn, F_SETFL, flags) < 0) {
		close(pvt->fdCxn);
		perror("fcntl");
		return (-1);
	}
#endif

	code = irs_irp_read_response(pvt, text, sizeof text);
	if (code != IRPD_WELCOME_CODE) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "Connection failed: %s", text);
		}
		irs_irp_disconnect(pvt);
		return (-1);
	}

	return (0);
}



/*
 * int	irs_irp_is_connected(struct irp_p *pvt);
 *
 * Returns:
 *
 *	Non-zero if streams are setup to remote.
 *
 */

int
irs_irp_is_connected(struct irp_p *pvt) {
	return (pvt->fdCxn >= 0);
}



/*
 * void
 * irs_irp_disconnect(struct irp_p *pvt);
 *
 *	Closes streams to remote.
 */

void
irs_irp_disconnect(struct irp_p *pvt) {
	if (pvt->fdCxn != -1) {
		close(pvt->fdCxn);
		pvt->fdCxn = -1;
	}
}



int
irs_irp_read_line(struct irp_p *pvt, char *buffer, int len) {
	char *realstart = &pvt->inbuffer[0];
	char *p, *start, *end;
	int spare;
	int i;
	int buffpos = 0;
	int left = len - 1;

	while (left > 0) {
		start = p = &pvt->inbuffer[pvt->incurr];
		end = &pvt->inbuffer[pvt->inlast];

		while (p != end && *p != '\n')
			p++;

		if (p == end) {
			/* Found no newline so shift data down if necessary
			 * and append new data to buffer
			 */
			if (start > realstart) {
				memmove(realstart, start, end - start);
				pvt->inlast = end - start;
				start = realstart;
				pvt->incurr = 0;
				end = &pvt->inbuffer[pvt->inlast];
			}

			spare = sizeof (pvt->inbuffer) - pvt->inlast;

			p = end;
			i = read(pvt->fdCxn, end, spare);
			if (i < 0) {
				close(pvt->fdCxn);
				pvt->fdCxn = -1;
				return (buffpos > 0 ? buffpos : -1);
			} else if (i == 0) {
				return (buffpos);
			}

			end += i;
			pvt->inlast += i;

			while (p != end && *p != '\n')
				p++;
		}

		if (p == end) {
			/* full buffer and still no newline */
			i = sizeof pvt->inbuffer;
		} else {
			/* include newline */
			i = p - start + 1;
		}

		if (i > left)
			i = left;
		memcpy(buffer + buffpos, start, i);
		pvt->incurr += i;
		buffpos += i;
		buffer[buffpos] = '\0';

		if (p != end) {
			left = 0;
		} else {
			left -= i;
		}
	}

#if 0
	fprintf(stderr, "read line: %s\n", buffer);
#endif
	return (buffpos);
}





/*
 * int irp_read_response(struct irp_p *pvt);
 *
 * Returns:
 *
 *	The number found at the beginning of the line read from
 *	FP. 0 on failure(0 is not a legal response code). The
 *	rest of the line is discarded.
 *
 */

int
irs_irp_read_response(struct irp_p *pvt, char *text, size_t textlen) {
	char line[1024];
	int code;
	char *p;

	if (irs_irp_read_line(pvt, line, sizeof line) <= 0) {
		return (0);
	}

	p = strchr(line, '\n');
	if (p == NULL) {
		return (0);
	}

	if (sscanf(line, "%d", &code) != 1) {
		code = 0;
	} else if (text != NULL && textlen > 0U) {
		p = line;
		while (isspace((unsigned char)*p)) p++;
		while (isdigit((unsigned char)*p)) p++;
		while (isspace((unsigned char)*p)) p++;
		strncpy(text, p, textlen - 1);
		p[textlen - 1] = '\0';
	}

	return (code);
}



/*
 * char *irp_read_body(struct irp_p *pvt, size_t *size);
 *
 *	Read in the body of a response. Terminated by a line with
 *	just a dot on it. Lines should be terminated with a CR-LF
 *	sequence, but we're nt piccky if the CR is missing.
 *	No leading dot escaping is done as the protcol doesn't
 *	use leading dots anywhere.
 *
 * Returns:
 *
 *	Pointer to null-terminated buffer allocated by memget.
 *	*SIZE is set to the length of the buffer.
 *
 */

char *
irs_irp_read_body(struct irp_p *pvt, size_t *size) {
	char line[1024];
	u_int linelen;
	size_t len = LINEINCR;
	char *buffer = memget(len);
	int idx = 0;

	if (buffer == NULL)
		return (NULL);

	for (;;) {
		if (irs_irp_read_line(pvt, line, sizeof line) <= 0 ||
		    strchr(line, '\n') == NULL)
			goto death;

		linelen = strlen(line);

		if (line[linelen - 1] != '\n')
			goto death;

		/* We're not strict about missing \r. Should we be??  */
		if (linelen > 2 && line[linelen - 2] == '\r') {
			line[linelen - 2] = '\n';
			line[linelen - 1] = '\0';
			linelen--;
		}

		if (linelen == 2 && line[0] == '.') {
			*size = len;
			buffer[idx] = '\0';

			return (buffer);
		}

		if (linelen > (len - (idx + 1))) {
			char *p = memget(len + LINEINCR);

			if (p == NULL)
				goto death;
			memcpy(p, buffer, len);
			memput(buffer, len);
			buffer = p;
			len += LINEINCR;
		}

		memcpy(buffer + idx, line, linelen);
		idx += linelen;
	}
 death:
	memput(buffer, len);
	return (NULL);
}


/*
 * int irs_irp_get_full_response(struct irp_p *pvt, int *code,
 *			char **body, size_t *bodylen);
 *
 *	Gets the response to a command. If the response indicates
 *	there's a body to follow(code % 10 == 1), then the
 *	body buffer is allcoated with memget and stored in
 *	*BODY. The length of the allocated body buffer is stored
 *	in *BODY. The caller must give the body buffer back to
 *	memput when done. The results code is stored in *CODE.
 *
 * Returns:
 *
 *	0 if a result was read. -1 on some sort of failure.
 *
 */

int
irs_irp_get_full_response(struct irp_p *pvt, int *code, char *text,
			  size_t textlen, char **body, size_t *bodylen) {
	int result = irs_irp_read_response(pvt, text, textlen);

	*body = NULL;

	if (result == 0) {
		return (-1);
	}

	*code = result;

	/* Code that matches 2xx is a good result code.
	 * Code that matches xx1 means there's a response body coming.
	 */
	if ((result / 100) == 2 && (result % 10) == 1) {
		*body = irs_irp_read_body(pvt, bodylen);
		if (*body == NULL) {
			return (-1);
		}
	}

	return (0);
}


/*
 * int irs_irp_send_command(struct irp_p *pvt, const char *fmt, ...);
 *
 *	Sends command to remote connected via the PVT
 *	structure. FMT and args after it are fprintf-like
 *	arguments for formatting.
 *
 * Returns:
 *
 *	0 on success, -1 on failure.
 */

int
irs_irp_send_command(struct irp_p *pvt, const char *fmt, ...) {
	va_list ap;
	char buffer[1024];
	int pos = 0;
	int i, todo;


	if (pvt->fdCxn < 0) {
		return (-1);
	}

	va_start(ap, fmt);
	todo = VSPRINTF((buffer, fmt, ap));
	va_end(ap);
	if (todo > (int)sizeof(buffer) - 3) {
		syslog(LOG_CRIT, "memory overrun in irs_irp_send_command()");
		exit(1);
	}
	strcat(buffer, "\r\n");
	todo = strlen(buffer);

	while (todo > 0) {
		i = write(pvt->fdCxn, buffer + pos, todo);
#if 0
		/* XXX brister */
		fprintf(stderr, "Wrote: \"");
		fwrite(buffer + pos, sizeof (char), todo, stderr);
		fprintf(stderr, "\"\n");
#endif
		if (i < 0) {
			close(pvt->fdCxn);
			pvt->fdCxn = -1;
			return (-1);
		}
		todo -= i;
	}

	return (0);
}


/* Methods */



/*
 * void irp_close(struct irs_acc *this)
 *
 */

static void
irp_close(struct irs_acc *this) {
	struct irp_p *irp = (struct irp_p *)this->private;

	if (irp != NULL) {
		irs_irp_disconnect(irp);
		memput(irp, sizeof *irp);
	}

	memput(this, sizeof *this);
}



