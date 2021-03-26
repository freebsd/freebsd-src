/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Ryan Moeller <freqlabs@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/capsicum.h>
#include <sys/nv.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>
#include <casper/cap_netdb.h>

#include <atf-c.h>

static cap_channel_t *
initcap(void)
{
	cap_channel_t *capcas, *capnetdb;

	capcas = cap_init();
	ATF_REQUIRE(capcas != NULL);

	capnetdb = cap_service_open(capcas, "system.netdb");
	ATF_REQUIRE(capnetdb != NULL);

	cap_close(capcas);

	return (capnetdb);
}

ATF_TC_WITHOUT_HEAD(cap_netdb__getprotobyname);
ATF_TC_BODY(cap_netdb__getprotobyname, tc)
{
	cap_channel_t *capnetdb;
	struct protoent *pp;
	size_t n = 0;

	capnetdb = initcap();

	pp = cap_getprotobyname(capnetdb, "tcp");
	ATF_REQUIRE(pp != NULL);

	ATF_REQUIRE(pp->p_name != NULL);
	ATF_REQUIRE(pp->p_aliases != NULL);
	while (pp->p_aliases[n] != NULL)
		++n;
	ATF_REQUIRE(n > 0);
	ATF_REQUIRE(pp->p_proto != 0);

	cap_close(capnetdb);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_netdb__getprotobyname);

	return (atf_no_error());
}
