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
 *
 * $FreeBSD$
 */

/* Basic smoke test of the ioctl interface */

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

static bool do_getelmdesc(const char *devname, int fd) {
	regex_t re;
	FILE *pipe;
	char cmd[256];
	char line[256];
	char *actual;
	unsigned nobj;
	unsigned elm_idx = 0;
	int r;

	actual = calloc(UINT16_MAX, sizeof(char));
	ATF_REQUIRE(actual != NULL);
	r = regcomp(&re, "(Overall|Element [0-9]+) descriptor: ", REG_EXTENDED);
	ATF_REQUIRE_EQ(r, 0);

	r = ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj);
	ATF_REQUIRE_EQ(r, 0);

	snprintf(cmd, sizeof(cmd), "sg_ses -p7 %s", devname);
	pipe = popen(cmd, "r");
	ATF_REQUIRE(pipe != NULL);
	while(NULL != fgets(line, sizeof(line), pipe)) {
		regmatch_t matches[1];
		encioc_elm_desc_t e_desc;
		char *expected;
		size_t elen;

		if (regexec(&re, line, 1, matches, 0) == REG_NOMATCH) {
			continue;
		}

		expected = &line[matches[0].rm_eo];
		/* Remove trailing newline */
		elen = strnlen(expected, sizeof(line) - matches[0].rm_eo);
		expected[elen - 1] = '\0';
		/*
		 * Zero the result string.  XXX we wouldn't have to do this if
		 * the kernel would nul-terminate the result.
		 */
		memset(actual, 0, UINT16_MAX);
		e_desc.elm_idx = elm_idx;
		e_desc.elm_desc_len = UINT16_MAX;
		e_desc.elm_desc_str = actual;
		r = ioctl(fd, ENCIOC_GETELMDESC, (caddr_t) &e_desc);
		ATF_REQUIRE_EQ(r, 0);
		if (0 == strcmp("<empty>", expected)) {
			/* sg_ses replaces "" with "<empty>" */
			ATF_CHECK_STREQ("", actual);
		} else
			ATF_CHECK_STREQ(expected, actual);
		elm_idx++;
	}

	r = pclose(pipe);
	regfree(&re);
	free(actual);
	if (r != 0) {
		/* Probably an SGPIO device */

		return (false);
	} else {
		ATF_CHECK_EQ_MSG(nobj, elm_idx,
				"Did not find the expected number of element "
				"descriptors in sg_ses's output");
		return (true);
	}
}

ATF_TC(getelmdesc);
ATF_TC_HEAD(getelmdesc, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETELMDESC's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getelmdesc, tc)
{
	for_each_ses_dev(do_getelmdesc, O_RDONLY);
}

static bool do_getelmdevnames(const char *devname __unused, int fd) {
	encioc_element_t *map;
	unsigned nobj;
	const size_t namesize = 128;
	int r;
	char *namebuf;
	unsigned elm_idx;

	r = ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj);
	ATF_REQUIRE_EQ(r, 0);

	namebuf = calloc(namesize, sizeof(char));
	ATF_REQUIRE(namebuf != NULL);
	map = calloc(nobj, sizeof(encioc_element_t));
	ATF_REQUIRE(map != NULL);
	r = ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) map);
	ATF_REQUIRE_EQ(r, 0);

	for (elm_idx = 0; elm_idx < nobj; elm_idx++) {
		/*
		 * devnames should be present if:
		 * * The element is of type Device Slot or Array Device Slot
		 * * It isn't an Overall Element
		 * * The element's status is not "Not Installed"
		 */
		encioc_elm_status_t e_status;
		encioc_elm_devnames_t elmdn;

		memset(&e_status, 0, sizeof(e_status));
		e_status.elm_idx = elm_idx;
		r = ioctl(fd, ENCIOC_GETELMSTAT, (caddr_t)&e_status);
		ATF_REQUIRE_EQ(r, 0);

		memset(&elmdn, 0, sizeof(elmdn));
		elmdn.elm_idx = elm_idx;
		elmdn.elm_names_size = namesize;
		elmdn.elm_devnames = namebuf;
		namebuf[0] = '\0';
		r = ioctl(fd, ENCIOC_GETELMDEVNAMES, (caddr_t) &elmdn);
		if (e_status.cstat[0] != SES_OBJSTAT_UNSUPPORTED &&
		    e_status.cstat[0] != SES_OBJSTAT_NOTINSTALLED &&
		    (map[elm_idx].elm_type == ELMTYP_DEVICE ||
		     map[elm_idx].elm_type == ELMTYP_ARRAY_DEV))
		{
			ATF_CHECK_EQ_MSG(r, 0, "devnames not found.  This could be due to a buggy ses driver, buggy ses controller, dead HDD, or an ATA HDD in a SAS slot");
		} else {
			ATF_CHECK(r != 0);
		}

		if (r == 0) {
			size_t z = 0;
			int da = 0, ada = 0, pass = 0, nvd = 0;
			int nvme = 0, unknown = 0;

			while(elmdn.elm_devnames[z] != '\0') {
				size_t e;
				char *s;

				if (elmdn.elm_devnames[z] == ',')
					z++;	/* Skip the comma */
				s = elmdn.elm_devnames + z;
				e = strcspn(s, "0123456789");
				if (0 == strncmp("da", s, e))
					da++;
				else if (0 == strncmp("ada", s, e))
					ada++;
				else if (0 == strncmp("pass", s, e))
					pass++;
				else if (0 == strncmp("nvd", s, e))
					nvd++;
				else if (0 == strncmp("nvme", s, e))
					nvme++;
				else
					unknown++;
				z += strcspn(elmdn.elm_devnames + z, ",");
			}
			/* There should be one pass dev for each non-pass dev */
			ATF_CHECK_EQ(pass, da + ada + nvd + nvme);
			ATF_CHECK_EQ_MSG(0, unknown,
			    "Unknown device names %s", elmdn.elm_devnames);
		}
	}
	free(map);
	free(namebuf);

	return (true);
}

