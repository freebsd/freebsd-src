/*
 * Copyright(c) 1999 by Internet Software Consortium.
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

/* Notes. */

#if 0

I have to use an AF_INET. Ctl_server should probably take a AF arugment.

The server has no way to issue any other greeting than HELLO. E.g., would
like to be able to drop connection on greeting if client is not comming
from 127.0.0.1.

Need to fix client to handle response with body.

should add iovec with body to the struct ctl_sess?

should we close connections on some errors (like marshalling errors)?

getnetbyname falls back to /etc/networks when named not running. Does not
seem to be so for getnetbyaddr

#endif

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: irpd.c,v 1.10 2000/12/23 08:14:33 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <assert.h>
#include <ctype.h>
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <pwd.h>
#include <resolv.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <utmp.h>

#ifdef EMPTY
/* Digital UNIX utmp.h defines this. */
#undef EMPTY
#endif

#include <isc/ctl.h>
#include <isc/assertions.h>
#include <isc/list.h>
#include <isc/memcluster.h>
#include <isc/logging.h>

#include <irs.h>
#include <irp.h>
#include <isc/irpmarshall.h>
#include <irs_data.h>

#include "port_after.h"

/* Macros. */

#define ALLDIGITS(s) (strspn((s), "0123456789") == strlen((s)))

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

#define MAXNETNAMELEN 256

#if !defined(SUN_LEN)
#define SUN_LEN(su) \
	(sizeof (*(su)) - sizeof ((su)->sun_path) + strlen((su)->sun_path))
#endif

/*
 * This macro is used to initialize a specified field of a net_data struct.
 * If the initialization fails then an error response code is sent with a
 * description of which field failed to be initialized.
 *
 * This is only meant for use at the start of the various verb functions.
 */

#define ND_INIT(nd, field, sess, respcode)				     \
	do{ if ((nd)->field == 0) {					     \
		(nd)->field = (*(nd)->irs->field ## _map)(nd->irs);	     \
	        if ((nd)->field == 0) {					     \
		    char *msg = "net_data " #field " initialization failed"; \
		    ctl_response(sess, respcode, msg, CTL_EXIT, NULL,	     \
			         NULL, NULL, NULL, 0);			     \
		    return;						     \
                }							     \
	    }								     \
 	} while (0)

/* Data structures. */

struct arg_s  {
	struct iovec *		iov;
	int			iovlen;
};

struct response_buff {
	char *			buff;
	size_t			bufflen;
};

struct client_ctx {
	struct net_data *	net_data;
};

/* Forwards. */

static struct response_buff *newbuffer(u_int length);
static void release_buffer(struct response_buff *b);
static struct arg_s *split_string(const char *string);
static void free_args(struct arg_s *args);
static struct client_ctx *make_cli_ctx(void);
static struct net_data *get_net_data(struct ctl_sess *sess);

static void irpd_gethostbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
			       const struct ctl_verb *verb, const char *rest,
			       u_int respflags, void *respctx, void *uctx);
static void irpd_gethostbyname2(struct ctl_sctx *ctx, struct ctl_sess *sess,
				const struct ctl_verb *verb, const char *rest,
				u_int respflags, void *respctx, void *uctx);
static void irpd_gethostbyaddr(struct ctl_sctx *ctx, struct ctl_sess *sess,
			       const struct ctl_verb *verb, const char *rest,
			       u_int respflags, void *respctx, void *uctx);
static void irpd_gethostent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			    const struct ctl_verb *verb, const char *rest,
			    u_int respflags, void *respctx, void *uctx);
static void irpd_sethostent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			    const struct ctl_verb *verb, const char *rest,
			    u_int respflags, void *respctx, void *uctx);
static void irpd_getpwnam(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_getpwuid(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_getpwent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_setpwent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_getnetbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
			      const struct ctl_verb *verb, const char *rest,
			      u_int respflags, void *respctx, void *uctx);
static void irpd_getnetbyaddr(struct ctl_sctx *ctx, struct ctl_sess *sess,
			      const struct ctl_verb *verb, const char *rest,
			      u_int respflags, void *respctx, void *uctx);
static void irpd_getnetent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			   const struct ctl_verb *verb, const char *rest,
			   u_int respflags, void *respctx, void *uctx);
static void irpd_setnetent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			   const struct ctl_verb *verb, const char *rest,
			   u_int respflags, void *respctx, void *uctx);
static void irpd_getgrnam(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_getgrgid(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_getgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_setgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  const struct ctl_verb *verb, const char *rest,
			  u_int respflags, void *respctx, void *uctx);
static void irpd_getservbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
			       const struct ctl_verb *verb, const char *rest,
			       u_int respflags, void *respctx, void *uctx);
static void irpd_getservbyport(struct ctl_sctx *ctx, struct ctl_sess *sess,
			       const struct ctl_verb *verb, const char *rest,
			       u_int respflags, void *respctx, void *uctx);
static void irpd_getservent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			    const struct ctl_verb *verb, const char *rest,
			    u_int respflags, void *respctx, void *uctx);
static void irpd_setservent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			    const struct ctl_verb *verb, const char *rest,
			    u_int respflags, void *respctx, void *uctx);
static void irpd_getprotobyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
				const struct ctl_verb *verb, const char *rest,
				u_int respflags, void *respctx, void *uctx);
static void irpd_getprotobynumber(struct ctl_sctx *ctx, struct ctl_sess *sess,
				  const struct ctl_verb *verb, const char *rest,
				  u_int respflags, void *respctx, void *uctx);
static void irpd_getprotoent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			     const struct ctl_verb *verb, const char *rest,
			     u_int respflags, void *respctx, void *uctx);
static void irpd_setprotoent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			     const struct ctl_verb *verb, const char *rest,
			     u_int respflags, void *respctx, void *uctx);
static void irpd_getnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			     const struct ctl_verb *verb, const char *rest,
			     u_int respflags, void *respctx, void *uctx);
static void irpd_innetgr(struct ctl_sctx *ctx, struct ctl_sess *sess,
			 const struct ctl_verb *verb, const char *rest,
			 u_int respflags, void *respctx, void *uctx);
static void irpd_setnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			     const struct ctl_verb *verb, const char *rest,
			     u_int respflags, void *respctx, void *uctx);
static void irpd_endnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
			     const struct ctl_verb *verb, const char *rest,
			     u_int respflags, void *respctx, void *uctx);
static void irpd_quit(struct ctl_sctx *ctx, struct ctl_sess *sess,
		      const struct ctl_verb *verb, const char *rest,
		      u_int respflags, void *respctx, void *uctx);
static void irpd_help(struct ctl_sctx *ctx, struct ctl_sess *sess,
		      const struct ctl_verb *verb, const char *rest,
		      u_int respflags, void *respctx, void *uctx);
static void irpd_accept(struct ctl_sctx *ctx, struct ctl_sess *sess,
			const struct ctl_verb *verb, const char *rest,
			u_int respflags, void *respctx, void *uctx);
static void irpd_abort(struct ctl_sctx *ctx, struct ctl_sess *sess,
		       const struct ctl_verb *verb, const char *rest,
		       u_int respflags, void *respctx, void *uctx);

