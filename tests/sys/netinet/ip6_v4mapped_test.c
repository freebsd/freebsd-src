/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Michael J. Karels.
 * Copyright (c) 2020 Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This test is derived from tcp_connect_port_test.c.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#define	SYSCTLBAKFILE	"tmp.net.inet.ip.portrange.values"

#define	PORT_FIRST	10000		/* normal default */
#define	PORT_LAST	10003
#define	LOOPS		10		/* 5 should be enough */

struct portrange {
	int	first;
	int	last;
};

/*
 * Set first and last ports in the ipport range.  Save the old values
 * of the sysctls so they can be restored later.
 */
static void
set_portrange(void)
{
	int error, fd, first_new, last_new;
	struct portrange save_ports;
	size_t sysctlsz;

	/*
	 * Pre-emptively unlink our restoration file, so we will do no
	 * restoration on error.
	 */
	unlink(SYSCTLBAKFILE);

	/*
	 * Set the net.inet.ip.portrange.{first,last} sysctls. Save the
	 * old values so we can restore them.
	 */
	first_new = PORT_FIRST;
	sysctlsz = sizeof(save_ports.first);
	error = sysctlbyname("net.inet.ip.portrange.first", &save_ports.first,
	    &sysctlsz, &first_new, sizeof(first_new));
	if (error) {
		warn("sysctlbyname(\"net.inet.ip.portrange.first\") "
		    "failed");
		atf_tc_skip("Unable to set sysctl");
	}
	if (sysctlsz != sizeof(save_ports.first)) {
		fprintf(stderr, "Error: unexpected sysctl value size "
		    "(expected %zu, actual %zu)\n", sizeof(save_ports.first),
		    sysctlsz);
		goto restore_sysctl;
	}

	last_new = PORT_LAST;
	sysctlsz = sizeof(save_ports.last);
	error = sysctlbyname("net.inet.ip.portrange.last", &save_ports.last,
	    &sysctlsz, &last_new, sizeof(last_new));
	if (error) {
		warn("sysctlbyname(\"net.inet.ip.portrange.last\") "
		    "failed");
		atf_tc_skip("Unable to set sysctl");
	}
	if (sysctlsz != sizeof(save_ports.last)) {
		fprintf(stderr, "Error: unexpected sysctl value size "
		    "(expected %zu, actual %zu)\n", sizeof(save_ports.last),
		    sysctlsz);
		goto restore_sysctl;
	}

	/* Open the backup file, write the contents, and close it. */
	fd = open(SYSCTLBAKFILE, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL,
	    S_IRUSR|S_IWUSR);
	if (fd < 0) {
		warn("error opening sysctl backup file");
		goto restore_sysctl;
	}
	error = write(fd, &save_ports, sizeof(save_ports));
	if (error < 0) {
		warn("error writing saved value to sysctl backup file");
		goto cleanup_and_restore;
	}
	if (error != (int)sizeof(save_ports)) {
		fprintf(stderr,
		    "Error writing saved value to sysctl backup file: "
		    "(expected %zu, actual %d)\n", sizeof(save_ports), error);
		goto cleanup_and_restore;
	}
	error = close(fd);
	if (error) {
		warn("error closing sysctl backup file");
cleanup_and_restore:
		(void)close(fd);
		(void)unlink(SYSCTLBAKFILE);
restore_sysctl:
		sysctlsz = sizeof(save_ports.first);
		(void)sysctlbyname("net.inet.ip.portrange.first", NULL,
		    NULL, &save_ports.first, sysctlsz);
		sysctlsz = sizeof(save_ports.last);
		(void)sysctlbyname("net.inet.ip.portrange.last", NULL,
		    NULL, &save_ports.last, sysctlsz);
		atf_tc_skip("Error setting sysctl");
	}
}

/*
 * Restore the sysctl values from the backup file and delete the backup file.
 */
