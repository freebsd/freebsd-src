/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999 by Internet Software Consortium.
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

/*
 * $Id: irp.h,v 1.3.18.1 2005-04-27 05:00:49 sra Exp $
 */

#ifndef _IRP_H_INCLUDED
#define _IRP_H_INCLUDED

/*! \file */

#define IRPD_TIMEOUT 30			/*%< seconds */
#define IRPD_MAXSESS 50			/*%< number of simultaneous sessions. */
#define IRPD_PORT 6660			/*%< 10 times the number of the beast. */
#define IRPD_PATH "/var/run/irpd"	/*%< af_unix socket path */

/* If sets the environment variable IRPDSERVER to an IP address
   (e.g. "192.5.5.1"), then that's the host the client expects irpd to be
   running on. */
#define IRPD_HOST_ENV "IRPDSERVER"

/* Protocol response codes.  */
#define IRPD_WELCOME_CODE 200
#define IRPD_NOT_WELCOME_CODE 500

#define IRPD_GETHOST_ERROR 510
#define IRPD_GETHOST_NONE 210
#define IRPD_GETHOST_OK 211
#define IRPD_GETHOST_SETOK 212

#define IRPD_GETNET_ERROR 520
#define IRPD_GETNET_NONE 220
#define IRPD_GETNET_OK 221
#define IRPD_GETNET_SETOK 222

#define IRPD_GETUSER_ERROR 530
#define IRPD_GETUSER_NONE 230
#define IRPD_GETUSER_OK 231
#define IRPD_GETUSER_SETOK 232

#define IRPD_GETGROUP_ERROR 540
#define IRPD_GETGROUP_NONE 240
#define IRPD_GETGROUP_OK 241
#define IRPD_GETGROUP_SETOK 242

#define IRPD_GETSERVICE_ERROR 550
#define IRPD_GETSERVICE_NONE 250
#define IRPD_GETSERVICE_OK 251
#define IRPD_GETSERVICE_SETOK 252

#define IRPD_GETPROTO_ERROR 560
#define IRPD_GETPROTO_NONE 260
#define IRPD_GETPROTO_OK 261
#define IRPD_GETPROTO_SETOK 262

#define IRPD_GETNETGR_ERROR 570
#define IRPD_GETNETGR_NONE 270
#define IRPD_GETNETGR_OK 271
#define IRPD_GETNETGR_NOMORE 272
#define IRPD_GETNETGR_MATCHES 273
#define IRPD_GETNETGR_NOMATCH 274
#define IRPD_GETNETGR_SETOK 275
#define IRPD_GETNETGR_SETERR 276

#define	irs_irp_read_body __irs_irp_read_body
#define irs_irp_read_response __irs_irp_read_response
#define irs_irp_disconnect __irs_irp_disconnect
#define irs_irp_connect __irs_irp_connect
#define irs_irp_connection_setup __irs_irp_connection_setup
#define irs_irp_send_command __irs_irp_send_command

struct irp_p;

char   *irs_irp_read_body(struct irp_p *, size_t *);
int	irs_irp_read_response(struct irp_p *, char *, size_t);
void	irs_irp_disconnect(struct irp_p *);
int	irs_irp_connect(struct irp_p *);
int	irs_irp_is_connected(struct irp_p *);
int	irs_irp_connection_setup(struct irp_p *, int *);
#ifdef __GNUC__
int	irs_irp_send_command(struct irp_p *, const char *, ...)
			     __attribute__((__format__(__printf__, 2, 3)));
#else
int	irs_irp_send_command(struct irp_p *, const char *, ...);
#endif
int	irs_irp_get_full_response(struct irp_p *, int *, char *, size_t,
				  char **, size_t *);
int	irs_irp_read_line(struct irp_p *, char *, int);

#endif

/*! \file */
