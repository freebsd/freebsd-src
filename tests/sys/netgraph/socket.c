/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <sys/socket.h>
#include <netgraph.h>
#include <netgraph/ng_socket.h>

#include <errno.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(getsockname);
ATF_TC_BODY(getsockname, tc)
{
	struct sockaddr_ng sg;
	socklen_t len = sizeof(struct sockaddr_ng);
#define	NAME	"0123456789012345678901234567891"	/* 31 chars */
	char name[NG_NODESIZ] = NAME;
	int cs;

	/* Unnamed node returns its ID as name. */
	ATF_REQUIRE(NgMkSockNode(NULL, &cs, NULL) == 0);
	ATF_REQUIRE(getsockname(cs, (struct sockaddr *)&sg, &len) == 0);
	ATF_REQUIRE(strspn(sg.sg_data, "[0123456789abcdef]") >= 3 &&
	    sg.sg_data[strspn(sg.sg_data, "[0123456789abcdef]")] == '\0');
	close(cs);

	/* Named node. */
	ATF_REQUIRE(NgMkSockNode(name, &cs, NULL) == 0);
	ATF_REQUIRE(getsockname(cs, (struct sockaddr *)&sg, &len) == 0);
	ATF_REQUIRE(strcmp(sg.sg_data, NAME) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, getsockname);

	return (atf_no_error());
}