static void
restore_portrange(void)
{
	int error, fd;
	struct portrange save_ports;

	/* Open the backup file, read the contents, close it, and delete it. */
	fd = open(SYSCTLBAKFILE, O_RDONLY);
	if (fd < 0) {
		warn("error opening sysctl backup file");
		return;
	}
	error = read(fd, &save_ports, sizeof(save_ports));
	if (error < 0) {
		warn("error reading saved values from sysctl backup file");
		return;
	}
	if (error != (int)sizeof(save_ports)) {
		fprintf(stderr,
		    "Error reading saved values from sysctl backup file: "
		    "(expected %zu, actual %d)\n", sizeof(save_ports), error);
		return;
	}
	error = close(fd);
	if (error)
		warn("error closing sysctl backup file");
	error = unlink(SYSCTLBAKFILE);
	if (error)
		warn("error removing sysctl backup file");

	/* Restore the saved sysctl values. */
	error = sysctlbyname("net.inet.ip.portrange.first", NULL, NULL,
	    &save_ports.first, sizeof(save_ports.first));
	if (error)
		warn("sysctlbyname(\"net.inet.ip.portrange.first\") "
		    "failed while restoring value");
	error = sysctlbyname("net.inet.ip.portrange.last", NULL, NULL,
	    &save_ports.last, sizeof(save_ports.last));
	if (error)
		warn("sysctlbyname(\"net.inet.ip.portrange.last\") "
		    "failed while restoring value");
}

ATF_TC_WITH_CLEANUP(tcp_v4mapped_bind);
ATF_TC_HEAD(tcp_v4mapped_bind, tc)
{
	/* root is only required for sysctls (setup and cleanup). */
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "allow_sysctl_side_effects");
	atf_tc_set_md_var(tc, "descr",
	    "Check local port assignment with bind and mapped V4 addresses");
}
/*
 * Create a listening IPv4 socket, then connect to it repeatedly using a
 * bound IPv6 socket using a v4 mapped address.  With a small port range,
 * this should fail on a bind() call with EADDRNOTAVAIL.  However, in
 * previous systems, the bind() would succeed, binding a duplicate port,
 * and then the connect would fail with EADDRINUSE.  Make sure we get
 * the right error.
 */
ATF_TC_BODY(tcp_v4mapped_bind, tc)
{
	union {
		struct sockaddr saddr;
		struct sockaddr_in saddr4;
		struct sockaddr_in6 saddr6;
	} su_clnt, su_srvr, su_mapped;
	struct addrinfo ai_hint, *aip;
	socklen_t salen;
	int csock, error, i, lsock, off = 0;
	bool got_bind_error = false;

	/*
	 * Set the net.inet.ip.portrange.{first,last} sysctls to use a small
	 * range, allowing us to generate port exhaustion quickly.
	 */
	set_portrange();

	/* Setup the listen socket. */
	lsock = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(lsock >= 0, "socket() for listen socket failed: %s",
	    strerror(errno));

	memset(&su_srvr.saddr4, 0, sizeof(su_srvr.saddr4));
	su_srvr.saddr4.sin_family = AF_INET;
	error = bind(lsock, &su_srvr.saddr, sizeof(su_srvr.saddr4));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	error = listen(lsock, LOOPS + 1);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));

	/* Get the address of the listen socket. */
	salen = sizeof(su_srvr);
	error = getsockname(lsock, &su_srvr.saddr, &salen);
	ATF_REQUIRE_MSG(error == 0,
	    "getsockname() for listen socket failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(salen == sizeof(struct sockaddr_in),
	    "unexpected sockaddr size");
	ATF_REQUIRE_MSG(su_srvr.saddr.sa_len == sizeof(struct sockaddr_in),
	    "unexpected sa_len size");

	/* Set up destination address for client sockets. */
	memset(&ai_hint, 0, sizeof(ai_hint));
	ai_hint.ai_family = AF_INET6;
	ai_hint.ai_flags = AI_NUMERICHOST | AI_V4MAPPED;
	error = getaddrinfo("127.0.0.1", NULL, &ai_hint, &aip);
	ATF_REQUIRE_MSG(error == 0, "getaddrinfo: %s", gai_strerror(error));
	memcpy(&su_mapped.saddr6, aip->ai_addr, sizeof(su_mapped.saddr6));
	su_mapped.saddr6.sin6_port = su_srvr.saddr4.sin_port;
	freeaddrinfo(aip);

	/* Set up address to bind for client sockets (unspecified). */
	memset(&su_clnt.saddr6, 0, sizeof(su_clnt.saddr6));
	su_clnt.saddr6.sin6_family = AF_INET6;

	/* Open connections in a loop. */
	for (i = 0; i < LOOPS; i++) {
		csock = socket(PF_INET6, SOCK_STREAM, 0);
		ATF_REQUIRE_MSG(csock >= 0,
		    "socket() for client socket %d failed: %s",
		    i, strerror(errno));
		error = setsockopt(csock, IPPROTO_IPV6, IPV6_V6ONLY, &off,
		    sizeof(off));
		ATF_REQUIRE_MSG(error == 0,
		    "setsockopt(IPV6_ONLY = 0) failed: %s", strerror(errno));

		/*
		 * A bind would not be necessary for operation, but
		 * provokes the error.
		 */
		error = bind(csock, &su_clnt.saddr, sizeof(su_clnt.saddr6));
		if (error != 0) {
			if (errno == EADDRNOTAVAIL) {	/* Success, expected */
				got_bind_error = true;
				break;
			}
			ATF_REQUIRE_MSG(error == 0,
			    "client bind %d failed: %s", i, strerror(errno));
		}

		error = connect(csock, &su_mapped.saddr, su_mapped.saddr.sa_len);
		if (error != 0 && errno == EADDRINUSE) {
			/* This is the specific error we were looking for. */
			atf_tc_fail("client connect %d failed, "
			    " client had duplicate port: %s",
			    i, strerror(errno));
		}
		ATF_REQUIRE_MSG(error == 0,
		    "connect() for client socket %d failed: %s",
		    i, strerror(errno));

		/*
		 * We don't accept the new socket from the server socket
		 * or close the client socket, as we want the ports to
		 * remain busy.  The range is small enough that this is
		 * not a problem.
		 */
	}
	ATF_REQUIRE_MSG(i >= 1, "No successful connections");
	ATF_REQUIRE_MSG(got_bind_error == true, "No expected bind error");

	ATF_REQUIRE(close(lsock) == 0);
}
ATF_TC_CLEANUP(tcp_v4mapped_bind, tc)
{
	restore_portrange();
}

