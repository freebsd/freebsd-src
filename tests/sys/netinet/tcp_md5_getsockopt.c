/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022, Klara Inc.
 * Copyright (c) 2022, Claudio Jeker <claudio@openbsd.org>
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

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <fcntl.h>
#include <unistd.h>
#include <err.h>

#include <atf-c.h>

void test_tcp_md5_getsockopt(int);

void
test_tcp_md5_getsockopt(int v6)
{
	if (kldfind("tcpmd5.ko") == -1)
		atf_tc_skip("Test requires the tcpmd5 kernel module to be loaded");

        struct sockaddr_in *s;
        struct sockaddr_in6 s6 = { 0 };
        struct sockaddr_in s4 = { 0 };
        socklen_t len;
        int csock, ssock, opt;
	int pf;

	if (v6) {
		pf = PF_INET6;
		len = sizeof(s6);

		s6.sin6_family = pf;
		s6.sin6_len = sizeof(s6);
		s6.sin6_addr = in6addr_loopback;
		s6.sin6_port = 0;

		s = (struct sockaddr_in *)&s6;
	} else {
		pf = PF_INET;
		len = sizeof(s4);

		s4.sin_family = pf;
		s4.sin_len = sizeof(s4);
		s4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		s4.sin_port = 0;

		s = &s4;
	}

        if ((ssock = socket(pf, SOCK_STREAM, 0)) == -1)
                atf_tc_fail("creating server socket");

	fcntl(ssock, F_SETFL, O_NONBLOCK);

	if ((bind(ssock, (struct sockaddr *)s, len) == -1))
		atf_tc_fail("binding to localhost");

	getsockname(ssock, (struct sockaddr *)s, &len);

	listen(ssock, 1);

        if ((csock = socket(pf, SOCK_STREAM, 0)) == -1)
                atf_tc_fail("creating client socket");

        if (connect(csock, (struct sockaddr *)s, len) == -1)
                atf_tc_fail("connecting to server instance");

        if (getsockopt(csock, IPPROTO_TCP, TCP_MD5SIG, &opt, &len) == -1)
                atf_tc_fail("getsockopt");

	close(csock);
	close(ssock);

	atf_tc_pass();
}

ATF_TC(tcp_md5_getsockopt_v4);
ATF_TC_HEAD(tcp_md5_getsockopt_v4, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test getsockopt for TCP MD5 SIG (IPv4)");
}

ATF_TC_BODY(tcp_md5_getsockopt_v4, tc)
{
	test_tcp_md5_getsockopt(0);
}

ATF_TC(tcp_md5_getsockopt_v6);
ATF_TC_HEAD(tcp_md5_getsockopt_v6, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test getsockopt for TCP MD5 SIG (IPv6)");
}

ATF_TC_BODY(tcp_md5_getsockopt_v6, tc)
{
	test_tcp_md5_getsockopt(1);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, tcp_md5_getsockopt_v4);
	ATF_TP_ADD_TC(tp, tcp_md5_getsockopt_v6);

	return atf_no_error();
}