static void response_done(struct ctl_sctx *ctx, struct ctl_sess *sess,
			  void *uap);
static void logger(enum ctl_severity, const char *fmt, ...);

/* Constants. */

static const u_int hello_code = IRPD_WELCOME_CODE;
static const char hello_msg[] = "Welcome to IRPD (v 1)";
static const u_int unkncode = 500;
static const u_int timeoutcode = 501;
static const u_int irpd_quit_ok = 201;
static const u_int timeout = IRPD_TIMEOUT;

/* Globals. */

static int main_needs_exit = 0;
static evContext ev;

struct ctl_verb verbs [] = {
	{ "gethostbyname", irpd_gethostbyname },
	{ "gethostbyname2", irpd_gethostbyname2 },
	{ "gethostbyaddr", irpd_gethostbyaddr },
	{ "gethostent", irpd_gethostent },
	{ "sethostent", irpd_sethostent },
#ifdef WANT_IRS_PW
	{ "getpwnam", irpd_getpwnam },
	{ "getpwuid", irpd_getpwuid },
	{ "getpwent", irpd_getpwent },
	{ "setpwent", irpd_setpwent },
#endif
	{ "getnetbyname", irpd_getnetbyname },
	{ "getnetbyaddr", irpd_getnetbyaddr },
	{ "getnetent", irpd_getnetent },
	{ "setnetent", irpd_setnetent },
#ifdef WANT_IRS_GR
	{ "getgrnam", irpd_getgrnam },
	{ "getgrgid", irpd_getgrgid },
	{ "getgrent", irpd_getgrent },
	{ "setgrent", irpd_setgrent },
#endif
	{ "getservbyname", irpd_getservbyname },
	{ "getservbyport", irpd_getservbyport },
	{ "getservent", irpd_getservent },
	{ "setservent", irpd_setservent },

	{ "getprotobyname", irpd_getprotobyname },
	{ "getprotobynumber", irpd_getprotobynumber },
	{ "getprotoent", irpd_getprotoent },
	{ "setprotoent", irpd_setprotoent },

	{ "getnetgrent", irpd_getnetgrent },
	{ "innetgr", irpd_innetgr },
	{ "setnetgrent", irpd_setnetgrent },
	{ "endnetgrent", irpd_endnetgrent },
	{ "quit", irpd_quit },
	{ "help", irpd_help },

	{ "", irpd_accept },	/* For connection setups. */

	/* abort is a verb expected by the ctl library. Is called when the
	 * client drops the connection unexpectedly.
	 */
	{ "abort", irpd_abort },

	{ NULL, NULL }
};

/*
 * An empty string causes the library to use the compiled in
 * defaults and to ignore any external files.
 */
char *conffile = "";			

/* Public. */

int
main(int argc, char **argv) {
	struct ctl_sctx *ctx;
	struct sockaddr *addr;
#ifndef NO_SOCKADDR_UN
	struct sockaddr_un uaddr;
#endif
	struct sockaddr_in iaddr;
	short port = IRPD_PORT;
	char *prog = argv[0];
	char *sockname = IRPD_PATH;
	char *p;
	int ch;
	size_t socksize;

	addr = (struct sockaddr *)&iaddr;
	socksize = sizeof iaddr;

	openlog("iprd", LOG_CONS|LOG_PID, ISC_FACILITY);
	while ((ch = getopt(argc, argv, "u:p:c:")) != -1) {
		switch(ch) {
		case 'c':
			conffile = optarg;
			break;

		case 'p':
			port = strtol(optarg, &p, 10);
			if (*p != '\0') {
				/* junk in argument */
				syslog(LOG_ERR, "port option not a number");
				exit(1);
			}
			break;

#ifndef NO_SOCKADDR_UN
		case 'u':
			sockname = optarg;
			addr = (struct sockaddr *)&uaddr;
			socksize = sizeof uaddr;
			break;
#endif

		case 'h':
		case '?':
		default:
			fprintf(stderr, "%s [ -c config-file ]\n", prog);
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	memset(&iaddr, 0, sizeof iaddr);

#ifdef HAVE_SA_LEN
	iaddr.sin_len = sizeof iaddr;
#endif
	iaddr.sin_family = AF_INET;
	iaddr.sin_port = htons(IRPD_PORT);
	iaddr.sin_addr.s_addr = htonl(INADDR_ANY);

#ifndef NO_SOCKADDR_UN
	memset(&uaddr, 0, sizeof uaddr);
	if (addr == (struct sockaddr *)&uaddr) {
		uaddr.sun_family = AF_UNIX;
		strncpy(uaddr.sun_path, sockname, sizeof uaddr.sun_path);
#ifdef HAVE_SA_LEN
		uaddr.sun_len = SUN_LEN(&uaddr);
#endif

		socksize = SUN_LEN(&uaddr);

		/* XXX what if this file is not currently a socket? */
		unlink(sockname);
	}
#endif

	evCreate(&ev);

	ctx = ctl_server(ev, addr, socksize, verbs,
			 unkncode, timeoutcode, /* IRPD_TIMEOUT */ 30, 5,
			 IRPD_MAXSESS, logger, NULL);
	
	INSIST(ctx != NULL);

	while (!main_needs_exit) {
		evEvent event;

		INSIST_ERR(evGetNext(ev, &event, EV_WAIT) != -1);
		INSIST_ERR(evDispatch(ev, event) != -1);
	}

	return (0);
}


/*
 * static void
 * simple_response(struct ctl_sess *sess, u_int code, char *msg);
 *	Send back a simple, one-line response to the client.
 */
static void
simple_response(struct ctl_sess *sess, u_int code, char *msg) {
	struct response_buff *b = newbuffer(strlen(msg) + 1);

	if (b == 0)
		return;
	strcpy(b->buff, msg);
	ctl_response(sess, code, b->buff, 0, 0,	 response_done, b, NULL, 0);
}

/*
 * static void
 * send_hostent(struct ctl_sess *sess, struct hostent *ho);
 *	Send a hostent struct over the wire. If HO is NULL, then
 *	a "No such host" is sent instead.
 */
static void
send_hostent(struct ctl_sess *sess, struct hostent *ho) {
	if (ho == NULL)
		simple_response(sess, IRPD_GETHOST_NONE, "No such host");
	else {
		struct response_buff *b = newbuffer(0);

		if (irp_marshall_ho(ho, &b->buff, &b->bufflen) != 0) {
			simple_response(sess, IRPD_GETHOST_ERROR,
					"Internal error");
			logger(ctl_warning,
			       "Cannot marshall host data for %s\n",
			       ho->h_name);
			release_buffer(b);
		} else {
			strcat(b->buff, "\r\n");

			ctl_response(sess, IRPD_GETHOST_OK, "Host found",
				     0, 0, response_done,
				     b, b->buff, strlen(b->buff));
		}
	}
}

/*
 * static void
 * do_gethostbyname2(struct ctl_sess *sess, struct net_data *nd,
 *		     const char *hostname, int af);
 *	Look up the given HOSTNAME by Address-Family
 *	and then send the results to the client connected to
 *	SESS.
 */
static void
do_gethostbyname2(struct ctl_sess *sess, struct net_data *nd,
		  const char *hostname, int af)
{
	struct hostent *ho;

	ho = gethostbyname2_p(hostname, af, nd);
	send_hostent(sess, ho);
}

/*
 * static void
 * irpd_gethostbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		      const struct ctl_verb *verb, const char *rest,
 *		      u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETHOSTBYNAME verb.
 */
static void
irpd_gethostbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
		   const struct ctl_verb *verb, const char *rest,
		   u_int respflags, void *respctx, void *uctx)
{
	char hname[MAXHOSTNAMELEN];
	struct arg_s *args;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ho, sess, IRPD_GETHOST_ERROR);
	
	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETHOST_ERROR,
				"Incorrect usage: GETHOSTBYNAME hostname");
	} else {
		if (args->iov[0].iov_len >= sizeof hname) {
			simple_response(sess, IRPD_GETHOST_ERROR,
					"GETHOSTBYNAME: name too long");
		} else {
			strncpy(hname, args->iov[0].iov_base,
				args->iov[0].iov_len);
			hname[args->iov[0].iov_len] = '\0';
			do_gethostbyname2(sess, netdata, hname, AF_INET);
		}
	}
	free_args(args);
}

