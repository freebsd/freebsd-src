/*	$NetBSD: t_rpc.c,v 1.3 2013/02/28 15:56:53 christos Exp $	*/

#include <sys/cdefs.h>
__RCSID("$NetBSD: t_rpc.c,v 1.3 2013/02/28 15:56:53 christos Exp $");

#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <stdlib.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>


#ifndef TEST
#include <atf-c.h>

#define ERRX(ev, msg, ...)	ATF_REQUIRE_MSG(0, msg, __VA_ARGS__)

#define SKIPX(ev, msg, ...)	do {			\
	atf_tc_skip(msg, __VA_ARGS__);			\
	return;						\
} while(/*CONSTCOND*/0)

#else
#define ERRX(ev, msg, ...)	errx(ev, msg, __VA_ARGS__)
#define SKIPX(ev, msg, ...)	errx(ev, msg, __VA_ARGS__)
#endif


#define RPCBPROC_NULL 0

static int
reply(caddr_t replyp, struct netbuf * raddrp, struct netconfig * nconf)
{
	char host[NI_MAXHOST];
	struct sockaddr *sock = raddrp->buf;
	int error;


	error = getnameinfo(sock, sock->sa_len, host, sizeof(host), NULL, 0, 0);
	if (error)
		warnx("Cannot resolve address (%s)", gai_strerror(error));
	else
		printf("response from: %s\n", host);
	return 0;
}

extern bool __rpc_control(int, void *);

static void
onehost(const char *host, const char *transp)
{
	CLIENT         *clnt;
	struct netbuf   addr;
	struct timeval  tv;

	/*
	 * Magic!
	 */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
#define CLCR_SET_RPCB_TIMEOUT   2
	__rpc_control(CLCR_SET_RPCB_TIMEOUT, &tv);

	if ((clnt = clnt_create(host, RPCBPROG, RPCBVERS, transp)) == NULL)
		SKIPX(EXIT_FAILURE, "clnt_create (%s)", clnt_spcreateerror(""));

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (clnt_call(clnt, RPCBPROC_NULL, xdr_void, NULL, xdr_void, NULL, tv)
	    != RPC_SUCCESS)
		ERRX(EXIT_FAILURE, "clnt_call (%s)", clnt_sperror(clnt, ""));
	clnt_control(clnt, CLGET_SVC_ADDR, (char *) &addr);
	reply(NULL, &addr, NULL);
}

#ifdef TEST
static void
allhosts(void)
{
	enum clnt_stat  clnt_stat;

	clnt_stat = rpc_broadcast(RPCBPROG, RPCBVERS, RPCBPROC_NULL,
	    (xdrproc_t)xdr_void, NULL, (xdrproc_t)xdr_void,
	    NULL, (resultproc_t)reply, transp);
	if (clnt_stat != RPC_SUCCESS && clnt_stat != RPC_TIMEDOUT)
		ERRX(EXIT_FAILURE, "%s", clnt_sperrno(clnt_stat));
}

int
main(int argc, char *argv[])
{
	int             ch;
	const char     *transp = "udp";


	while ((ch = getopt(argc, argv, "ut")) != -1)
		switch (ch) {
		case 't':
			transp = "tcp";
			break;
		case 'u':
			transp = "udp";
			break;
		default:
			fprintf(stderr, "Usage: %s -[t|u] [<hostname>...]\n",
			    getprogname());
			return EXIT_FAILURE;
		}

	if (argc == optind)
		allhosts();
	else
		for (; optind < argc; optind++)
			onehost(argv[optind], transp);

	return EXIT_SUCCESS;
}

#else

ATF_TC(get_svc_addr_tcp);
ATF_TC_HEAD(get_svc_addr_tcp, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks CLGET_SVC_ADDR for tcp");

}

ATF_TC_BODY(get_svc_addr_tcp, tc)
{
	onehost("localhost", "tcp");

}

ATF_TC(get_svc_addr_udp);
ATF_TC_HEAD(get_svc_addr_udp, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks CLGET_SVC_ADDR for udp");
}

ATF_TC_BODY(get_svc_addr_udp, tc)
{
	onehost("localhost", "udp");

}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, get_svc_addr_udp);
	ATF_TP_ADD_TC(tp, get_svc_addr_tcp);

	return atf_no_error();
}

#endif
