/*	$NetBSD: t_posix_memalign.c,v 1.2 2011/07/07 11:12:18 jruoho Exp $ */

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2008\
 The NetBSD Foundation, inc. All rights reserved.");
__RCSID("$NetBSD: t_posix_memalign.c,v 1.2 2011/07/07 11:12:18 jruoho Exp $");

#include <atf-c.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ATF_TC(posix_memalign_basic);
ATF_TC_HEAD(posix_memalign_basic, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks posix_memalign(3)");
}
ATF_TC_BODY(posix_memalign_basic, tc)
{
	size_t size[] = {
		1, 2, 3, 4, 10, 100, 16384, 32768, 65536
	};
	size_t align[] = {
		512, 1024, 16, 32, 64, 4, 2048, 16, 2
	};

	size_t i;
	void *p;

	for (i = 0; i < __arraycount(size); i++) {
		int ret;
		p = (void*)0x1;

		(void)printf("Checking posix_memalign(&p, %zd, %zd)...\n",
			align[i], size[i]);
		ret = posix_memalign(&p, align[i], size[i]);

		if ( align[i] < sizeof(void *))
			ATF_REQUIRE_EQ_MSG(ret, EINVAL,
			    "posix_memalign: %s", strerror(ret));
		else {
			ATF_REQUIRE_EQ_MSG(ret, 0,
			    "posix_memalign: %s", strerror(ret));
			ATF_REQUIRE_EQ_MSG(((intptr_t)p) & (align[i] - 1), 0,
			    "p = %p", p);
			free(p);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, posix_memalign_basic);

	return atf_no_error();
}