/*
 * static void
 * irpd_gethostbyname2(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		       const struct ctl_verb *verb, const char *rest,
 *		       u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETHOSTBYNAME2 verb.
 */
static void
irpd_gethostbyname2(struct ctl_sctx *ctx, struct ctl_sess *sess,
		    const struct ctl_verb *verb, const char *rest,
		    u_int respflags, void *respctx, void *uctx)
{
	char hname[MAXHOSTNAMELEN];
	struct arg_s *args;
	int af;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ho, sess, IRPD_GETHOST_ERROR);
	
	args = split_string(rest);
	if (args->iovlen != 3) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETHOST_ERROR,
				"Incorrect usage: GETHOSTBYNAME2 hostname AF");
	} else if (args->iov[0].iov_len >= sizeof hname) {
		simple_response(sess, IRPD_GETHOST_ERROR,
				"GETHOSTBYNAME2: name too long");
	} else {
		if (strncasecmp(args->iov[1].iov_base, "af_inet6", 8) == 0)
			af = AF_INET6;
		else if (strncasecmp(args->iov[1].iov_base, "af_inet", 7) == 0)
			af = AF_INET;
		else {
			simple_response(sess, IRPD_GETHOST_ERROR,
					"Unknown address family");
			goto untimely;
		}

		strncpy(hname, args->iov[0].iov_base,
			args->iov[0].iov_len);
		hname[args->iov[0].iov_len] = '\0';
		do_gethostbyname2(sess, netdata, hname, af);
	}

 untimely:
	free_args(args);
}

/*
 * static void
 * irpd_gethostbyaddr(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		      const struct ctl_verb *verb, const char *rest,
 *		      u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETHOSTBYADDR verb.
 */
static void
irpd_gethostbyaddr(struct ctl_sctx *ctx, struct ctl_sess *sess,
		   const struct ctl_verb *verb, const char *rest,
		   u_int respflags, void *respctx, void *uctx)
{
	struct hostent *ho;
	char haddr[MAXHOSTNAMELEN];
	char tmpaddr[NS_IN6ADDRSZ];
	struct arg_s *args;
	int af;
	int addrlen;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ho, sess, IRPD_GETHOST_ERROR);

	args = split_string(rest);
	if (args->iovlen != 3) {
		simple_response(sess, IRPD_GETHOST_ERROR,
				"GETHOSTBYADDR addr afamily");
	} else {
		if (args->iov[0].iov_len >= sizeof haddr) {
			simple_response(sess, IRPD_GETHOST_ERROR,
					"Address too long");
		} else {
			strncpy(haddr, args->iov[1].iov_base,
				args->iov[1].iov_len);
			haddr[args->iov[1].iov_len] = '\0';
			if (strcasecmp(haddr, "af_inet") == 0) {
				af = AF_INET;
				addrlen = NS_INADDRSZ;
			} else if (strcasecmp(haddr, "af_inet6") == 0) {
				af = AF_INET6;
				addrlen = NS_IN6ADDRSZ;
			} else {
				simple_response(sess, IRPD_GETHOST_ERROR,
					      "Unknown address family");
				goto untimely;
			}

			strncpy(haddr, args->iov[0].iov_base,
				args->iov[0].iov_len);
			haddr[args->iov[0].iov_len] = '\0';

			if (inet_pton(af, haddr, tmpaddr) != 1) {
				simple_response(sess, IRPD_GETHOST_ERROR,
					     "Invalid address");
				goto untimely;
			}

			ho = gethostbyaddr_p(tmpaddr, addrlen, af, netdata);
			send_hostent(sess, ho);
		}
	}

 untimely:
	free_args(args);
}


/*
 * static void
 * irpd_gethostent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		   const struct ctl_verb *verb, const char *rest,
 *		   u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETHOSTENT verb
 */
static void
irpd_gethostent(struct ctl_sctx *ctx, struct ctl_sess *sess,
		const struct ctl_verb *verb, const char *rest,
		u_int respflags, void *respctx, void *uctx)
{
	struct hostent *ho;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ho, sess, IRPD_GETHOST_ERROR);

	ho = gethostent_p(netdata);

	send_hostent(sess, ho);
}

/*
 * static void
 * irpd_sethostent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		   const struct ctl_verb *verb, const char *rest,
 *		   u_int respflags, void *respctx, void *uctx);
 *	Implementation of the SETHOSTENT verb
 */
static void
irpd_sethostent(struct ctl_sctx *ctx, struct ctl_sess *sess,
		const struct ctl_verb *verb, const char *rest,
		u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ho, sess, IRPD_GETHOST_ERROR);

	sethostent_p(1, netdata);	/* always stayopen */
	simple_response(sess, IRPD_GETHOST_SETOK, "ok");
}

#ifdef WANT_IRS_PW
/*
 * static void
 * send_pwent(struct ctl_sess *sess, struct passwd *pw);
 *	Send PW over the wire, or, if PW is NULL, a "No such
 *	user" response.
 */
static void
send_pwent(struct ctl_sess *sess, struct passwd *pw) {
	if (pw == NULL) {
		simple_response(sess, IRPD_GETUSER_NONE,
				"No such user");
	} else {
		struct response_buff *b = newbuffer(0);

		if (irp_marshall_pw(pw, &b->buff,
				    &b->bufflen) != 0) {
			simple_response(sess, IRPD_GETUSER_ERROR,
					"Internal error");
			logger(ctl_warning, "Cant marshall pw\n");
			return;
		}

		strcat(b->buff, "\r\n");

		ctl_response(sess, IRPD_GETUSER_OK, "User found", 0, 0,
			     response_done, b, b->buff, strlen(b->buff));
	}
}

/*
 * static void
 * irpd_getpwnam(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETPWNAM verb
 */
