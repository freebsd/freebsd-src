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

#include <sys/sndstat.h>
#include <sys/nv.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Example program showcasing how to use sndstat(4)'s nvlist interface, and how
 * to fetch all currently supported fields, with the appropriate error checks.
 *
 * For more detailed information on what each nvlist field represents, please
 * read sndstat(4)'s man page.
 */

int
main(int argc, char *argv[])
{
	nvlist_t *nvl;
	const nvlist_t * const *di;
	const nvlist_t * const *cdi;
	struct sndstioc_nv_arg arg;
	size_t nitems, nchans, i, j;
	int fd, pchan, rchan;

	if ((fd = open("/dev/sndstat", O_RDONLY)) < 0)
		err(1, "open(/dev/sndstat)");

	if (ioctl(fd, SNDSTIOC_REFRESH_DEVS, NULL) < 0)
		err(1, "ioctl(SNDSTIOC_REFRESH_DEVS)");

	arg.nbytes = 0;
	arg.buf = NULL;
	if (ioctl(fd, SNDSTIOC_GET_DEVS, &arg) < 0)
		err(1, "ioctl(SNDSTIOC_GET_DEVS#1)");

	if ((arg.buf = malloc(arg.nbytes)) == NULL)
		err(1, "malloc");

	if (ioctl(fd, SNDSTIOC_GET_DEVS, &arg) < 0)
		err(1, "ioctl(SNDSTIOC_GET_DEVS#2)");

	if ((nvl = nvlist_unpack(arg.buf, arg.nbytes, 0)) == NULL)
		err(1, "nvlist_unpack");

	if (nvlist_empty(nvl) || !nvlist_exists(nvl, SNDST_DSPS))
		errx(1, "no soundcards attached");

	di = nvlist_get_nvlist_array(nvl, SNDST_DSPS, &nitems);
	for (i = 0; i < nitems; i++) {
#define NV(type, item)	\
	nvlist_get_ ## type (di[i], SNDST_DSPS_ ## item)
		printf("nameunit=%s\n", NV(string, NAMEUNIT));
		printf("\tfrom_user=%d\n", NV(bool, FROM_USER));
		printf("\tdevnode=%s\n", NV(string, DEVNODE));
		printf("\tdesc=%s\n", NV(string, DESC));
		printf("\tprovider=%s\n", NV(string, PROVIDER));
		printf("\tpchan=%d\n", (int)NV(number, PCHAN));
		printf("\trchan=%d\n", (int)NV(number, RCHAN));
		pchan = NV(number, PCHAN);
		rchan = NV(number, RCHAN);
#undef NV

		if (pchan && !nvlist_exists(di[i], SNDST_DSPS_INFO_PLAY))
			errx(1, "playback channel list empty");
		if (rchan && !nvlist_exists(di[i], SNDST_DSPS_INFO_REC))
			errx(1, "recording channel list empty");

#define NV(type, mode, item)						\
	nvlist_get_ ## type (nvlist_get_nvlist(di[i],			\
	    SNDST_DSPS_INFO_ ## mode), SNDST_DSPS_INFO_ ## item)
		if (pchan) {
			printf("\tplay_min_rate=%d\n",
			    (int)NV(number, PLAY, MIN_RATE));
			printf("\tplay_max_rate=%d\n",
			    (int)NV(number, PLAY, MAX_RATE));
			printf("\tplay_formats=%#08x\n",
			    (int)NV(number, PLAY, FORMATS));
			printf("\tplay_min_chn=%d\n",
			    (int)NV(number, PLAY, MIN_CHN));
			printf("\tplay_max_chn=%d\n",
			    (int)NV(number, PLAY, MAX_CHN));
		}
		if (rchan) {
			printf("\trec_min_rate=%d\n",
			    (int)NV(number, REC, MIN_RATE));
			printf("\trec_max_rate=%d\n",
			    (int)NV(number, REC, MAX_RATE));
			printf("\trec_formats=%#08x\n",
			    (int)NV(number, REC, FORMATS));
			printf("\trec_min_chn=%d\n",
			    (int)NV(number, REC, MIN_CHN));
			printf("\trec_max_chn=%d\n",
			    (int)NV(number, REC, MAX_CHN));
		}
#undef NV

		if (!nvlist_exists(di[i], SNDST_DSPS_PROVIDER_INFO))
			continue;

#define NV(type, item)							\
	nvlist_get_ ## type (nvlist_get_nvlist(di[i],			\
	    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_ ## item)
		printf("\tunit=%d\n", (int)NV(number, UNIT));
		printf("\tstatus=%s\n", NV(string, STATUS));
		printf("\tbitperfect=%d\n", NV(bool, BITPERFECT));
		printf("\tpvchan=%d\n", (int)NV(number, PVCHAN));
		printf("\tpvchanrate=%d\n", (int)NV(number, PVCHANRATE));
		printf("\tpvchanformat=%#08x\n", (int)NV(number, PVCHANFORMAT));
		printf("\trvchan=%d\n", (int)NV(number, RVCHAN));
		printf("\trvchanrate=%d\n", (int)NV(number, RVCHANRATE));
		printf("\trvchanformat=%#08x\n", (int)NV(number, RVCHANFORMAT));
#undef NV

		if (!nvlist_exists(nvlist_get_nvlist(di[i],
		    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_CHAN_INFO))
			errx(1, "channel info list empty");

		cdi = nvlist_get_nvlist_array(
		    nvlist_get_nvlist(di[i], SNDST_DSPS_PROVIDER_INFO),
		    SNDST_DSPS_SOUND4_CHAN_INFO, &nchans);
		for (j = 0; j < nchans; j++) {
#define NV(type, item)	\
	nvlist_get_ ## type (cdi[j], SNDST_DSPS_SOUND4_CHAN_ ## item)
			printf("\tchan=%s\n", NV(string, NAME));
			printf("\t\tparentchan=%s\n", NV(string, PARENTCHAN));
			printf("\t\tunit=%d\n", (int)NV(number, UNIT));
			printf("\t\tcaps=%#08x\n", (int)NV(number, CAPS));
			printf("\t\tlatency=%d\n", (int)NV(number, LATENCY));
			printf("\t\trate=%d\n", (int)NV(number, RATE));
			printf("\t\tformat=%#08x\n", (int)NV(number, FORMAT));
			printf("\t\tpid=%d\n", (int)NV(number, PID));
			printf("\t\tcomm=%s\n", NV(string, COMM));
			printf("\t\tintr=%d\n", (int)NV(number, INTR));
			printf("\t\txruns=%d\n", (int)NV(number, XRUNS));
			printf("\t\tfeedcnt=%d\n", (int)NV(number, FEEDCNT));
			printf("\t\tleftvol=%d\n", (int)NV(number, LEFTVOL));
			printf("\t\trightvol=%d\n", (int)NV(number, RIGHTVOL));
			printf("\t\thwbuf_format=%#08x\n",
			    (int)NV(number, HWBUF_FORMAT));
			printf("\t\thwbuf_size=%d\n",
			    (int)NV(number, HWBUF_SIZE));
			printf("\t\thwbuf_blksz=%d\n",
			    (int)NV(number, HWBUF_BLKSZ));
			printf("\t\thwbuf_blkcnt=%d\n",
			    (int)NV(number, HWBUF_BLKCNT));
			printf("\t\thwbuf_free=%d\n",
			    (int)NV(number, HWBUF_FREE));
			printf("\t\thwbuf_ready=%d\n",
			    (int)NV(number, HWBUF_READY));
			printf("\t\tswbuf_format=%#08x\n",
			    (int)NV(number, SWBUF_FORMAT));
			printf("\t\tswbuf_size=%d\n",
			    (int)NV(number, SWBUF_SIZE));
			printf("\t\tswbuf_blksz=%d\n",
			    (int)NV(number, SWBUF_BLKSZ));
			printf("\t\tswbuf_blkcnt=%d\n",
			    (int)NV(number, SWBUF_BLKCNT));
			printf("\t\tswbuf_free=%d\n",
			    (int)NV(number, SWBUF_FREE));
			printf("\t\tswbuf_ready=%d\n",
			    (int)NV(number, SWBUF_READY));
			printf("\t\tswbuf_feederchain=%s\n",
			    NV(string, FEEDERCHAIN));
#undef NV
		}
	}

	free(arg.buf);
	nvlist_destroy(nvl);
	close(fd);

	return (0);
}
