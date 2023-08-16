/*-
 * Copyright (c) 2021 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
#include <sys/param.h>
#include <sys/physmem.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(hwregion);
ATF_TC_BODY(hwregion, tc)
{
	vm_paddr_t avail[4];
	size_t len;

	physmem_hardware_region(2 * PAGE_SIZE, PAGE_SIZE);

	len = physmem_avail(avail, 4);
	ATF_CHECK_EQ(len, 2);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 3 * PAGE_SIZE);

	/* Add an overlap */
	physmem_hardware_region(2 * PAGE_SIZE, 2 * PAGE_SIZE);
	len = physmem_avail(avail, 4);
	ATF_CHECK_EQ(len, 2);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 4 * PAGE_SIZE);

	/* Add a touching region */
	physmem_hardware_region(4 * PAGE_SIZE, PAGE_SIZE);
	len = physmem_avail(avail, 4);
	ATF_CHECK_EQ(len, 2);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 5 * PAGE_SIZE);

	/* Add a partial overlap */
	physmem_hardware_region(4 * PAGE_SIZE, 2 * PAGE_SIZE);
	len = physmem_avail(avail, 4);
	ATF_CHECK_EQ(len, 2);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 6 * PAGE_SIZE);

	/* Add a non-page aligned section */
	physmem_hardware_region(6 * PAGE_SIZE, PAGE_SIZE / 2);
	len = physmem_avail(avail, 4);
	ATF_CHECK_EQ(len, 2);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 6 * PAGE_SIZE);

	/* Add the remaining part of the page */
	physmem_hardware_region(6 * PAGE_SIZE + PAGE_SIZE / 2, PAGE_SIZE / 2);
	len = physmem_avail(avail, 4);
	ATF_CHECK_EQ(len, 2);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 7 * PAGE_SIZE);
}

ATF_TC_WITHOUT_HEAD(hwregion_exclude);
ATF_TC_BODY(hwregion_exclude, tc)
{
	vm_paddr_t avail[6];
	size_t len;

	physmem_hardware_region(2 * PAGE_SIZE, 5 * PAGE_SIZE);
	physmem_exclude_region(4 * PAGE_SIZE, PAGE_SIZE, EXFLAG_NOALLOC);

	len = physmem_avail(avail, 6);
	ATF_CHECK_EQ(len, 4);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 4 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[2], 5 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[3], 7 * PAGE_SIZE);

	/* Check mis-aligned/sized excluded regions work */
	physmem_exclude_region(4 * PAGE_SIZE - 1, PAGE_SIZE + 2,
	    EXFLAG_NOALLOC);
	len = physmem_avail(avail, 6);
	ATF_CHECK_EQ(len, 4);
	ATF_CHECK_EQ(avail[0], 2 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 3 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[2], 6 * PAGE_SIZE);
	ATF_CHECK_EQ(avail[3], 7 * PAGE_SIZE);
}

ATF_TC_WITHOUT_HEAD(hwregion_unordered);
ATF_TC_BODY(hwregion_unordered, tc)
{
	vm_paddr_t avail[4];
	size_t len;

	/* Add a partial page */
	physmem_hardware_region(PAGE_SIZE, PAGE_SIZE / 2);
	/* Add a full page not touching the previous */
	physmem_hardware_region( 2 * PAGE_SIZE, PAGE_SIZE);
	/* Add the remainder of the first page */
	physmem_hardware_region(PAGE_SIZE + PAGE_SIZE / 2, PAGE_SIZE / 2);

	len = physmem_avail(avail, 4);
	ATF_CHECK_EQ(len, 2);
	ATF_CHECK_EQ(avail[0], PAGE_SIZE);
	ATF_CHECK_EQ(avail[1], 3 * PAGE_SIZE);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, hwregion);
	ATF_TP_ADD_TC(tp, hwregion_exclude);
	ATF_TP_ADD_TC(tp, hwregion_unordered);
	return (atf_no_error());
}
