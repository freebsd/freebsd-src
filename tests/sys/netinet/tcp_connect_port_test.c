/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#define	SYSCTLBAKFILE	"tmp.net.inet.ip.portrange.randomized"

/*
 * Check if port allocation is randomized. If so, update it. Save the old
 * value of the sysctl so it can be updated later.
 */
static void
disable_random_ports(void)
{
	int error, fd, random_new, random_save;
	size_t sysctlsz;

	/*
	 * Pre-emptively unlink our restoration file, so we will do no
	 * restoration on error.
	 */
	unlink(SYSCTLBAKFILE);

	/*
	 * Disable the net.inet.ip.portrange.randomized sysctl. Save the
	 * old value so we can restore it, if necessary.
	 */
	random_new = 0;
	sysctlsz = sizeof(random_save);
	error = sysctlbyname("net.inet.ip.portrange.randomized", &random_save,
	    &sysctlsz, &random_new, sizeof(random_new));
	if (error) {
		warn("sysctlbyname(\"net.inet.ip.portrange.randomized\") "
		    "failed");
		atf_tc_skip("Unable to set sysctl");
	}
	if (sysctlsz != sizeof(random_save)) {
		fprintf(stderr, "Error: unexpected sysctl value size "
		    "(expected %zu, actual %zu)\n", sizeof(random_save),
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
	error = write(fd, &random_save, sizeof(random_save));
	if (error < 0) {
		warn("error writing saved value to sysctl backup file");
		goto cleanup_and_restore;
	}
	if (error != (int)sizeof(random_save)) {
		fprintf(stderr,
		    "Error writing saved value to sysctl backup file: "
		    "(expected %zu, actual %d)\n", sizeof(random_save), error);
		goto cleanup_and_restore;
	}
	error = close(fd);
	if (error) {
		warn("error closing sysctl backup file");
cleanup_and_restore:
		(void)close(fd);
		(void)unlink(SYSCTLBAKFILE);
restore_sysctl:
		(void)sysctlbyname("net.inet.ip.portrange.randomized", NULL,
		    NULL, &random_save, sysctlsz);
		atf_tc_skip("Error setting sysctl");
	}
}

/*
 * Restore the sysctl value from the backup file and delete the backup file.
 */
static void
restore_random_ports(void)
{
	int error, fd, random_save;

	/* Open the backup file, read the contents, close it, and delete it. */
	fd = open(SYSCTLBAKFILE, O_RDONLY);
	if (fd < 0) {
		warn("error opening sysctl backup file");
		return;
	}
	error = read(fd, &random_save, sizeof(random_save));
	if (error < 0) {
		warn("error reading saved value from sysctl backup file");
		return;
	}
	if (error != (int)sizeof(random_save)) {
		fprintf(stderr,
		    "Error reading saved value from sysctl backup file: "
		    "(expected %zu, actual %d)\n", sizeof(random_save), error);
		return;
	}
	error = close(fd);
	if (error)
		warn("error closing sysctl backup file");
	error = unlink(SYSCTLBAKFILE);
	if (error)
		warn("error removing sysctl backup file");

	/* Restore the saved sysctl value. */
	error = sysctlbyname("net.inet.ip.portrange.randomized", NULL, NULL,
	    &random_save, sizeof(random_save));
	if (error)
		warn("sysctlbyname(\"net.inet.ip.portrange.randomized\") "
		    "failed while restoring value");
}

/*
 * Given a domain and sockaddr, open a listening socket with automatic port
 * selection. Then, try to connect 64K times. Ensure the connected socket never
 * uses an overlapping port.
 */
static void
connect_loop(int domain, const struct sockaddr *addr)
{
	union {
		struct sockaddr saddr;
		struct sockaddr_in saddr4;
		struct sockaddr_in6 saddr6;
	} su_clnt, su_srvr;
	socklen_t salen;
	int asock, csock, error, i, lsock;
	const struct linger lopt = { 1, 0 };

	/*
	 * Disable the net.inet.ip.portrange.randomized sysctl. Assuming an
	 * otherwise idle system, this makes the kernel try all possible
	 * ports sequentially and makes it more likely it will try the
	 * port on which we have a listening socket.
	 */
	disable_random_ports();

	/* Setup the listen socket. */
	lsock = socket(domain, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(lsock >= 0, "socket() for listen socket failed: %s",
	    strerror(errno));
	error = bind(lsock, addr, addr->sa_len);
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	error = listen(lsock, 1);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));

	/*
	 * Get the address of the listen socket, which will be the destination
	 * address for our connection attempts.
	 */
	salen = sizeof(su_srvr);
	error = getsockname(lsock, &su_srvr.saddr, &salen);
	ATF_REQUIRE_MSG(error == 0,
	    "getsockname() for listen socket failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(salen == (domain == PF_INET ?
	    sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)),
	    "unexpected sockaddr size");
	ATF_REQUIRE_MSG(su_srvr.saddr.sa_len == (domain == PF_INET ?
	    sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)),
	    "unexpected sa_len size");

	/* Open 64K connections in a loop. */
	for (i = 0; i < 65536; i++) {
		csock = socket(domain, SOCK_STREAM, 0);
		ATF_REQUIRE_MSG(csock >= 0,
		    "socket() for client socket %d failed: %s",
		    i, strerror(errno));

		error = connect(csock, &su_srvr.saddr, su_srvr.saddr.sa_len);
		ATF_REQUIRE_MSG(error == 0,
		    "connect() for client socket %d failed: %s",
		    i, strerror(errno));

		error = setsockopt(csock, SOL_SOCKET, SO_LINGER, &lopt,
		    sizeof(lopt));
		ATF_REQUIRE_MSG(error == 0,
		    "Setting linger for client socket %d failed: %s",
		    i, strerror(errno));

		/* Ascertain the client socket address. */
		salen = sizeof(su_clnt);
		error = getsockname(csock, &su_clnt.saddr, &salen);
		ATF_REQUIRE_MSG(error == 0,
		    "getsockname() for client socket %d failed: %s",
		    i, strerror(errno));
		ATF_REQUIRE_MSG(salen == (domain == PF_INET ?
		    sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)),
		    "unexpected sockaddr size for client socket %d", i);

		/* Ensure the ports do not match. */
		switch (domain) {
		case PF_INET:
			ATF_REQUIRE_MSG(su_clnt.saddr4.sin_port !=
			    su_srvr.saddr4.sin_port,
			    "client socket %d using the same port as server",
			    i);
			break;
		case PF_INET6:
			ATF_REQUIRE_MSG(su_clnt.saddr6.sin6_port !=
			    su_srvr.saddr6.sin6_port,
			    "client socket %d using the same port as server",
			    i);
			break;
		}

		/* Accept the socket and close both ends. */
		asock = accept(lsock, NULL, NULL);
		ATF_REQUIRE_MSG(asock >= 0,
		    "accept() failed for client socket %d: %s",
		    i, strerror(errno));

		error = close(asock);
		ATF_REQUIRE_MSG(error == 0,
		    "close() failed for accepted socket %d: %s",
		    i, strerror(errno));

		error = close(csock);
		ATF_REQUIRE_MSG(error == 0,
		    "close() failed for client socket %d: %s",
		    i, strerror(errno));
	}
}