static void
irpd_getpwnam(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct passwd *pw;
	char username[64];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pw, sess, IRPD_GETUSER_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETUSER_ERROR,
				"GETPWNAM username");
	} else {
		if (args->iov[0].iov_len >= sizeof username) {
			simple_response(sess, IRPD_GETUSER_ERROR,
					"Name too long");
		} else {
			strncpy(username, args->iov[0].iov_base,
				args->iov[0].iov_len);
			username[args->iov[0].iov_len] = '\0';

			pw = getpwnam_p(username, netdata);
			send_pwent(sess, pw);
		}
	}

	free_args(args);
}

/*
 * static void
 * irpd_getpwuid(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETPWUID verb.
 */
static void
irpd_getpwuid(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct passwd *pw;
	char userid[64];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pw, sess, IRPD_GETUSER_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETUSER_ERROR,
				"GETPWUID uid");
	} else {
		if (args->iov[0].iov_len >= sizeof userid) {
			simple_response(sess, IRPD_GETUSER_ERROR,
					"Name too long");
		} else {
			strncpy(userid, args->iov[0].iov_base,
				args->iov[0].iov_len);
			userid[args->iov[0].iov_len] = '\0';

			if (!ALLDIGITS(userid)) {
				simple_response(sess, IRPD_GETUSER_ERROR,
						"Not a uid");
			} else {
				uid_t uid;
				long lval;

				lval = strtol(userid, 0, 10);
				uid = (uid_t)lval;
				if ((long)uid != lval) {
					/* value was too big */
					simple_response(sess,
							IRPD_GETUSER_ERROR,
							"Not a valid uid");
					goto untimely;
				}

				pw = getpwuid_p(uid, netdata);
				send_pwent(sess, pw);
			}
		}
	}

  untimely:
	free_args(args);
}

/*
 * static void
 * irpd_getpwent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implemtnation of the GETPWENT verb. 
 */
static void
irpd_getpwent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct passwd *pw;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pw, sess, IRPD_GETUSER_ERROR);

	pw = getpwent_p(netdata);
	send_pwent(sess, pw);
}

/*
 * static void
 * irpd_setpwent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implemtnation of the SETPWENT verb.
 */
static void
irpd_setpwent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pw, sess, IRPD_GETUSER_ERROR);

	setpwent_p(netdata);
	simple_response(sess, IRPD_GETUSER_SETOK, "ok");
}
#endif /* WANT_IRS_PW */

/*
 * static void
 * send_nwent(struct ctl_sess *sess, struct nwent *ne);
 *	Sends a nwent structure over the wire, or "No such
 *	network" if NE is NULL.
 */
static void
send_nwent(struct ctl_sess *sess, struct nwent *nw) {
	if (nw == NULL) {
		simple_response(sess, IRPD_GETNET_NONE, "No such net");
	} else {
		struct response_buff *b = newbuffer(0);

		if (irp_marshall_nw(nw, &b->buff,
				    &b->bufflen) != 0) {
			simple_response(sess, IRPD_GETNET_ERROR,
					"Internal error");
			logger(ctl_warning, "Cant marshall nw\n");
			return;
		}

		strcat(b->buff, "\r\n");

		ctl_response(sess, IRPD_GETNET_OK, "Network found", 0, 0,
			     response_done, b, b->buff, strlen(b->buff));
	}
}

/*
 * static void
 * irpd_getnetbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		     const struct ctl_verb *verb, const char *rest,
 *		     u_int respflags, void *respctx, void *uctx);
 *	Implementation of GETNETBYNAME verb.
 */
static void
irpd_getnetbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
		  const struct ctl_verb *verb, const char *rest,
		  u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct netent *ne;
	struct nwent *nw;
	char netname[MAXNETNAMELEN];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, nw, sess, IRPD_GETNET_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETNET_ERROR,
				"GETNETBYNAME name");
	} else {
		if (args->iov[0].iov_len >= sizeof netname) {
			simple_response(sess, IRPD_GETNET_ERROR,
					"Name too long");
		} else {
			strncpy(netname, args->iov[0].iov_base,
				args->iov[0].iov_len);
			netname[args->iov[0].iov_len] = '\0';

			ne = getnetbyname_p(netname, netdata);

			/* The public interface only gives us a struct
			   netent, and we need a struct nwent that irs uses
			   internally, so we go dig it out ourselves. Yuk
			*/
			nw = NULL;
			if (ne != NULL) {
				/* Puke. */
				INSIST(netdata->nw_last == ne);
				nw = netdata->nww_last;
			}

			send_nwent(sess, nw);
		}
	}
	free_args(args);
}

/*
 * static void
 * irpd_getnetbyaddr(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		     const struct ctl_verb *verb, const char *rest,
 *		     u_int respflags, void *respctx, void *uctx);
 */
static void
irpd_getnetbyaddr(struct ctl_sctx *ctx, struct ctl_sess *sess,
		  const struct ctl_verb *verb, const char *rest,
		  u_int respflags, void *respctx, void *uctx)
{
	struct netent *ne;
	struct nwent *nw;
	char haddr[MAXHOSTNAMELEN];
	long tmpaddr;
	struct arg_s *args;
	int af;
	int addrlen;
	int bits;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, nw, sess, IRPD_GETUSER_ERROR);

	args = split_string(rest);
	if (args->iovlen != 3) {
		simple_response(sess, IRPD_GETNET_ERROR,
				"GETNETBYADDR addr afamily");
	} else {
		if (args->iov[0].iov_len >= sizeof haddr) {
			simple_response(sess, IRPD_GETNET_ERROR,
					"Address too long");
		} else {
			strncpy(haddr, args->iov[1].iov_base,
				args->iov[1].iov_len);
			haddr[args->iov[1].iov_len] = '\0';
			if (strcasecmp(haddr, "af_inet") == 0) {
				af = AF_INET;
				addrlen = NS_INADDRSZ;
			} else if (strcasecmp(haddr, "af_inet6") == 0) {
				af = AF_INET6;
				addrlen = NS_IN6ADDRSZ;

				/* XXX the interface we use(getnetbyaddr)
				 * can't handle AF_INET6, so for now we
				 * bail.
				 */
				simple_response(sess, IRPD_GETNET_ERROR,
						"AF_INET6 unsupported");
				goto untimely;
			} else {
				simple_response(sess, IRPD_GETNET_ERROR,
					      "Unknown address family");
				goto untimely;
			}

			strncpy(haddr, args->iov[0].iov_base,
				args->iov[0].iov_len);
			haddr[args->iov[0].iov_len] = '\0';

			bits = inet_net_pton(af, haddr,
					     &tmpaddr, sizeof tmpaddr);
			if (bits < 0) {
				simple_response(sess, IRPD_GETNET_ERROR,
						"Invalid address");
				goto untimely;
			}

			ne = getnetbyaddr_p(tmpaddr, af, netdata);

			/* The public interface only gives us a struct
			   netent, and we need a struct nwent that irs uses
			   internally, so we go dig it out ourselves. Yuk
			*/
			nw = NULL;
			if (ne != NULL) {
				/* Puke puke */
				INSIST(netdata->nw_last == ne);
				nw = netdata->nww_last;
			}

			send_nwent(sess, nw);
		}
	}

  untimely:
	free_args(args);
}


