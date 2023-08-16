/*-
 * Copyright (C) 2021 Axcient, Inc. All rights reserved.
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

/* Tests that alter an enclosure's state */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <atf-c.h>
#include <fcntl.h>
#include <glob.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cam/scsi/scsi_enc.h>

#include "common.h"

// Run a test function on just one ses device
static void
for_one_ses_dev(ses_cb cb)
{
	glob_t g;
	int fd, r;

	g.gl_pathc = 0;
	g.gl_pathv = NULL;
	g.gl_offs = 0;

	r = glob("/dev/ses*", GLOB_NOCHECK | GLOB_NOSORT, NULL, &g);
	ATF_REQUIRE_EQ(r, 0);
	if (g.gl_matchc == 0)
		return;

	fd = open(g.gl_pathv[0], O_RDWR);
	ATF_REQUIRE(fd >= 0);
	cb(g.gl_pathv[0], fd);
	close(fd);

	globfree(&g);
}

static bool
do_setelmstat(const char *devname __unused, int fd)
{
	encioc_element_t *map;
	unsigned elm_idx;
	unsigned nobj;
	int r;
	elm_type_t last_elm_type = -1;

	r = ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj);
	ATF_REQUIRE_EQ(r, 0);

	map = calloc(nobj, sizeof(encioc_element_t));
	ATF_REQUIRE(map != NULL);
	r = ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) map);

	/* Set the IDENT bit for every disk slot */
	for (elm_idx = 0; elm_idx < nobj; elm_idx++) {
		encioc_elm_status_t elmstat;
		struct ses_ctrl_dev_slot *cslot;

		if (last_elm_type != map[elm_idx].elm_type) {
			/* skip overall elements */
			last_elm_type = map[elm_idx].elm_type;
			continue;
		}
		elmstat.elm_idx = elm_idx;
		if (map[elm_idx].elm_type == ELMTYP_DEVICE ||
		    map[elm_idx].elm_type == ELMTYP_ARRAY_DEV)
		{
			r = ioctl(fd, ENCIOC_GETELMSTAT, (caddr_t)&elmstat);
			ATF_REQUIRE_EQ(r, 0);
			ses_status_to_ctrl(map[elm_idx].elm_type,
				&elmstat.cstat[0]);

			cslot = (struct ses_ctrl_dev_slot*)&elmstat.cstat[0];

			ses_ctrl_common_set_select(&cslot->common, 1);
			ses_ctrl_dev_slot_set_rqst_ident(cslot, 1);
			r = ioctl(fd, ENCIOC_SETELMSTAT, (caddr_t)&elmstat);
			ATF_REQUIRE_EQ(r, 0);
		}
	}

	/* Check the IDENT bit for every disk slot */
	last_elm_type = -1;
	for (elm_idx = 0; elm_idx < nobj; elm_idx++) {
		encioc_elm_status_t elmstat;
		struct ses_status_dev_slot *sslot =
			(struct ses_status_dev_slot*)&elmstat.cstat[0];

		if (last_elm_type != map[elm_idx].elm_type) {
			/* skip overall elements */
			last_elm_type = map[elm_idx].elm_type;
			continue;
		}
		elmstat.elm_idx = elm_idx;
		if (map[elm_idx].elm_type == ELMTYP_DEVICE ||
		    map[elm_idx].elm_type == ELMTYP_ARRAY_DEV)
		{
			int i;

			for (i = 0; i < 10; i++) {
				r = ioctl(fd, ENCIOC_GETELMSTAT,
				    (caddr_t)&elmstat);
				ATF_REQUIRE_EQ(r, 0);
				if (0 == ses_status_dev_slot_get_ident(sslot)) {
					/* Needs more time to take effect */
					usleep(100000);
				}
			}
			ATF_CHECK(ses_status_dev_slot_get_ident(sslot) != 0);

		}
	}

	free(map);
	return (true);
}

/*
 * sg_ses doesn't provide "dump and restore" functionality.  The closest is to
 * dump status page 2, then manually edit the file to set every individual
 * element select bit, then load the entire file.  But that is much too hard.
 * Instead, we'll just clear every ident bit.
 */