ATF_TC(getelmdevnames);
ATF_TC_HEAD(getelmdevnames, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETELMDEVNAMES's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getelmdevnames, tc)
{
	for_each_ses_dev(do_getelmdevnames, O_RDONLY);
}

static int
elm_type_name2int(const char *name) {
	const char *elm_type_names[] = ELM_TYPE_NAMES;
	int i;

	for (i = 0; i <= ELMTYP_LAST; i++) {
		/* sg_ses uses different case than ses(4) */
		if (0 == strcasecmp(name, elm_type_names[i]))
			return i;
	}
	return (-1);
}

static bool do_getelmmap(const char *devname, int fd) {
	encioc_element_t *map;
	FILE *pipe;
	char cmd[256];
	char line[256];
	unsigned elm_idx = 0;
	unsigned nobj, subenc_id;
	int r, elm_type;

	r = ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj);
	ATF_REQUIRE_EQ(r, 0);

	map = calloc(nobj, sizeof(encioc_element_t));
	ATF_REQUIRE(map != NULL);
	r = ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) map);
	ATF_REQUIRE_EQ(r, 0);

	snprintf(cmd, sizeof(cmd), "sg_ses -p1 %s", devname);
	pipe = popen(cmd, "r");
	ATF_REQUIRE(pipe != NULL);
	while(NULL != fgets(line, sizeof(line), pipe)) {
		char elm_type_name[80];
		int i, num_elm;

		r = sscanf(line,
		    "    Element type: %[a-zA-Z0-9_ /], subenclosure id: %d",
		    elm_type_name, &subenc_id);
		if (r == 2) {
			elm_type = elm_type_name2int(elm_type_name);
			continue;
		} else {
			r = sscanf(line,
			    "    Element type: vendor specific [0x%x], subenclosure id: %d",
			    &elm_type, &subenc_id);
			if (r == 2)
				continue;
		}
		r = sscanf(line, "      number of possible elements: %d",
		    &num_elm);
		if (r != 1)
			continue;

		/* Skip the Overall elements */
		elm_idx++;
		for (i = 0; i < num_elm; i++, elm_idx++) {
			ATF_CHECK_EQ(map[elm_idx].elm_idx, elm_idx);
			ATF_CHECK_EQ(map[elm_idx].elm_subenc_id, subenc_id);
			ATF_CHECK_EQ((int)map[elm_idx].elm_type, elm_type);
		}
	}

	free(map);
	r = pclose(pipe);
	if (r != 0) {
		/* Probably an SGPIO device */
		return (false);
	} else {
		ATF_CHECK_EQ_MSG(nobj, elm_idx,
				"Did not find the expected number of element "
				"descriptors in sg_ses's output");
		return (true);
	}
}

