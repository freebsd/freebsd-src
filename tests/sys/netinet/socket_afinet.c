/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Bjoern A. Zeeb
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
__FBSDID("$FreeBSD$");

#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(socket_afinet);
ATF_TC_BODY(socket_afinet, tc)
{
	int sd;

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_CHECK(sd >= 0);

	close(sd);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_bind_zero);
ATF_TC_BODY(socket_afinet_bind_zero, tc)
{
	int sd, rc;
	struct sockaddr_in sin;

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_CHECK(sd >= 0);

	bzero(&sin, sizeof(sin));
	/*
	 * For AF_INET we do not check the family in in_pcbbind_setup(9),
	 * sa_len gets set from the syscall argument in getsockaddr(9),
	 * so we bind to 0:0.
	 */
	rc = bind(sd, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);

	close(sd);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_bind_ok);
ATF_TC_BODY(socket_afinet_bind_ok, tc)
{
	int sd, rc;
	struct sockaddr_in sin;

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_CHECK(sd >= 0);

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(6666);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	rc = bind(sd, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);

	close(sd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, socket_afinet);
	ATF_TP_ADD_TC(tp, socket_afinet_bind_zero);
	ATF_TP_ADD_TC(tp, socket_afinet_bind_ok);

	return atf_no_error();
}