/*
 * static void
 * irpd_getnetent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		  const struct ctl_verb *verb, const char *rest,
 *		  u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETNETENT verb.
 */
static void
irpd_getnetent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	       const struct ctl_verb *verb, const char *rest,
	       u_int respflags, void *respctx, void *uctx)
{
	struct netent *ne;
	struct nwent *nw;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, nw, sess, IRPD_GETNET_ERROR);

	ne = getnetent_p(netdata);
	nw = NULL;
	if (ne != NULL) {
		/* triple puke */
		INSIST(netdata->nw_last == ne);
		nw = netdata->nww_last;
	}
	send_nwent(sess, nw);
}

/*
 * static void
 * irpd_setnetent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		  const struct ctl_verb *verb, const char *rest,
 *		  u_int respflags, void *respctx, void *uctx);
 *	Implementation of the SETNETENT verb.
 */
static void
irpd_setnetent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	       const struct ctl_verb *verb, const char *rest,
	       u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, nw, sess, IRPD_GETNET_ERROR);

	setnetent_p(1, netdata);	/* always stayopen */
	simple_response(sess, IRPD_GETNET_SETOK, "ok");
}

#ifdef WANT_IRS_GR
/*
 * static void
 * send_grent(struct ctl_sess *sess, struct group *gr);
 *	Marshall GR and send as body of response. If GR is NULL
 *	then a "No such group" response is sent instead.
 */
static void
send_grent(struct ctl_sess *sess, struct group *gr) {
	if (gr == NULL) {
		simple_response(sess, IRPD_GETGROUP_NONE,
				"No such user");
	} else {
		struct response_buff *b = newbuffer(0);

		if (irp_marshall_gr(gr, &b->buff, &b->bufflen) != 0) {
			simple_response(sess, IRPD_GETGROUP_ERROR,
					"Internal error");
			logger(ctl_warning, "Cant marshall gr\n");
			return;
		}

		strcat(b->buff, "\r\n");

		ctl_response(sess, IRPD_GETGROUP_OK, "Group found", 0, 0,
			     response_done, b, b->buff, strlen(b->buff));
	}
}

/*
 * static void
 * irpd_getgrnam(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETGRNAM verb.
 */
static void
irpd_getgrnam(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct group *gr;
	char groupname[64];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, gr, sess, IRPD_GETGROUP_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETGROUP_ERROR,
				"GETGRNAM groupname");
	} else {
		if (args->iov[0].iov_len >= sizeof groupname) {
			simple_response(sess, IRPD_GETGROUP_ERROR,
					"Name too long");
		} else {
			strncpy(groupname, args->iov[0].iov_base,
				args->iov[0].iov_len);
			groupname[args->iov[0].iov_len] = '\0';

			gr = getgrnam_p(groupname, netdata);
			send_grent(sess, gr);
		}
	}

	free_args(args);
}

/*
 * static void
 * irpd_getgrgid(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implentation of the GETGRGID verb.
 */
static void
irpd_getgrgid(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct group *gr;
	char groupid[64];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, gr, sess, IRPD_GETGROUP_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETGROUP_ERROR,
				"GETGRUID gid");
	} else {
		if (args->iov[0].iov_len >= sizeof groupid) {
			simple_response(sess, IRPD_GETGROUP_ERROR,
					"Name too long");
		} else {
			strncpy(groupid, args->iov[0].iov_base,
				args->iov[0].iov_len);
			groupid[args->iov[0].iov_len] = '\0';

			if (!ALLDIGITS(groupid)) {
				simple_response(sess, IRPD_GETGROUP_ERROR,
						"Not a gid");
			} else {
				gid_t gid;
				long lval;

				lval = strtol(groupid, 0, 10);
				gid = (gid_t)lval;
				if ((long)gid != lval) {
					/* value was too big */
					simple_response(sess,
							IRPD_GETGROUP_ERROR,
							"Not a valid gid");
					goto untimely;
				}

				gr = getgrgid_p(gid, netdata);
				send_grent(sess, gr);
			}
		}
	}

  untimely:
	free_args(args);
}

/*
 * static void
 * irpd_getgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implementation of the GETGRENT verb.
 */
static void
irpd_getgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct group *gr;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, gr, sess, IRPD_GETGROUP_ERROR);

	gr = getgrent_p(netdata);
	send_grent(sess, gr);
}

/*
 * static void
 * irpd_setgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		 const struct ctl_verb *verb, const char *rest,
 *		 u_int respflags, void *respctx, void *uctx);
 *	Implementation of the SETGRENT verb.
 */
static void
irpd_setgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, gr, sess, IRPD_GETGROUP_ERROR);

	setgrent_p(netdata);
	simple_response(sess, IRPD_GETGROUP_SETOK, "ok");
}
#endif /* WANT_IRS_GR */

static void
send_servent(struct ctl_sess *sess, struct servent *serv) {
	if (serv == NULL) {
		simple_response(sess, IRPD_GETSERVICE_NONE,
				"No such service");
	} else {
		struct response_buff *b = newbuffer(0);

		if (irp_marshall_sv(serv, &b->buff,
				    &b->bufflen) != 0) {
			simple_response(sess, IRPD_GETSERVICE_ERROR,
					"Internal error");
			logger(ctl_warning, "Cant marshall servent\n");
			return;
		}

		strcat(b->buff, "\r\n");

		ctl_response(sess, IRPD_GETSERVICE_OK, "Service found", 0, 0,
			     response_done, b, b->buff, strlen(b->buff));
	}
}

static void
irpd_getservbyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
		   const struct ctl_verb *verb, const char *rest,
		   u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct servent *serv;
	char servicename[64];
	char protoname[10];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, sv, sess, IRPD_GETSERVICE_ERROR);

	args = split_string(rest);
	if (args->iovlen != 3) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETSERVICE_ERROR,
				"GETSERVNAM servicename protocol");
	} else {
		if (args->iov[0].iov_len >= sizeof servicename) {
			simple_response(sess, IRPD_GETSERVICE_ERROR,
					"Invalid service name");
		} else if (args->iov[1].iov_len >= sizeof protoname) {
			simple_response(sess, IRPD_GETSERVICE_ERROR,
					"Invalid protocol name");
		} else {
			strncpy(servicename, args->iov[0].iov_base,
				args->iov[0].iov_len);
			servicename[args->iov[0].iov_len] = '\0';

			strncpy(protoname, args->iov[1].iov_base,
				args->iov[1].iov_len);
			protoname[args->iov[1].iov_len] = '\0';

			serv = getservbyname_p(servicename, protoname,
					       netdata);
			send_servent(sess, serv);
		}
	}

	free_args(args);
}

/*
 * static void
 * irpd_getservbyport(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		      const struct ctl_verb *verb, const char *rest,
 *		      u_int respflags, void *respctx, void *uctx);
 *	Handle the GETSERVBYPORT verb.
 */