ATF_TC(udp_v4mapped_sendto);
ATF_TC_HEAD(udp_v4mapped_sendto, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Validate sendto() with a v4-mapped address and a v6-only socket");
}
ATF_TC_BODY(udp_v4mapped_sendto, tc)
{
	struct addrinfo ai_hint, *aip;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	ssize_t n;
	socklen_t salen;
	int error, ls, s, zero;
	short port;
	char ch;

	ls = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(ls >= 0);

	memset(&ai_hint, 0, sizeof(ai_hint));
	ai_hint.ai_family = AF_INET;
	ai_hint.ai_flags = AI_NUMERICHOST;
	error = getaddrinfo("127.0.0.1", NULL, &ai_hint, &aip);
	ATF_REQUIRE_MSG(error == 0, "getaddrinfo: %s", gai_strerror(error));
	memcpy(&sin, aip->ai_addr, sizeof(sin));

	error = bind(ls, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind: %s", strerror(errno));
	salen = sizeof(sin);
	error = getsockname(ls, (struct sockaddr *)&sin, &salen);
	ATF_REQUIRE_MSG(error == 0,
	    "getsockname() for listen socket failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(salen == sizeof(struct sockaddr_in),
	    "unexpected sockaddr size");
	port = sin.sin_port;

	s = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(s >= 0);

	memset(&ai_hint, 0, sizeof(ai_hint));
	ai_hint.ai_family = AF_INET6;
	ai_hint.ai_flags = AI_NUMERICHOST | AI_V4MAPPED;
	error = getaddrinfo("127.0.0.1", NULL, &ai_hint, &aip);
	ATF_REQUIRE_MSG(error == 0, "getaddrinfo: %s", gai_strerror(error));
	memcpy(&sin6, aip->ai_addr, sizeof(sin6));
	sin6.sin6_port = port;
	freeaddrinfo(aip);

	ch = 0x42;
	n = sendto(s, &ch, 1, 0, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_ERRNO(EINVAL, n == -1);

	zero = 0;
	error = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero));
	ATF_REQUIRE_MSG(error == 0,
	    "setsockopt(IPV6_V6ONLY) failed: %s", strerror(errno));

	ch = 0x42;
	n = sendto(s, &ch, 1, 0, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(n == 1, "sendto() failed: %s", strerror(errno));

	ch = 0;
	n = recv(ls, &ch, 1, 0);
	ATF_REQUIRE_MSG(n == 1, "recv() failed: %s", strerror(errno));
	ATF_REQUIRE(ch == 0x42);

	ATF_REQUIRE(close(s) == 0);
	ATF_REQUIRE(close(ls) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, tcp_v4mapped_bind);
	ATF_TP_ADD_TC(tp, udp_v4mapped_sendto);

	return (atf_no_error());
}
