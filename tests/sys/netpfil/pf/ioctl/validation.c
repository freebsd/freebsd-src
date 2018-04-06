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

void
common_init_tbl(struct pfr_table *tbl)
{
	bzero(tbl, sizeof(struct pfr_table));
	strcpy(tbl->pfrt_anchor, "anchor");
	strcpy(tbl->pfrt_name, "name");
	tbl->pfrt_flags = 0;
	tbl->pfrt_fback = 0;
}

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

ATF_TC_WITHOUT_HEAD(gettables);
ATF_TC_BODY(gettables, tc)
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

	/* Negative size. This will succeed, because the kernel will not copy
	 * tables than it has. */
	io.pfrio_size = -1;
	if (ioctl(dev, DIOCRGETTABLES, &io) != 0)
		atf_tc_fail("Request with size -1 failed");

	/* Overly large size. See above. */
	io.pfrio_size = 1 << 24;
	if (ioctl(dev, DIOCRGETTABLES, &io) != 0)
		atf_tc_fail("Request with size 1 << 24 failed");

	COMMON_CLEANUP();
}

ATF_TC_WITHOUT_HEAD(gettstats);
ATF_TC_BODY(gettstats, tc)
{
	struct pfioc_table io;
	struct pfr_tstats stats;
	int flags;

	COMMON_HEAD();

	flags = 0;

	bzero(&io, sizeof(io));
	io.pfrio_flags = flags;
	io.pfrio_buffer = &stats;
	io.pfrio_esize = sizeof(stats);

	/* Negative size. This will succeed, because the kernel will not copy
	 * tables than it has. */
	io.pfrio_size = -1;
	if (ioctl(dev, DIOCRGETTSTATS, &io) != 0)
		atf_tc_fail("Request with size -1 failed");

	/* Overly large size. See above. */
	io.pfrio_size = 1 << 24;
	if (ioctl(dev, DIOCRGETTSTATS, &io) != 0)
		atf_tc_fail("Request with size 1 << 24 failed");

	COMMON_CLEANUP();
}

ATF_TC_WITHOUT_HEAD(clrtstats);
ATF_TC_BODY(clrtstats, tc)
{
	struct pfioc_table io;
	struct pfr_table tbl;
	int flags;

	COMMON_HEAD();

	flags = 0;

	common_init_tbl(&tbl);

	bzero(&io, sizeof(io));
	io.pfrio_flags = flags;
	io.pfrio_buffer = &tbl;
	io.pfrio_esize = sizeof(tbl);

	/* Negative size. This will succeed, because the kernel will not copy
	 * tables than it has. */
	io.pfrio_size = -1;
	if (ioctl(dev, DIOCRCLRTSTATS, &io) != 0)
		atf_tc_fail("Request with size -1 failed ");

	/* Overly large size. See above. */
	io.pfrio_size = 1 << 24;
	if (ioctl(dev, DIOCRCLRTSTATS, &io) != 0)
		atf_tc_fail("Request with size 1 << 24 failed");

	COMMON_CLEANUP();
}

ATF_TC_WITHOUT_HEAD(settflags);
ATF_TC_BODY(settflags, tc)
{
	struct pfioc_table io;
	struct pfr_table tbl;
	int flags;

	COMMON_HEAD();

	flags = 0;

	common_init_tbl(&tbl);

	bzero(&io, sizeof(io));
	io.pfrio_flags = flags;
	io.pfrio_buffer = &tbl;
	io.pfrio_esize = sizeof(tbl);

	/* Negative size. This will succeed, because the kernel will not copy
	 * tables than it has. */
	io.pfrio_size = -1;
	if (ioctl(dev, DIOCRSETTFLAGS, &io) != 0)
		atf_tc_fail("Request with size -1 failed");

	/* Overly large size. See above. */
	io.pfrio_size = 1 << 28;
	if (ioctl(dev, DIOCRSETTFLAGS, &io) != 0)
		atf_tc_fail("Request with size 1 << 24 failed");

	COMMON_CLEANUP();
}

ATF_TC_WITHOUT_HEAD(igetifaces);
ATF_TC_BODY(igetifaces, tc)
{
	struct pfioc_iface io;
	struct pfi_kif kif;

	COMMON_HEAD();

	bzero(&io, sizeof(io));
	io.pfiio_flags = 0;
	io.pfiio_buffer = &kif;
	io.pfiio_esize = sizeof(kif);

	/* Negative size */
	io.pfiio_size = -1;
	if (ioctl(dev, DIOCIGETIFACES, &io) == 0)
		atf_tc_fail("request with size -1 succeeded");

	/* Overflow size */
	io.pfiio_size = 1 << 31;
	if (ioctl(dev, DIOCIGETIFACES, &io) == 0)
		atf_tc_fail("request with size 1 << 31 succeeded");

	COMMON_CLEANUP();
}

ATF_TC_WITHOUT_HEAD(commit);
ATF_TC_BODY(commit, tc)
{
	struct pfioc_trans io;
	struct pfioc_trans_e ioe;

	COMMON_HEAD();

	bzero(&io, sizeof(io));
	io.esize = sizeof(ioe);
	io.array = &ioe;

	/* Negative size */
	io.size = -1;
	if (ioctl(dev, DIOCXCOMMIT, &io) == 0)
		atf_tc_fail("request with size -1 succeeded");

	/* Overflow size */
	io.size = 1 << 30;
	if (ioctl(dev, DIOCXCOMMIT, &io) == 0)
		atf_tc_fail("request with size 1 << 30 succeeded");

	/* NULL buffer */
	io.size = 1;
	io.array = NULL;
	if (ioctl(dev, DIOCXCOMMIT, &io) == 0)
		atf_tc_fail("request with size -1 succeeded");

	COMMON_CLEANUP();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, addtables);
	ATF_TP_ADD_TC(tp, deltables);
	ATF_TP_ADD_TC(tp, gettables);
	ATF_TP_ADD_TC(tp, gettstats);
	ATF_TP_ADD_TC(tp, clrtstats);
	ATF_TP_ADD_TC(tp, settflags);
	ATF_TP_ADD_TC(tp, igetifaces);
	ATF_TP_ADD_TC(tp, commit);

	return (atf_no_error());
}