ATF_TC(getelmmap);
ATF_TC_HEAD(getelmmap, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETELMMAP's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getelmmap, tc)
{
	for_each_ses_dev(do_getelmmap, O_RDONLY);
}

static bool do_getelmstat(const char *devname, int fd) {
	encioc_element_t *map;
	unsigned elm_idx;
	unsigned nobj;
	int r, elm_subidx;
	elm_type_t last_elm_type = -1;

	r = ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj);
	ATF_REQUIRE_EQ(r, 0);

	map = calloc(nobj, sizeof(encioc_element_t));
	ATF_REQUIRE(map != NULL);
	r = ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) map);

	for (elm_idx = 0; elm_idx < nobj; elm_subidx++, elm_idx++) {
		encioc_elm_status_t e_status;
		FILE *pipe;
		char cmd[256];
		uint32_t status;
		int pr;

		if (last_elm_type != map[elm_idx].elm_type)
			elm_subidx = -1;
		last_elm_type = map[elm_idx].elm_type;

		snprintf(cmd, sizeof(cmd),
		    "sg_ses -Hp2 --index=_%d,%d --get=0:7:32 %s",
		    map[elm_idx].elm_type, elm_subidx, devname);
		pipe = popen(cmd, "r");
		ATF_REQUIRE(pipe != NULL);
		r = fscanf(pipe, "0x%x", &status);
		pr = pclose(pipe);
		if (pr != 0) {
			/* Probably an SGPIO device */
			free(map);
			return (false);
		}
		ATF_REQUIRE_EQ(r, 1);

		memset(&e_status, 0, sizeof(e_status));
		e_status.elm_idx = map[elm_idx].elm_idx;
		r = ioctl(fd, ENCIOC_GETELMSTAT, (caddr_t)&e_status);
		ATF_REQUIRE_EQ(r, 0);

		// Compare the common status field
		ATF_CHECK_EQ(e_status.cstat[0], status >> 24);
		/*
		 * Ignore the other fields, because some have values that can
		 * change frequently (voltage, temperature, etc)
		 */
	}
	free(map);

	return (true);
}