static void
irpd_getservbyport(struct ctl_sctx *ctx, struct ctl_sess *sess,
		   const struct ctl_verb *verb, const char *rest,
		   u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct servent *sv;
	char portnum[64];
	char protoname[10];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, sv, sess, IRPD_GETSERVICE_ERROR);

	args = split_string(rest);
	if (args->iovlen != 3) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETSERVICE_ERROR,
				"GETSERVBYPORT port protocol");
	} else {
		if (args->iov[0].iov_len >= sizeof portnum) {
			simple_response(sess, IRPD_GETSERVICE_ERROR,
					"Invalid port");
		} else if (args->iov[1].iov_len > sizeof protoname - 1) {
			simple_response(sess, IRPD_GETSERVICE_ERROR,
					"Invalid protocol");
		} else {
			strncpy(portnum, args->iov[0].iov_base,
				args->iov[0].iov_len);
			portnum[args->iov[0].iov_len] = '\0';

			strncpy(protoname, args->iov[1].iov_base,
				args->iov[1].iov_len);
			protoname[args->iov[1].iov_len] = '\0';

			if (!ALLDIGITS(portnum)) {
				simple_response(sess, IRPD_GETSERVICE_ERROR,
						"Not a port number");
			} else {
				short port;
				long lval;

				lval = strtol(portnum, 0, 10);
				port = (short)lval;
				if ((long)port != lval) {
					/* value was too big */
					simple_response(sess,
							IRPD_GETSERVICE_ERROR,
							"Not a valid port");
					goto untimely;
				}
				port = htons(port);

				sv = getservbyport_p(port, protoname, netdata);
				send_servent(sess, sv);
			}
		}
	}

  untimely:
	free_args(args);
}

/*
 * static void
 * irpd_getservent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		   const struct ctl_verb *verb, const char *rest,
 *		   u_int respflags, void *respctx, void *uctx);
 *	Handle the GETSERVENT verb.
 */
static void
irpd_getservent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	       const struct ctl_verb *verb, const char *rest,
	       u_int respflags, void *respctx, void *uctx)
{
	struct servent *sv;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, sv, sess, IRPD_GETSERVICE_ERROR);

	sv = getservent_p(netdata);
	send_servent(sess, sv);
}

/*
 * static void
 * irpd_setservent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		   const struct ctl_verb *verb, const char *rest,
 *		   u_int respflags, void *respctx, void *uctx);
 *	Handle the SETSERVENT verb.
 */
static void
irpd_setservent(struct ctl_sctx *ctx, struct ctl_sess *sess,
	       const struct ctl_verb *verb, const char *rest,
	       u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, sv, sess, IRPD_GETSERVICE_ERROR);

	setservent_p(1, netdata);	/* always stay open */
	simple_response(sess, IRPD_GETSERVICE_SETOK, "ok");
}

/*
 * static void
 * send_prent(struct ctl_sess *sess, struct protoent *pr);
 *	Send the PR structure over the wire. If PR is NULL, then
 *	the response "No such protocol" is sent instead.
 */
static void
send_prent(struct ctl_sess *sess, struct protoent *pr) {
	if (pr == NULL) {
		simple_response(sess, IRPD_GETPROTO_NONE,
				"No such protocol");
	} else {
		struct response_buff *b = newbuffer(0);

		if (irp_marshall_pr(pr, &b->buff,
				    &b->bufflen) != 0) {
			simple_response(sess, IRPD_GETPROTO_ERROR,
					"Internal error");
			logger(ctl_warning, "Cant marshall pr\n");
			return;
		}

		strcat(b->buff, "\r\n");

		ctl_response(sess, IRPD_GETPROTO_OK, "Protocol found", 0, 0,
			     response_done, b, b->buff, strlen(b->buff));
	}
}

/*
 * static void
 * irpd_getprotobyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		       const struct ctl_verb *verb, const char *rest,
 *		       u_int respflags, void *respctx, void *uctx);
 *	Handle the GETPROTOBYNAME verb.
 */
static void
irpd_getprotobyname(struct ctl_sctx *ctx, struct ctl_sess *sess,
		    const struct ctl_verb *verb, const char *rest,
		    u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct protoent *pr;
	char protoname[64];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pr, sess, IRPD_GETPROTO_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETPROTO_ERROR,
				"GETPROTOBYNAME protocol");
	} else {
		if (args->iov[0].iov_len >= sizeof protoname) {
			simple_response(sess, IRPD_GETPROTO_ERROR,
					"Name too long");
		} else {
			strncpy(protoname, args->iov[0].iov_base,
				args->iov[0].iov_len);
			protoname[args->iov[0].iov_len] = '\0';

			pr = getprotobyname_p(protoname, netdata);
			send_prent(sess, pr);
		}
	}
	free_args(args);
}

/*
 * static void
 * irpd_getprotobynumber(struct ctl_sctx *ctx,
 *			 struct ctl_sess *sess, const struct ctl_verb *verb,
 *			 const char *rest, u_int respflags, void *respctx,
 *			 void *uctx);
 *	Handle the GETPROTOBYNUMBER verb.
 */
static void
irpd_getprotobynumber(struct ctl_sctx *ctx, struct ctl_sess *sess,
		      const struct ctl_verb *verb, const char *rest,
		      u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct protoent *pr;
	char protonum[64];
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pr, sess, IRPD_GETPROTO_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETPROTO_ERROR,
				"GETPROTOBYNUMBER protocol");
	} else {
		if (args->iov[0].iov_len >= sizeof protonum) {
			simple_response(sess, IRPD_GETGROUP_ERROR,
					"Name too long");
		} else {
			strncpy(protonum, args->iov[0].iov_base,
				args->iov[0].iov_len);
			protonum[args->iov[0].iov_len] = '\0';

			if (!ALLDIGITS(protonum)) {
				simple_response(sess, IRPD_GETPROTO_ERROR,
						"Not a protocol number");
			} else {
				int proto;
				long lval;

				lval = strtol(protonum, 0, 10);
				proto = (int)lval;
				if ((long)proto != lval) {
					/* value was too big */
					simple_response(sess,
							IRPD_GETPROTO_ERROR,
							"Not a valid proto");
					goto untimely;
				}

				pr = getprotobynumber_p(proto, netdata);
				send_prent(sess, pr);
			}
		}
	}

  untimely:
	free_args(args);
}

/*
 * static void
 * irpd_getprotoent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		    const struct ctl_verb *verb, const char *rest,
 *		    u_int respflags, void *respctx, void *uctx);
 *	Handle the GETPROTOENT verb.
 */
static void
irpd_getprotoent(struct ctl_sctx *ctx, struct ctl_sess *sess,
		const struct ctl_verb *verb, const char *rest,
		u_int respflags, void *respctx, void *uctx)
{
	struct protoent *pr;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pr, sess, IRPD_GETPROTO_ERROR);

	pr = getprotoent_p(netdata);
	send_prent(sess, pr);
}

/*
 * static void
 * irpd_setprotoent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		    const struct ctl_verb *verb, const char *rest,
 *		    u_int respflags, void *respctx, void *uctx);
 *	Handle the SETPROTOENT verb.
 */
