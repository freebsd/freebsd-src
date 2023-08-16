/*-
 * Copyright (c) 2020   Kristof Provost <kp@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>

#include <atf-c.h>
#include "freebsd_test_suite/macros.h"

ATF_TC(params);
ATF_TC_HEAD(params, tc)
{
        atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(params, tc)
{
	struct ifreq ifr;
	int s;

	kldload("if_epair");
	ATF_REQUIRE_KERNEL_MODULE("if_epair");

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		atf_tc_fail("Failed to create socket");

        bzero(&ifr, sizeof(ifr));
	ifr.ifr_data = (caddr_t)-1;
        (void) strlcpy(ifr.ifr_name, "epair", sizeof(ifr.ifr_name));

	if (ioctl(s, SIOCIFCREATE2, &ifr) < 0)
		atf_tc_fail("Failed to create interface");

	if (ioctl(s, SIOCIFDESTROY, &ifr) < 0)
		atf_tc_fail("Failed to destroy interface");
}

ATF_TP_ADD_TCS(tp)
{
        ATF_TP_ADD_TC(tp, params);

	return (atf_no_error());
}