static bool
do_setelmstat_cleanup(const char *devname __unused, int fd __unused)
{
	encioc_element_t *map;
	unsigned elm_idx;
	unsigned nobj;
	int r;
	elm_type_t last_elm_type = -1;

	r = ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj);
	ATF_REQUIRE_EQ(r, 0);

	map = calloc(nobj, sizeof(encioc_element_t));
	ATF_REQUIRE(map != NULL);
	r = ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) map);
	ATF_REQUIRE_EQ(r, 0);

	/* Clear the IDENT bit for every disk slot */
	for (elm_idx = 0; elm_idx < nobj; elm_idx++) {
		encioc_elm_status_t elmstat;
		struct ses_ctrl_dev_slot *cslot;

		if (last_elm_type != map[elm_idx].elm_type) {
			/* skip overall elements */
			last_elm_type = map[elm_idx].elm_type;
			continue;
		}
		elmstat.elm_idx = elm_idx;
		if (map[elm_idx].elm_type == ELMTYP_DEVICE ||
		    map[elm_idx].elm_type == ELMTYP_ARRAY_DEV)
		{
			r = ioctl(fd, ENCIOC_GETELMSTAT, (caddr_t)&elmstat);
			ATF_REQUIRE_EQ(r, 0);
			ses_status_to_ctrl(map[elm_idx].elm_type,
			    &elmstat.cstat[0]);

			cslot = (struct ses_ctrl_dev_slot*)&elmstat.cstat[0];

			ses_ctrl_common_set_select(&cslot->common, 1);
			ses_ctrl_dev_slot_set_rqst_ident(cslot, 0);
			r = ioctl(fd, ENCIOC_SETELMSTAT, (caddr_t)&elmstat);
			ATF_REQUIRE_EQ(r, 0);
		}
	}

	return(true);
}


ATF_TC_WITH_CLEANUP(setelmstat);
ATF_TC_HEAD(setelmstat, tc)
{
	atf_tc_set_md_var(tc, "descr", "Exercise ENCIOC_SETELMSTAT");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(setelmstat, tc)
{
	if (!has_ses())
		atf_tc_skip("No ses devices found");

	for_one_ses_dev(do_setelmstat);
}
ATF_TC_CLEANUP(setelmstat, tc)
{
	if (!has_ses())
		return;

	for_one_ses_dev(do_setelmstat_cleanup);
}


static bool
do_setencstat(const char *devname __unused, int fd)
{
	unsigned char encstat;
	int r, i;
	bool worked = false;

	/*
	 * SES provides no way to read the current setting of the enclosure
	 * control page common status bits.  So we'll blindly set CRIT.
	 */
	encstat = 1 << SES_CTRL_PAGE_CRIT_SHIFT;
	r = ioctl(fd, ENCIOC_SETENCSTAT, (caddr_t) &encstat);
	ATF_REQUIRE_EQ(r, 0);

	/* Check that the status has changed */
	for (i = 0; i < 10; i++) {
		r = ioctl(fd, ENCIOC_GETENCSTAT, (caddr_t) &encstat);
		ATF_REQUIRE_EQ(r, 0);
		if (encstat & SES_CTRL_PAGE_CRIT_MASK) {
			worked = true;
			break;
		}
		usleep(100000);
	}
	if (!worked) {
		/* Some enclosures don't support setting the enclosure status */
		return (false);
	} else
		return (true);
}

static bool
do_setencstat_cleanup(const char *devname __unused, int fd)
{
	unsigned char encstat;

	/*
	 * SES provides no way to read the current setting of the enclosure
	 * control page common status bits.  So we don't know what they were
	 * set to before the test.  We'll blindly clear all bits.
	 */
	encstat = 0;
	ioctl(fd, ENCIOC_SETENCSTAT, (caddr_t) &encstat);
	return (true);
}

ATF_TC_WITH_CLEANUP(setencstat);
ATF_TC_HEAD(setencstat, tc)
{
	atf_tc_set_md_var(tc, "descr", "Exercise ENCIOC_SETENCSTAT");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(setencstat, tc)
{
	if (!has_ses())
		atf_tc_skip("No ses devices found");

	for_each_ses_dev(do_setencstat, O_RDWR);
}
ATF_TC_CLEANUP(setencstat, tc)
{
	for_each_ses_dev(do_setencstat_cleanup, O_RDWR);
}

ATF_TP_ADD_TCS(tp)
{

	/*
	 * Untested ioctls:
	 *
	 * * ENCIOC_INIT because SES doesn't need it and I don't have any
	 *   SAF-TE devices.
	 *
	 * * ENCIOC_SETSTRING because it's seriously unsafe!  It's normally
	 *   used for stuff like firmware updates
	 */
	ATF_TP_ADD_TC(tp, setelmstat);
	ATF_TP_ADD_TC(tp, setencstat);

	return (atf_no_error());
}
