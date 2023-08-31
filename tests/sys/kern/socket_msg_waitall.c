/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

struct close_test_params {
	struct sockaddr_storage sa;
	size_t msglen;
	int count;
	int af, type, proto;
};

static void *
close_test_client(void *arg)
{
	struct close_test_params *p = arg;
	char *buf;
	size_t buflen;

	buflen = p->msglen + 1;
	buf = malloc(buflen);
	ATF_REQUIRE(buf != NULL);

	while (p->count-- > 0) {
		ssize_t n;
		int error, s;

		s = socket(p->af, p->type, p->proto);
		ATF_REQUIRE_MSG(s >= 0, "socket: %s", strerror(errno));

		error = connect(s, (struct sockaddr *)&p->sa, p->sa.ss_len);
		ATF_REQUIRE_MSG(error == 0, "connect: %s", strerror(errno));

		n = recv(s, buf, buflen, MSG_WAITALL);
		ATF_REQUIRE_MSG(n == (ssize_t)p->msglen,
		    "recv: %s", strerror(errno));

		ATF_REQUIRE(close(s) == 0);
	}

	return (NULL);
}

static void
close_test(struct sockaddr *sa, unsigned int count, int af, int type, int proto)
{
	struct close_test_params p;
	const char *msg;
	pthread_t t;
	size_t msglen;
	int error, s;

	s = socket(af, type, proto);
	ATF_REQUIRE_MSG(s >= 0, "socket %s", strerror(errno));

	ATF_REQUIRE_MSG(bind(s, sa, sa->sa_len) == 0,
	    "bind: %s", strerror(errno));
	ATF_REQUIRE_MSG(listen(s, 1) == 0,
	    "listen: %s", strerror(errno));
	ATF_REQUIRE_MSG(getsockname(s, sa, &(socklen_t){ sa->sa_len }) == 0,
	    "getsockname: %s", strerror(errno));

	msg = "hello bonjour";
	msglen = strlen(msg) + 1;
	p = (struct close_test_params){
		.count = count,
		.msglen = msglen,
		.af = af,
		.type = type,
		.proto = proto,
	};
	memcpy(&p.sa, sa, sa->sa_len);
	error = pthread_create(&t, NULL, close_test_client, &p);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	while (count-- > 0) {
		ssize_t n;
		int cs;

		cs = accept(s, NULL, NULL);
		ATF_REQUIRE_MSG(cs >= 0, "accept: %s", strerror(errno));

		n = send(cs, msg, msglen, 0);
		ATF_REQUIRE_MSG(n == (ssize_t)msglen,
		    "send: %s", strerror(errno));

		ATF_REQUIRE(close(cs) == 0);
	}

	ATF_REQUIRE(close(s) == 0);
	ATF_REQUIRE(pthread_join(t, NULL) == 0);
}

/*
 * Make sure that closing a connection kicks a MSG_WAITALL recv() out of the
 * syscall.  See bugzilla PR 212716.
 */
ATF_TC(close_tcp);
ATF_TC_HEAD(close_tcp, tc)
{
	atf_tc_set_md_var(tc, "timeout", "10");
}
ATF_TC_BODY(close_tcp, tc)
{
	struct sockaddr_in sin;

	sin = (struct sockaddr_in){
		.sin_len = sizeof(sin),
		.sin_family = AF_INET,
		.sin_addr = { htonl(INADDR_LOOPBACK) },
		.sin_port = htons(0),
	};
	close_test((struct sockaddr *)&sin, 1000, AF_INET, SOCK_STREAM,
	    IPPROTO_TCP);
}

/* A variant of the above test for UNIX domain stream sockets. */
ATF_TC(close_unix_stream);
ATF_TC_HEAD(close_unix_stream, tc)
{
	atf_tc_set_md_var(tc, "timeout", "10");
}
ATF_TC_BODY(close_unix_stream, tc)
{
	struct sockaddr_un sun;

	sun = (struct sockaddr_un){
		.sun_len = sizeof(sun),
		.sun_family = AF_UNIX,
		.sun_path = "socket_msg_waitall_unix",
	};
	close_test((struct sockaddr *)&sun, 1000, AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(unlink(sun.sun_path) == 0,
	    "unlink: %s", strerror(errno));
}

/* A variant of the above test for UNIX domain seqpacket sockets. */
ATF_TC(close_unix_seqpacket);
ATF_TC_HEAD(close_unix_seqpacket, tc)
{
	atf_tc_set_md_var(tc, "timeout", "10");
}
ATF_TC_BODY(close_unix_seqpacket, tc)
{
	struct sockaddr_un sun;

	sun = (struct sockaddr_un){
		.sun_len = sizeof(sun),
		.sun_family = AF_UNIX,
		.sun_path = "socket_msg_waitall_unix",
	};
	close_test((struct sockaddr *)&sun, 1000, AF_UNIX, SOCK_SEQPACKET, 0);
	ATF_REQUIRE_MSG(unlink(sun.sun_path) == 0,
	    "unlink: %s", strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, close_tcp);
	ATF_TP_ADD_TC(tp, close_unix_stream);
	ATF_TP_ADD_TC(tp, close_unix_seqpacket);

	return (atf_no_error());
}