static void
irpd_setprotoent(struct ctl_sctx *ctx, struct ctl_sess *sess,
		const struct ctl_verb *verb, const char *rest,
		u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, pr, sess, IRPD_GETPROTO_ERROR);

	setprotoent_p(1, netdata);	/* always stay open */
	simple_response(sess, IRPD_GETPROTO_SETOK, "ok");
}

/*
 * static void
 * send_pwent(struct ctl_sess *sess, struct passwd *pw);
 *	Send PW over the wire, or, if PW is NULL, a "No such
 *	user" response.
 */
static void
send_ngent(struct ctl_sess *sess, char *host, char *user, char *domain) {
	struct response_buff *b = newbuffer(0);

	if (irp_marshall_ng(host, user, domain, &b->buff,
			    &b->bufflen) != 0) {
		simple_response(sess, IRPD_GETNETGR_ERROR,
				"Internal error");
		logger(ctl_warning, "Cant marshall ng\n");
		return;
	}
	
	strcat(b->buff, "\r\n");
	
	ctl_response(sess, IRPD_GETNETGR_OK, "Netgroup entry", 0, 0,
		     response_done, b, b->buff, strlen(b->buff));
}

/*
 * static void
 * irpd_getnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		    const struct ctl_verb *verb, const char *rest,
 *		    u_int respflags, void *respctx, void *uctx);
 *	Handle the GETNETGRENT verb. 
 */
static void
irpd_getnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
		 const struct ctl_verb *verb, const char *rest,
		 u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ng, sess, IRPD_GETNETGR_ERROR);

	if (rest != NULL && strlen(rest) > 0) {
		simple_response(sess, IRPD_GETNETGR_ERROR,
				"GETNETGRENT");
	} else {
		char *host, *user, *domain;
		
		if (getnetgrent_p(&host, &user, &domain, netdata) == 1) {
			send_ngent(sess, host, user, domain);
		} else {
			simple_response(sess, IRPD_GETNETGR_NOMORE,
					"No more");
		}
	}
}

/*
 * static void
 * irpd_innetgr(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		const struct ctl_verb *verb, const char *rest,
 *		u_int respflags, void *respctx, void *uctx);
 *	Handle the INNETGR verb.
 */
static void
irpd_innetgr(struct ctl_sctx *ctx, struct ctl_sess *sess,
		 const struct ctl_verb *verb, const char *rest,
		 u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct net_data *netdata = get_net_data(sess);
	char *host;
	char *user;
	char *domain;
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ng, sess, IRPD_GETNETGR_ERROR);

	args = split_string(rest);
	if (args->iovlen != 3) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETNETGR_ERROR,
				"INNETGR netgroup ngentry");
	} else {
		char *grptmp = memget(args->iov[0].iov_len + 1);
		char *ngtmp = memget(args->iov[1].iov_len + 1);

		strncpy(grptmp, args->iov[0].iov_base, args->iov[0].iov_len);
		strncpy(ngtmp, args->iov[1].iov_base, args->iov[1].iov_len);

		grptmp[args->iov[0].iov_len] = '\0';
		ngtmp[args->iov[1].iov_len] = '\0';
		
		if (irp_unmarshall_ng(&host, &user, &domain, ngtmp) != 0) {
			simple_response(sess, IRPD_GETNETGR_ERROR,
					"ngentry must be (host,user,domain)");
		} else {
			if (innetgr_p(grptmp, host, user, domain,
				      netdata) == 1) {
				simple_response(sess, IRPD_GETNETGR_MATCHES,
						"INNETGR matches");
			} else {
				simple_response(sess, IRPD_GETNETGR_NOMATCH,
						"INNETGR does not match");
			}
		}

		memput(grptmp, args->iov[0].iov_len + 1);
		memput(ngtmp, args->iov[1].iov_len + 1);
	}
	
	free_args(args);
}

/*
 * static void
 * irpd_setnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		    const struct ctl_verb *verb, const char *rest,
 *		    u_int respflags, void *respctx, void *uctx);
 *	Handle the SETNETGRENT verb.
 */
static void
irpd_setnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
		 const struct ctl_verb *verb, const char *rest,
		 u_int respflags, void *respctx, void *uctx)
{
	struct arg_s *args;
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ng, sess, IRPD_GETNETGR_ERROR);

	args = split_string(rest);
	if (args->iovlen != 2) {	/* len includes NULL at end */
		simple_response(sess, IRPD_GETNETGR_ERROR,
				"setnetgrent netgroup");
	} else {
		setnetgrent_p(rest, netdata);
		simple_response(sess, IRPD_GETNETGR_SETOK,
				"setnetgrent ok");
	}
	
	free_args(args);
}

/*
 * static void
 * irpd_endnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *		    const struct ctl_verb *verb, const char *rest,
 *		    u_int respflags, void *respctx, void *uctx);
 *	Handle the ENDNETGRENT verb.
 */
static void
irpd_endnetgrent(struct ctl_sctx *ctx, struct ctl_sess *sess,
		 const struct ctl_verb *verb, const char *rest,
		 u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	INSIST(netdata != NULL);

	ND_INIT(netdata, ng, sess, IRPD_GETNETGR_ERROR);

	if (rest != NULL && strlen (rest) > 0) {
		simple_response(sess, IRPD_GETNETGR_ERROR,
				"endnetgrent netgroup");
	} else {
		endnetgrent_p(netdata);
		simple_response(sess, IRPD_GETNETGR_SETOK,
				"endnetgrent ok");
	}
}

/*
 * static void
 * irpd_quit(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *	     const struct ctl_verb *verb, const char *rest,
 *	     u_int respflags, void *respctx, void *uctx);
 *	Handle the QUIT verb.
 */
static void
irpd_quit(struct ctl_sctx *ctx, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	ctl_response(sess, irpd_quit_ok, "See ya!", CTL_EXIT, NULL,
		     0 , NULL, NULL, 0);
}

/*
 * static void
 * irpd_help(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *	     const struct ctl_verb *verb, const char *rest,
 *	     u_int respflags, void *respctx, void *uctx);
 *	Handle the HELP verb.
 */
static void
irpd_help(struct ctl_sctx *ctx, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	/* XXX	should make this do something better (like include required
	 *	arguments.
	 */
	ctl_sendhelp(sess, 231);
}

/*
 * static void
 * irpd_accept(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *	       const struct ctl_verb *verb, const char *rest,
 *	       u_int respflags, void *respctx, void *uctx);
 *	Handle a new connection.
 */