ATF_TC(getelmstat);
ATF_TC_HEAD(getelmstat, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETELMSTAT's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getelmstat, tc)
{
	for_each_ses_dev(do_getelmstat, O_RDONLY);
}

static bool do_getencid(const char *devname, int fd) {
	encioc_string_t stri;
	FILE *pipe;
	char cmd[256];
	char encid[32];
	char line[256];
	char sg_encid[32];
	int r, sg_ses_r;

	snprintf(cmd, sizeof(cmd), "sg_ses -p1 %s", devname);
	pipe = popen(cmd, "r");
	ATF_REQUIRE(pipe != NULL);
	sg_encid[0] = '\0';
	while(NULL != fgets(line, sizeof(line), pipe)) {
		const char *f = "      enclosure logical identifier (hex): %s";

		if (1 == fscanf(pipe, f, sg_encid))
			break;
	}
	sg_ses_r = pclose(pipe);

	stri.bufsiz = sizeof(encid);
	stri.buf = &encid[0];
	r = ioctl(fd, ENCIOC_GETENCID, (caddr_t) &stri);
	ATF_REQUIRE_EQ(r, 0);
	if (sg_ses_r == 0) {
		ATF_REQUIRE(sg_encid[0] != '\0');
		ATF_CHECK_STREQ(sg_encid, (char*)stri.buf);
		return (true);
	} else {
		/* Probably SGPIO; sg_ses unsupported */
		return (false);
	}
}

ATF_TC(getencid);
ATF_TC_HEAD(getencid, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETENCID's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getencid, tc)
{
	for_each_ses_dev(do_getencid, O_RDONLY);
}

static bool do_getencname(const char *devname, int fd) {
	encioc_string_t stri;
	FILE *pipe;
	char cmd[256];
	char encname[32];
	char line[256];
	int r;

	snprintf(cmd, sizeof(cmd), "sg_inq -o %s | awk '"
		"/Vendor identification/ {vi=$NF} "
		"/Product identification/ {pi=$NF} "
		"/Product revision level/ {prl=$NF} "
		"END {printf(vi \" \" pi \" \" prl)}'", devname);
	pipe = popen(cmd, "r");
	ATF_REQUIRE(pipe != NULL);
	ATF_REQUIRE(NULL != fgets(line, sizeof(line), pipe));
	pclose(pipe);

	stri.bufsiz = sizeof(encname);
	stri.buf = &encname[0];
	r = ioctl(fd, ENCIOC_GETENCNAME, (caddr_t) &stri);
	ATF_REQUIRE_EQ(r, 0);
	if (strlen(line) < 3) {
		// Probably an SGPIO device, INQUIRY unsupported
		return (false);
	} else {
		ATF_CHECK_STREQ(line, (char*)stri.buf);
		return (true);
	}
}

ATF_TC(getencname);
ATF_TC_HEAD(getencname, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETENCNAME's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_inq");
}
ATF_TC_BODY(getencname, tc)
{
	for_each_ses_dev(do_getencname, O_RDONLY);
}

static bool do_getencstat(const char *devname, int fd) {
	FILE *pipe;
	char cmd[256];
	unsigned char e, estat, invop, info, noncrit, crit, unrecov;
	int r;

	snprintf(cmd, sizeof(cmd), "sg_ses -p2 %s "
		"| grep 'INVOP='",
		devname);
	pipe = popen(cmd, "r");
	ATF_REQUIRE(pipe != NULL);
	r = fscanf(pipe,
	    "  INVOP=%hhu, INFO=%hhu, NON-CRIT=%hhu, CRIT=%hhu, UNRECOV=%hhu",
	    &invop, &info, &noncrit, &crit, &unrecov);
	pclose(pipe);
	if (r != 5) {
		/* Probably on SGPIO device */
		return (false);
	} else {
		r = ioctl(fd, ENCIOC_GETENCSTAT, (caddr_t) &estat);
		ATF_REQUIRE_EQ(r, 0);
		/* Exclude the info bit because it changes frequently */
		e = (invop << 4) | (noncrit << 2) | (crit << 1) | unrecov;
		ATF_CHECK_EQ(estat & ~0x08, e);
		return (true);
	}
}

ATF_TC(getencstat);
ATF_TC_HEAD(getencstat, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETENCSTAT's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getencstat, tc)
{
	for_each_ses_dev(do_getencstat, O_RDONLY);
}

static bool do_getnelm(const char *devname, int fd) {
	FILE *pipe;
	char cmd[256];
	char line[256];
	unsigned nobj, expected = 0;
	int r, sg_ses_r;

	snprintf(cmd, sizeof(cmd), "sg_ses -p1 %s", devname);
	pipe = popen(cmd, "r");
	ATF_REQUIRE(pipe != NULL);

	while(NULL != fgets(line, sizeof(line), pipe)) {
		unsigned nelm;

		if (1 == fscanf(pipe, "      number of possible elements: %u",
		    &nelm))
		{
			expected += 1 + nelm;	// +1 for the Overall element
		}
	}
	sg_ses_r = pclose(pipe);

	r = ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj);
	ATF_REQUIRE_EQ(r, 0);
	if (sg_ses_r == 0) {
		ATF_CHECK_EQ(expected, nobj);
		return (true);
	} else {
		/* Probably SGPIO, sg_ses unsupported */
		return (false);
	}
}

ATF_TC(getnelm);
ATF_TC_HEAD(getnelm, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETNELM's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getnelm, tc)
{
	for_each_ses_dev(do_getnelm, O_RDONLY);
}

static bool do_getstring(const char *devname, int fd) {
	FILE *pipe;
	char cmd[256];
	char *sg_ses_buf, *ses_buf;
	ssize_t sg_ses_count;
	encioc_string_t str_in;
	int r;

	sg_ses_buf = malloc(65535);
	ATF_REQUIRE(sg_ses_buf != NULL);
	ses_buf = malloc(65535);
	ATF_REQUIRE(ses_buf != NULL);

	snprintf(cmd, sizeof(cmd), "sg_ses -p4 -rr %s", devname);
	pipe = popen(cmd, "r");
	ATF_REQUIRE(pipe != NULL);
	sg_ses_count = fread(sg_ses_buf, 1, 65535, pipe);
	r = pclose(pipe);
	if (r != 0) {
		// This SES device does not support the STRINGIN diagnostic page
		return (false);
	}
	ATF_REQUIRE(sg_ses_count > 0);

	str_in.bufsiz = 65535;
	str_in.buf = ses_buf;
	r = ioctl(fd, ENCIOC_GETSTRING, (caddr_t) &str_in);
	ATF_REQUIRE_EQ(r, 0);
	ATF_CHECK_EQ(sg_ses_count, (ssize_t)str_in.bufsiz);
	ATF_CHECK_EQ(0, memcmp(sg_ses_buf, ses_buf, str_in.bufsiz));

	free(ses_buf);
	free(sg_ses_buf);

	return (true);
}

ATF_TC(getstring);
ATF_TC_HEAD(getstring, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Compare ENCIOC_GETSTRING's output to sg3_utils'");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.progs", "sg_ses");
}
ATF_TC_BODY(getstring, tc)
{
	atf_tc_expect_fail("Bug 258188 ENCIO_GETSTRING does not set the string's returned size");
	for_each_ses_dev(do_getstring, O_RDWR);
}

ATF_TP_ADD_TCS(tp)
{

	/*
	 * Untested ioctls:
	 *
	 * * ENCIOC_GETTEXT because it was never implemented
	 *
	 */
	ATF_TP_ADD_TC(tp, getelmdesc);
	ATF_TP_ADD_TC(tp, getelmdevnames);
	ATF_TP_ADD_TC(tp, getelmmap);
	ATF_TP_ADD_TC(tp, getelmstat);
	ATF_TP_ADD_TC(tp, getencid);
	ATF_TP_ADD_TC(tp, getencname);
	ATF_TP_ADD_TC(tp, getencstat);
	ATF_TP_ADD_TC(tp, getnelm);
	ATF_TP_ADD_TC(tp, getstring);

	return (atf_no_error());
}
