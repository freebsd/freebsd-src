/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Christos Margiolis <christos@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/linker.h>
#include <sys/nv.h>
#include <sys/sndstat.h>
#include <sys/soundcard.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

static void
load_dummy(void)
{
	if (kldload("snd_dummy.ko") < 0 && errno != EEXIST)
		atf_tc_skip("snd_dummy.ko not found");
}

ATF_TC(sndstat_nv);
ATF_TC_HEAD(sndstat_nv, tc)
{
	atf_tc_set_md_var(tc, "descr", "/dev/sndstat nvlist test");
}

ATF_TC_BODY(sndstat_nv, tc)
{
	nvlist_t *nvl;
	const nvlist_t * const *di;
	const nvlist_t * const *cdi;
	struct sndstioc_nv_arg arg;
	size_t nitems, nchans, i, j;
	int fd, rc, pchan, rchan;

	load_dummy();

	if ((fd = open("/dev/sndstat", O_RDONLY)) < 0)
		atf_tc_skip("/dev/sndstat not found, load sound(4)");

	rc = ioctl(fd, SNDSTIOC_REFRESH_DEVS, NULL);
	ATF_REQUIRE_EQ(rc, 0);

	arg.nbytes = 0;
	arg.buf = NULL;
	rc = ioctl(fd, SNDSTIOC_GET_DEVS, &arg);
	ATF_REQUIRE_EQ_MSG(rc, 0, "ioctl(SNDSTIOC_GET_DEVS#1) failed");

	arg.buf = malloc(arg.nbytes);
	ATF_REQUIRE(arg.buf != NULL);

	rc = ioctl(fd, SNDSTIOC_GET_DEVS, &arg);
	ATF_REQUIRE_EQ_MSG(rc, 0, "ioctl(SNDSTIOC_GET_DEVS#2) failed");

	nvl = nvlist_unpack(arg.buf, arg.nbytes, 0);
	ATF_REQUIRE(nvl != NULL);

	if (nvlist_empty(nvl) || !nvlist_exists(nvl, SNDST_DSPS))
		atf_tc_skip("no soundcards attached");

	di = nvlist_get_nvlist_array(nvl, SNDST_DSPS, &nitems);
	for (i = 0; i < nitems; i++) {
#define NV(type, item)	do {						\
	ATF_REQUIRE_MSG(nvlist_exists(di[i], SNDST_DSPS_ ## item),	\
	    "SNDST_DSPS_" #item " does not exist");			\
	nvlist_get_ ## type (di[i], SNDST_DSPS_ ## item);		\
} while (0)
		NV(string, NAMEUNIT);
		NV(bool, FROM_USER);
		NV(string, DEVNODE);
		NV(string, DESC);
		NV(string, PROVIDER);
		NV(number, PCHAN);
		NV(number, RCHAN);
#undef NV

		/* Cannot asign using the macro. */
		pchan = nvlist_get_number(di[i], SNDST_DSPS_PCHAN);
		rchan = nvlist_get_number(di[i], SNDST_DSPS_RCHAN);

		if (pchan && !nvlist_exists(di[i], SNDST_DSPS_INFO_PLAY))
			atf_tc_fail("playback channel list empty");
		if (rchan && !nvlist_exists(di[i], SNDST_DSPS_INFO_REC))
			atf_tc_fail("recording channel list empty");

#define NV(type, mode, item)	do {					\
	ATF_REQUIRE_MSG(nvlist_exists(nvlist_get_nvlist(di[i],		\
	    SNDST_DSPS_INFO_ ## mode), SNDST_DSPS_INFO_ ## item),	\
	    "SNDST_DSPS_INFO_" #item " does not exist");		\
	nvlist_get_ ## type (nvlist_get_nvlist(di[i],			\
	    SNDST_DSPS_INFO_ ## mode), SNDST_DSPS_INFO_ ## item);	\
} while (0)
		if (pchan) {
			NV(number, PLAY, MIN_RATE);
			NV(number, PLAY, MAX_RATE);
			NV(number, PLAY, FORMATS);
			NV(number, PLAY, MIN_CHN);
			NV(number, PLAY, MAX_CHN);
		}
		if (rchan) {
			NV(number, REC, MIN_RATE);
			NV(number, REC, MAX_RATE);
			NV(number, REC, FORMATS);
			NV(number, REC, MIN_CHN);
			NV(number, REC, MAX_CHN);
		}
#undef NV

		if (!nvlist_exists(di[i], SNDST_DSPS_PROVIDER_INFO))
			continue;

#define NV(type, item)	do {						\
	ATF_REQUIRE_MSG(nvlist_exists(nvlist_get_nvlist(di[i],		\
	    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_ ## item),	\
	    "SNDST_DSPS_SOUND4_" #item " does not exist");		\
	nvlist_get_ ## type (nvlist_get_nvlist(di[i],			\
	    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_ ## item);	\
} while (0)
		NV(number, UNIT);
		NV(string, STATUS);
		NV(bool, BITPERFECT);
		NV(number, PVCHAN);
		NV(number, PVCHANRATE);
		NV(number, PVCHANFORMAT);
		NV(number, RVCHAN);
		NV(number, PVCHANRATE);
		NV(number, PVCHANFORMAT);
#undef NV

		if (!nvlist_exists(nvlist_get_nvlist(di[i],
		    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_CHAN_INFO))
			atf_tc_fail("channel info list empty");

		cdi = nvlist_get_nvlist_array(
		    nvlist_get_nvlist(di[i], SNDST_DSPS_PROVIDER_INFO),
		    SNDST_DSPS_SOUND4_CHAN_INFO, &nchans);
		for (j = 0; j < nchans; j++) {
#define NV(type, item)	do {							\
	ATF_REQUIRE_MSG(nvlist_exists(cdi[j], SNDST_DSPS_SOUND4_CHAN_ ## item),	\
	    "SNDST_DSPS_SOUND4_CHAN_" #item " does not exist");			\
	nvlist_get_ ## type (cdi[j], SNDST_DSPS_SOUND4_CHAN_ ## item);		\
} while (0)
			NV(string, NAME);
			NV(string, PARENTCHAN);
			NV(number, UNIT);
			NV(number, CAPS);
			NV(number, LATENCY);
			NV(number, RATE);
			NV(number, FORMAT);
			NV(number, PID);
			NV(string, COMM);
			NV(number, INTR);
			NV(number, XRUNS);
			NV(number, FEEDCNT);
			NV(number, LEFTVOL);
			NV(number, RIGHTVOL);
			NV(number, HWBUF_FORMAT);
			NV(number, HWBUF_SIZE);
			NV(number, HWBUF_BLKSZ);
			NV(number, HWBUF_BLKCNT);
			NV(number, HWBUF_FREE);
			NV(number, HWBUF_READY);
			NV(number, SWBUF_FORMAT);
			NV(number, SWBUF_SIZE);
			NV(number, SWBUF_BLKSZ);
			NV(number, SWBUF_BLKCNT);
			NV(number, SWBUF_FREE);
			NV(number, SWBUF_READY);
			NV(string, FEEDERCHAIN);
#undef NV
		}
	}

	free(arg.buf);
	nvlist_destroy(nvl);
	close(fd);
}

#define UDEV_PROVIDER	"sndstat_udev"
#define UDEV_NAMEUNIT	"sndstat_udev"
#define UDEV_DEVNODE	"sndstat_udev"
#define UDEV_DESC	"Test Device"
#define UDEV_PCHAN	1
#define UDEV_RCHAN	1
#define UDEV_MIN_RATE	8000
#define UDEV_MAX_RATE	96000
#define UDEV_FORMATS	(AFMT_S16_NE | AFMT_S24_NE | AFMT_S32_NE)
#define UDEV_MIN_CHN	1
#define UDEV_MAX_CHN	2

ATF_TC(sndstat_udev);
ATF_TC_HEAD(sndstat_udev, tc)
{
	atf_tc_set_md_var(tc, "descr", "/dev/sndstat userdev interface test");
}

ATF_TC_BODY(sndstat_udev, tc)
{
	nvlist_t *nvl, *di, *dichild;
	const nvlist_t * const *rdi;
	struct sndstioc_nv_arg arg;
	const char *str;
	size_t nitems, i;
	int fd, rc, pchan, rchan, n;

	load_dummy();

	if ((fd = open("/dev/sndstat", O_RDWR)) < 0)
		atf_tc_skip("/dev/sndstat not found, load sound(4)");

	nvl = nvlist_create(0);
	ATF_REQUIRE(nvl != NULL);

	di = nvlist_create(0);
	ATF_REQUIRE(di != NULL);

	dichild = nvlist_create(0);
	ATF_REQUIRE(dichild != NULL);

	nvlist_add_string(di, SNDST_DSPS_PROVIDER, UDEV_PROVIDER);
	nvlist_add_string(di, SNDST_DSPS_NAMEUNIT, UDEV_NAMEUNIT);
	nvlist_add_string(di, SNDST_DSPS_DESC, UDEV_DESC);
	nvlist_add_string(di, SNDST_DSPS_DEVNODE, UDEV_DEVNODE);
	nvlist_add_number(di, SNDST_DSPS_PCHAN, UDEV_PCHAN);
	nvlist_add_number(di, SNDST_DSPS_RCHAN, UDEV_RCHAN);

	nvlist_add_number(dichild, SNDST_DSPS_INFO_MIN_RATE, UDEV_MIN_RATE);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_MAX_RATE, UDEV_MAX_RATE);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_FORMATS, UDEV_FORMATS);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_MIN_CHN, UDEV_MIN_CHN);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_MAX_CHN, UDEV_MAX_CHN);

	nvlist_add_nvlist(di, SNDST_DSPS_INFO_PLAY, dichild);
	nvlist_add_nvlist(di, SNDST_DSPS_INFO_REC, dichild);

	nvlist_append_nvlist_array(nvl, SNDST_DSPS, di);
	ATF_REQUIRE_EQ(nvlist_error(nvl), 0);

	arg.buf = nvlist_pack(nvl, &arg.nbytes);
	ATF_REQUIRE_MSG(arg.buf != NULL, "failed to pack nvlist");

	rc = ioctl(fd, SNDSTIOC_ADD_USER_DEVS, &arg);
	free(arg.buf);
	ATF_REQUIRE_EQ_MSG(rc, 0, "ioctl(SNDSTIOC_ADD_USER_DEVS) failed");

	nvlist_destroy(di);
	nvlist_destroy(dichild);
	nvlist_destroy(nvl);

	/* Read back registered values. */
	rc = ioctl(fd, SNDSTIOC_REFRESH_DEVS, NULL);
	ATF_REQUIRE_EQ(rc, 0);

	arg.nbytes = 0;
	arg.buf = NULL;
	rc = ioctl(fd, SNDSTIOC_GET_DEVS, &arg);
	ATF_REQUIRE_EQ_MSG(rc, 0, "ioctl(SNDSTIOC_GET_DEVS#1) failed");

	arg.buf = malloc(arg.nbytes);
	ATF_REQUIRE(arg.buf != NULL);

	rc = ioctl(fd, SNDSTIOC_GET_DEVS, &arg);
	ATF_REQUIRE_EQ_MSG(rc, 0, "ioctl(SNDSTIOC_GET_DEVS#2) failed");

	nvl = nvlist_unpack(arg.buf, arg.nbytes, 0);
	ATF_REQUIRE(nvl != NULL);

	if (nvlist_empty(nvl) || !nvlist_exists(nvl, SNDST_DSPS))
		atf_tc_skip("no soundcards attached");

	rdi = nvlist_get_nvlist_array(nvl, SNDST_DSPS, &nitems);
	for (i = 0; i < nitems; i++) {
#define NV(type, item, var)	do {					\
	ATF_REQUIRE_MSG(nvlist_exists(rdi[i], SNDST_DSPS_ ## item),	\
	    "SNDST_DSPS_" #item " does not exist");			\
	var = nvlist_get_ ## type (rdi[i], SNDST_DSPS_ ## item);	\
} while (0)
		/* Search for our device. */
		NV(string, NAMEUNIT, str);
		if (strcmp(str, UDEV_NAMEUNIT) == 0)
			break;
	}
	if (i == nitems)
		atf_tc_fail("userland device %s not found", UDEV_NAMEUNIT);

	NV(string, NAMEUNIT, str);
	ATF_CHECK(strcmp(str, UDEV_NAMEUNIT) == 0);

	NV(bool, FROM_USER, n);
	ATF_CHECK(n);

	NV(string, DEVNODE, str);
	ATF_CHECK(strcmp(str, UDEV_DEVNODE) == 0);

	NV(string, DESC, str);
	ATF_CHECK(strcmp(str, UDEV_DESC) == 0);

	NV(string, PROVIDER, str);
	ATF_CHECK(strcmp(str, UDEV_PROVIDER) == 0);

	NV(number, PCHAN, pchan);
	ATF_CHECK(pchan == UDEV_PCHAN);
	if (pchan && !nvlist_exists(rdi[i], SNDST_DSPS_INFO_PLAY))
		atf_tc_fail("playback channel list empty");

	NV(number, RCHAN, rchan);
	ATF_CHECK(rchan == UDEV_RCHAN);
	if (rchan && !nvlist_exists(rdi[i], SNDST_DSPS_INFO_REC))
		atf_tc_fail("recording channel list empty");
#undef NV

#define NV(type, mode, item, var)	do {				\
	ATF_REQUIRE_MSG(nvlist_exists(nvlist_get_nvlist(rdi[i],		\
	    SNDST_DSPS_INFO_ ## mode), SNDST_DSPS_INFO_ ## item),	\
	    "SNDST_DSPS_INFO_" #item " does not exist");		\
	var = nvlist_get_ ## type (nvlist_get_nvlist(rdi[i],		\
	    SNDST_DSPS_INFO_ ## mode), SNDST_DSPS_INFO_ ## item);	\
} while (0)
	if (pchan) {
		NV(number, PLAY, MIN_RATE, n);
		ATF_CHECK(n == UDEV_MIN_RATE);

		NV(number, PLAY, MAX_RATE, n);
		ATF_CHECK(n == UDEV_MAX_RATE);

		NV(number, PLAY, FORMATS, n);
		ATF_CHECK(n == UDEV_FORMATS);

		NV(number, PLAY, MIN_CHN, n);
		ATF_CHECK(n == UDEV_MIN_CHN);

		NV(number, PLAY, MAX_CHN, n);
		ATF_CHECK(n == UDEV_MAX_CHN);
	}
	if (rchan) {
		NV(number, REC, MIN_RATE, n);
		ATF_CHECK(n == UDEV_MIN_RATE);

		NV(number, REC, MAX_RATE, n);
		ATF_CHECK(n == UDEV_MAX_RATE);

		NV(number, REC, FORMATS, n);
		ATF_CHECK(n == UDEV_FORMATS);

		NV(number, REC, MIN_CHN, n);
		ATF_CHECK(n == UDEV_MIN_CHN);

		NV(number, REC, MAX_CHN, n);
		ATF_CHECK(n == UDEV_MAX_CHN);
	}
#undef NV

	free(arg.buf);
	nvlist_destroy(nvl);
	close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sndstat_nv);
	ATF_TP_ADD_TC(tp, sndstat_udev);

	return (atf_no_error());
}