static void
irpd_accept(struct ctl_sctx *ctx, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	struct sockaddr *sa = respctx;
	char raddr[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
	int reject = 1;
	int response;
	char *respmsg = NULL;

	if (sa->sa_family == AF_UNIX) {
		syslog (LOG_INFO, "New AF_UNIX connection");
		reject = 0;
	} else if (sa->sa_family == AF_INET) {
		struct sockaddr_in *sin = respctx;
		static long localhost;
		static long zero;

		if (localhost == 0) {
			/* yes, this could be done with simple arithmetic... */
			inet_pton(AF_INET, "127.0.0.1", &localhost);
		}

		inet_ntop(AF_INET, &sin->sin_addr, raddr, sizeof raddr);

		/* we reject INET connections that are not from the local
		 * machine.
		 */
		if (sin->sin_addr.s_addr == zero ||
		    sin->sin_addr.s_addr == localhost) {
			reject = 0;
			syslog(LOG_INFO, "New connection from %s", raddr);
		} else {
			syslog(LOG_INFO, "New connection from %s (reject)",
			       raddr);
			respmsg = "Connections from off host not permitted";
		}
	} else if (sa->sa_family == AF_INET6) {
		/* XXX should do something intelligent here. */
		respmsg = "IPv6 connections not implemented yet.";
		syslog(LOG_ERR, "Cannot handle AF_INET6 connections yet");
	} else {
		syslog (LOG_ERR, "Unknown peer type: %d", sa->sa_family);
		respmsg = "What are you???";
	}

	if (reject) {
		response = IRPD_NOT_WELCOME_CODE;
		if (respmsg == NULL) {
			respmsg = "Go away!";
		}
		/* XXX can we be sure that stacked up commands will not be
		 * processed before the control connection is closed???
		 */
	} else {
		void *ctx = make_cli_ctx();

		if (ctx == NULL) {
			response = IRPD_NOT_WELCOME_CODE;
			respmsg = "Internal error (client context)";
		} else {
			response = IRPD_WELCOME_CODE;
			if (respmsg == NULL) {
				respmsg = "Welcome to IRPD (v 1)";
			}
			ctl_setcsctx(sess, ctx);
		}
	}
	ctl_response(sess, response, respmsg, (reject ? CTL_EXIT : 0), NULL,
		     0, NULL, NULL, 0);
}

/*
 * static void
 * irpd_abort(struct ctl_sctx *ctx, struct ctl_sess *sess,
 *	      const struct ctl_verb *verb, const char *rest,
 *	      u_int respflags, void *respctx, void *uctx);
 *	Handle a dropped connection.
 */
static void
irpd_abort(struct ctl_sctx *ctx, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	struct net_data *netdata = get_net_data(sess);
	
	if (netdata != NULL)
		net_data_destroy(netdata);
}

/*
 * void
 * response_done(struct ctl_sctx *ctx, struct ctl_sess *sess, void *uap)
 *	UAP is the response_buffer passed through to
 *	ctl_response.
 */
static void
response_done(struct ctl_sctx *ctx, struct ctl_sess *sess, void *uap) {
	release_buffer(uap);
}

/*
 * static void
 * logger(enum ctl_severity sev, const char *fmt, ...);
 *	Logging routine called by the ctl_* functions. For now we
 *	just spit everything to stderr.
 */

static void
logger(enum ctl_severity sev, const char *fmt, ...) {
	char buffer[1024];
	va_list ap;
	int level;

	if (sev == ctl_debug)
		return;

	if (sev == ctl_warning)
		level = LOG_WARNING;
	else if (sev == ctl_error)
		level = LOG_ERR;
	else {
		syslog(LOG_CRIT, "Invalid severity: %d", (int)sev);
		exit(1);
	}

	va_start(ap, fmt);

#if 0
	fprintf(stderr, "irpd: ");
	vfprintf(stderr, fmt, ap);
#else
	if (vsprintf(buffer, fmt, ap) > (sizeof (buffer) - 1)) {
		syslog(LOG_CRIT, "Buffer overrun in logger");
		abort();
	}
	syslog(level, "%s", buffer);
#endif
	va_end(ap);
}

/*
 * static struct response_buff *
 * newbuffer(u_int length);
 *	Create a structure to hold an allocated buffer. We do
 *	this so we can get the size to deallocate later.
 * Returns:
 *	Pointer to the structure
 */
static struct response_buff *
newbuffer(u_int length) {
	struct response_buff *h;

	h = memget(sizeof *h);
	if (h == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	h->buff = NULL;
	h->bufflen = length;

	if (length > 0) {
		h->buff = memget(h->bufflen);
		if (h->buff == NULL) {
			memput(h, sizeof *h);
			errno = ENOMEM;
			return (NULL);
		}
		memset(h->buff, 0, h->bufflen);
	}

	return (h);
}

/*
 * static void
 * release_buffer(struct response_buff *b);
 *	Free up a buffer allocated with newbuffer.
 */
static void
release_buffer(struct response_buff *b) {
	memset(b->buff, 0, b->bufflen);
	memput(b->buff, b->bufflen);

	memset(b, 0, sizeof *b);
	memput(b, sizeof *b);
}

/*
 * static struct arg_s *
 * split_string(const char *string);
 *	Create an array of iovecs(last one having NULL fields)
 *	pointing into STRING at the non-whitespace sections. The
 *	iovecs are stashed inside a structure so we can get the
 *	size back later at deallocation time. Iovecs are used to avoid
 *	modifying the argument with added nulls.
 * Returns:
 *	Pointer to the wrapper structure. Must be given to free_args()
 *	when done
 */
static struct arg_s *
split_string(const char *string) {
	struct iovec *iovs;
	const char *p;
	int i, c, iswh;
	struct arg_s *a;

	/* count + 1 of the number of runs of non-whitespace. */
	for (iswh = 1, i = 1, p = string ; p != NULL && *p ; p++) {
		if (iswh && !isspace(*p)) {
			iswh = 0;
			i++;
		} else if (!iswh && isspace(*p)) {
			iswh = 1;
		}
	}

	iovs = memget(sizeof (struct iovec) * i);
	if (iovs == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	a = memget(sizeof *a);
	if (a == NULL) {
		errno = ENOMEM;
		memput(iovs, sizeof (struct iovec) * i);
		return (NULL);
	}
	a->iov = iovs;
	a->iovlen = i;

	for (c = 0, p = string ; p != NULL && *p ; c++) {
		while (isspace(*p)) {
			p++;
		}

		if (*p == '\0')
			break;

		iovs[c].iov_base = (void *)p;

		while (*p && !isspace(*p)) {
			p++;
		}
		iovs[c].iov_len = p - (char *)iovs[c].iov_base;
	}
	INSIST(c == i - 1);
	iovs[c].iov_base = NULL;
	iovs[c].iov_len = 0;

	return (a);
}

/*
 * static void
 * free_args(struct arg_s *args);
 *	Free up the argument structure created with
 *	split_string().
 */

static void
free_args(struct arg_s *args) {
	memput(args->iov, sizeof (struct iovec) * args->iovlen);
	memput(args, sizeof *args);
}

static struct client_ctx *
make_cli_ctx(void) {
	struct client_ctx *p = memget (sizeof *p);

	if (p == NULL)
		return (NULL);
	
	p->net_data = net_data_create(conffile);

	return (p);
}

static struct net_data *
get_net_data(struct ctl_sess *sess) {
	struct client_ctx *ctx = ctl_getcsctx(sess);

	INSIST(ctx != NULL);
	INSIST(ctx->net_data != NULL);

	return (ctx->net_data);
}
