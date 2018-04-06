/*-
 * Copyright (c) 2018	Kristof Provost <kp@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/pfvar.h>

#include <fcntl.h>
#include <stdio.h>

#include <atf-c.h>

static int dev;

#define COMMON_HEAD() \
	if (modfind("pf") == -1) \
		atf_tc_skip("pf not loaded"); \
	dev = open("/dev/pf", O_RDWR); \
	if (dev == -1) \
		atf_tc_skip("Failed to open /dev/pf");

#define COMMON_CLEANUP() \
	close(dev);

ATF_TC_WITHOUT_HEAD(addtables);
ATF_TC_BODY(addtables, tc)
{
	struct pfioc_table io;
	struct pfr_table tbl;
	int flags;

	COMMON_HEAD();

	flags = 0;

	bzero(&io, sizeof(io));
	io.pfrio_flags = flags;
	io.pfrio_buffer = &tbl;
	io.pfrio_esize = sizeof(tbl);

	/* Negative size */
	io.pfrio_size = -1;
	if (ioctl(dev, DIOCRADDTABLES, &io) == 0)
		atf_tc_fail("Request with size -1 succeeded");

	/* Overly large size */
	io.pfrio_size = 1 << 24;
	if (ioctl(dev, DIOCRADDTABLES, &io) == 0)
		atf_tc_fail("Request with size 1 << 24 succeeded");

	/* NULL buffer */
	io.pfrio_size = 1;
	io.pfrio_buffer = NULL;
	if (ioctl(dev, DIOCRADDTABLES, &io) == 0)
		atf_tc_fail("Request with NULL buffer succeeded");

	COMMON_CLEANUP();
}

ATF_TC_WITHOUT_HEAD(deltables);
ATF_TC_BODY(deltables, tc)
{
	struct pfioc_table io;
	struct pfr_table tbl;
	int flags;

	COMMON_HEAD();

	flags = 0;

	bzero(&io, sizeof(io));
	io.pfrio_flags = flags;
	io.pfrio_buffer = &tbl;
	io.pfrio_esize = sizeof(tbl);

	/* Negative size */
	io.pfrio_size = -1;
	if (ioctl(dev, DIOCRDELTABLES, &io) == 0)
		atf_tc_fail("Request with size -1 succeeded");

	/* Overly large size */
	io.pfrio_size = 1 << 24;
	if (ioctl(dev, DIOCRDELTABLES, &io) == 0)
		atf_tc_fail("Request with size 1 << 24 succeeded");

	/* NULL buffer */
	io.pfrio_size = 1;
	io.pfrio_buffer = NULL;
	if (ioctl(dev, DIOCRDELTABLES, &io) == 0)
		atf_tc_fail("Request with NULL buffer succeeded");

	COMMON_CLEANUP();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, addtables);
	ATF_TP_ADD_TC(tp, deltables);

	return (atf_no_error());
}