ATF_TC_WITH_CLEANUP(basic_ipv4);
ATF_TC_HEAD(basic_ipv4, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "allow_sysctl_side_effects");
	atf_tc_set_md_var(tc, "descr",
	    "Check automatic local port assignment during TCP connect calls");
}

ATF_TC_BODY(basic_ipv4, tc)
{
	struct sockaddr_in saddr4;

	memset(&saddr4, 0, sizeof(saddr4));
	saddr4.sin_len = sizeof(saddr4);
	saddr4.sin_family = AF_INET;
	saddr4.sin_port = htons(0);
	saddr4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	connect_loop(PF_INET, (const struct sockaddr *)&saddr4);
}

ATF_TC_CLEANUP(basic_ipv4, tc)
{

	restore_random_ports();
}

ATF_TC_WITH_CLEANUP(basic_ipv6);
ATF_TC_HEAD(basic_ipv6, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "allow_sysctl_side_effects");
	atf_tc_set_md_var(tc, "descr",
	    "Check automatic local port assignment during TCP connect calls");
}

ATF_TC_BODY(basic_ipv6, tc)
{
	struct sockaddr_in6 saddr6;

	memset(&saddr6, 0, sizeof(saddr6));
	saddr6.sin6_len = sizeof(saddr6);
	saddr6.sin6_family = AF_INET6;
	saddr6.sin6_port = htons(0);
	saddr6.sin6_addr = in6addr_loopback;

	connect_loop(PF_INET6, (const struct sockaddr *)&saddr6);
}

ATF_TC_CLEANUP(basic_ipv6, tc)
{

	restore_random_ports();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, basic_ipv4);
	ATF_TP_ADD_TC(tp, basic_ipv6);

	return (atf_no_error());
}

