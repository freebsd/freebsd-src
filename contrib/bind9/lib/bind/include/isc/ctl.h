#ifndef ISC_CTL_H
#define ISC_CTL_H

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998,1999 by Internet Software Consortium.
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
 * $Id: ctl.h,v 1.1.2.2.4.1 2004/03/09 08:33:30 marka Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <isc/eventlib.h>

/* Macros. */

#define	CTL_MORE	0x0001	/* More will be / should be sent. */
#define	CTL_EXIT	0x0002	/* Close connection after this. */
#define	CTL_DATA	0x0004	/* Go into / this is DATA mode. */

/* Types. */

struct ctl_cctx;
struct ctl_sctx;
struct ctl_sess;
struct ctl_verb;

enum ctl_severity { ctl_debug, ctl_warning, ctl_error };

typedef void (*ctl_logfunc)(enum ctl_severity, const char *, ...);

typedef void (*ctl_verbfunc)(struct ctl_sctx *, struct ctl_sess *,
			     const struct ctl_verb *, const char *,
			     u_int, const void *, void *);

typedef void (*ctl_srvrdone)(struct ctl_sctx *, struct ctl_sess *, void *);

typedef void (*ctl_clntdone)(struct ctl_cctx *, void *, const char *, u_int);

struct ctl_verb {
	const char *	name;
	ctl_verbfunc	func;
	const char *	help;
};

/* General symbols. */

#define	ctl_logger	__ctl_logger

#ifdef __GNUC__
void			ctl_logger(enum ctl_severity, const char *, ...)
				__attribute__((__format__(__printf__, 2, 3)));
#else
void			ctl_logger(enum ctl_severity, const char *, ...);
#endif

/* Client symbols. */

#define	ctl_client	__ctl_client
#define	ctl_endclient	__ctl_endclient
#define	ctl_command	__ctl_command

struct ctl_cctx *	ctl_client(evContext, const struct sockaddr *, size_t,
				   const struct sockaddr *, size_t,
				   ctl_clntdone, void *,
				   u_int, ctl_logfunc);
void			ctl_endclient(struct ctl_cctx *);
int			ctl_command(struct ctl_cctx *, const char *, size_t,
				    ctl_clntdone, void *);

/* Server symbols. */

#define	ctl_server	__ctl_server
#define	ctl_endserver	__ctl_endserver
#define	ctl_response	__ctl_response
#define	ctl_sendhelp	__ctl_sendhelp
#define ctl_getcsctx	__ctl_getcsctx
#define ctl_setcsctx	__ctl_setcsctx

struct ctl_sctx *	ctl_server(evContext, const struct sockaddr *, size_t,
				   const struct ctl_verb *,
				   u_int, u_int,
				   u_int, int, int,
				   ctl_logfunc, void *);
void			ctl_endserver(struct ctl_sctx *);
void			ctl_response(struct ctl_sess *, u_int,
				     const char *, u_int, const void *,
				     ctl_srvrdone, void *,
				     const char *, size_t);
void			ctl_sendhelp(struct ctl_sess *, u_int);
void *			ctl_getcsctx(struct ctl_sess *);
void *			ctl_setcsctx(struct ctl_sess *, void *);

#endif /*ISC_CTL_H*/
