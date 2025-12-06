/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2025 The FreeBSD Foundation
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

#include <sys/nv.h>
#include <sys/queue.h>
#include <sys/sndstat.h>
#include <sys/soundcard.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <mixer.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Taken from sys/dev/sound/pcm/ */
#define STATUS_LEN	64
#define FMTSTR_LEN	16

struct snd_chan {
	char name[NAME_MAX];
	char parentchan[NAME_MAX];
	int unit;
#define INPUT	0
#define OUTPUT	1
	int direction;
	char caps[BUFSIZ];
	int latency;
	int rate;
	char format[FMTSTR_LEN];
	int pid;
	char proc[NAME_MAX];
	int interrupts;
	int xruns;
	int feedcount;
	int volume;
	struct {
		char format[FMTSTR_LEN];
		int rate;
		int size_bytes;
		int size_frames;
		int blksz;
		int blkcnt;
		int free;
		int ready;
	} hwbuf, swbuf;
	char feederchain[BUFSIZ];
	struct snd_dev *dev;
	TAILQ_ENTRY(snd_chan) next;
};

struct snd_dev {
	char name[NAME_MAX];
	char desc[NAME_MAX];
	char status[BUFSIZ];
	char devnode[NAME_MAX];
	int from_user;
	int unit;
	char caps[BUFSIZ];
	int bitperfect;
	int realtime;
	int autoconv;
	struct {
		char format[FMTSTR_LEN];
		int rate;
		int pchans;
		int vchans;
		int min_rate;
		int max_rate;
		int min_chans;
		int max_chans;
		char formats[BUFSIZ];
	} play, rec;
	TAILQ_HEAD(, snd_chan) chans;
};

struct snd_ctl {
	const char *name;
	size_t off;
#define STR	0
#define NUM	1
#define VOL	2
#define GRP	3
	int type;
	int (*mod)(struct snd_dev *, void *);
};

struct map {
	int val;
	const char *str;
};

static int mod_bitperfect(struct snd_dev *, void *);
static int mod_autoconv(struct snd_dev *, void *);
static int mod_realtime(struct snd_dev *, void *);
static int mod_play_vchans(struct snd_dev *, void *);
static int mod_play_rate(struct snd_dev *, void *);
static int mod_play_format(struct snd_dev *, void *);
static int mod_rec_vchans(struct snd_dev *, void *);
static int mod_rec_rate(struct snd_dev *, void *);
static int mod_rec_format(struct snd_dev *, void *);

static struct snd_ctl dev_ctls[] = {
#define F(member)	offsetof(struct snd_dev, member)
	{ "name",		F(name),		STR,	NULL },
	{ "desc",		F(desc),		STR,	NULL },
	{ "status",		F(status),		STR,	NULL },
	{ "devnode",		F(devnode),		STR,	NULL },
	{ "from_user",		F(from_user),		NUM,	NULL },
	{ "unit",		F(unit),		NUM,	NULL },
	{ "caps",		F(caps),		STR,	NULL },
	{ "bitperfect",		F(bitperfect),		NUM,	mod_bitperfect },
	{ "autoconv",		F(autoconv),		NUM,	mod_autoconv },
	{ "realtime",		F(realtime),		NUM,	mod_realtime },
	{ "play",		F(play),		GRP,	NULL },
	{ "play.format",	F(play.format),		STR,	mod_play_format },
	{ "play.rate",		F(play.rate),		NUM,	mod_play_rate },
	/*{ "play.pchans",	F(play.pchans),		NUM,	NULL },*/
	{ "play.vchans",	F(play.vchans),		NUM,	mod_play_vchans },
	{ "play.min_rate",	F(play.min_rate),	NUM,	NULL },
	{ "play.max_rate",	F(play.max_rate),	NUM,	NULL },
	{ "play.min_chans",	F(play.min_chans),	NUM,	NULL },
	{ "play.max_chans",	F(play.max_chans),	NUM,	NULL },
	{ "play.formats",	F(play.formats),	STR,	NULL },
	{ "rec",		F(rec),			GRP,	NULL },
	{ "rec.rate",		F(rec.rate),		NUM,	mod_rec_rate },
	{ "rec.format",		F(rec.format),		STR,	mod_rec_format },
	/*{ "rec.pchans",		F(rec.pchans),		NUM,	NULL },*/
	{ "rec.vchans",		F(rec.vchans),		NUM,	mod_rec_vchans },
	{ "rec.min_rate",	F(rec.min_rate),	NUM,	NULL },
	{ "rec.max_rate",	F(rec.max_rate),	NUM,	NULL },
	{ "rec.min_chans",	F(rec.min_chans),	NUM,	NULL },
	{ "rec.max_chans",	F(rec.max_chans),	NUM,	NULL },
	{ "rec.formats",	F(rec.formats),		STR,	NULL },
	{ NULL,			0,			0,	NULL }
#undef F
};

static struct snd_ctl chan_ctls[] = {
#define F(member)	offsetof(struct snd_chan, member)
	/*{ "name",		F(name),		STR,	NULL },*/
	{ "parentchan",		F(parentchan),		STR,	NULL },
	{ "unit",		F(unit),		NUM,	NULL },
	{ "caps",		F(caps),		STR,	NULL },
	{ "latency",		F(latency),		NUM,	NULL },
	{ "rate",		F(rate),		NUM,	NULL },
	{ "format",		F(format),		STR,	NULL },
	{ "pid",		F(pid),			NUM,	NULL },
	{ "proc",		F(proc),		STR,	NULL },
	{ "interrupts",		F(interrupts),		NUM,	NULL },
	{ "xruns",		F(xruns),		NUM,	NULL },
	{ "feedcount",		F(feedcount),		NUM,	NULL },
	{ "volume",		F(volume),		VOL,	NULL },
	{ "hwbuf",		F(hwbuf),		GRP,	NULL },
	{ "hwbuf.format",	F(hwbuf.format),	STR,	NULL },
	{ "hwbuf.rate",		F(hwbuf.rate),		NUM,	NULL },
	{ "hwbuf.size_bytes",	F(hwbuf.size_bytes),	NUM,	NULL },
	{ "hwbuf.size_frames",	F(hwbuf.size_frames),	NUM,	NULL },
	{ "hwbuf.blksz",	F(hwbuf.blksz),		NUM,	NULL },
	{ "hwbuf.blkcnt",	F(hwbuf.blkcnt),	NUM,	NULL },
	{ "hwbuf.free",		F(hwbuf.free),		NUM,	NULL },
	{ "hwbuf.ready",	F(hwbuf.ready),		NUM,	NULL },
	{ "swbuf",		F(swbuf),		GRP,	NULL },
	{ "swbuf.format",	F(swbuf.format),	STR,	NULL },
	{ "swbuf.rate",		F(swbuf.rate),		NUM,	NULL },
	{ "swbuf.size_bytes",	F(swbuf.size_bytes),	NUM,	NULL },
	{ "swbuf.size_frames",	F(swbuf.size_frames),	NUM,	NULL },
	{ "swbuf.blksz",	F(swbuf.blksz),		NUM,	NULL },
	{ "swbuf.blkcnt",	F(swbuf.blkcnt),	NUM,	NULL },
	{ "swbuf.free",		F(swbuf.free),		NUM,	NULL },
	{ "swbuf.ready",	F(swbuf.ready),		NUM,	NULL },
	{ "feederchain",	F(feederchain),		STR,	NULL },
	{ NULL,			0,			0,	NULL }
#undef F
};

/*
 * Taken from the OSSv4 manual. Not all of them are supported on FreeBSD
 * however, and some of them are obsolete.
 */
static struct map capmap[] = {
	{ PCM_CAP_ANALOGIN,	"ANALOGIN" },
	{ PCM_CAP_ANALOGOUT,	"ANALOGOUT" },
	{ PCM_CAP_BATCH,	"BATCH" },
	{ PCM_CAP_BIND,		"BIND" },
	{ PCM_CAP_COPROC,	"COPROC" },
	{ PCM_CAP_DEFAULT,	"DEFAULT" },
	{ PCM_CAP_DIGITALIN,	"DIGITALIN" },
	{ PCM_CAP_DIGITALOUT,	"DIGITALOUT" },
	{ PCM_CAP_DUPLEX,	"DUPLEX" },
	{ PCM_CAP_FREERATE,	"FREERATE" },
	{ PCM_CAP_HIDDEN,	"HIDDEN" },
	{ PCM_CAP_INPUT,	"INPUT" },
	{ PCM_CAP_MMAP,		"MMAP" },
	{ PCM_CAP_MODEM,	"MODEM" },
	{ PCM_CAP_MULTI,	"MULTI" },
	{ PCM_CAP_OUTPUT,	"OUTPUT" },
	{ PCM_CAP_REALTIME,	"REALTIME" },
	{ PCM_CAP_REVISION,	"REVISION" },
	{ PCM_CAP_SHADOW,	"SHADOW" },
	{ PCM_CAP_SPECIAL,	"SPECIAL" },
	{ PCM_CAP_TRIGGER,	"TRIGGER" },
	{ PCM_CAP_VIRTUAL,	"VIRTUAL" },
	{ 0,			NULL }
};

static struct map fmtmap[] = {
	{ AFMT_A_LAW,		"alaw" },
	{ AFMT_MU_LAW,		"mulaw" },
	{ AFMT_S8,		"s8" },
	{ AFMT_U8,		"u8" },
	{ AFMT_AC3,		"ac3" },
	{ AFMT_S16_LE,		"s16le" },
	{ AFMT_S16_BE,		"s16be" },
	{ AFMT_U16_LE,		"u16le" },
	{ AFMT_U16_BE,		"u16be" },
	{ AFMT_S24_LE,		"s24le" },
	{ AFMT_S24_BE,		"s24be" },
	{ AFMT_U24_LE,		"u24le" },
	{ AFMT_U24_BE,		"u24be" },
	{ AFMT_S32_LE,		"s32le" },
	{ AFMT_S32_BE,		"s32be" },
	{ AFMT_U32_LE,		"u32le" },
	{ AFMT_U32_BE,		"u32be" },
	{ AFMT_F32_LE,		"f32le" },
	{ AFMT_F32_BE,		"f32be" },
	{ 0,			NULL }
};

static bool oflag = false;
static bool vflag = false;

static void
cap2str(char *buf, size_t size, int caps)
{
	struct map *p;

	for (p = capmap; p->str != NULL; p++) {
		if ((p->val & caps) == 0)
			continue;
		strlcat(buf, p->str, size);
		strlcat(buf, ",", size);
	}
	if (*buf == '\0')
		strlcpy(buf, "UNKNOWN", size);
	else
		buf[strlen(buf) - 1] = '\0';
}

static void
fmt2str(char *buf, size_t size, int fmt)
{
	struct map *p;
	int enc, ch, ext;

	enc = fmt & 0xf00fffff;
	ch = (fmt & 0x07f00000) >> 20;
	ext = (fmt & 0x08000000) >> 27;

	for (p = fmtmap; p->str != NULL; p++) {
		if ((p->val & enc) == 0)
			continue;
		strlcat(buf, p->str, size);
		if (ch) {
			snprintf(buf + strlen(buf), size,
			    ":%d.%d", ch - ext, ext);
		}
		strlcat(buf, ",", size);
	}
	if (*buf == '\0')
		strlcpy(buf, "UNKNOWN", size);
	else
		buf[strlen(buf) - 1] = '\0';
}

static int
bytes2frames(int bytes, int fmt)
{
	int enc, ch, samplesz;

	enc = fmt & 0xf00fffff;
	ch = (fmt & 0x07f00000) >> 20;
	/* Add the channel extension if present (e.g 2.1). */
	ch += (fmt & 0x08000000) >> 27;

	if (enc & (AFMT_S8 | AFMT_U8 | AFMT_MU_LAW | AFMT_A_LAW))
		samplesz = 1;
	else if (enc & (AFMT_S16_NE | AFMT_U16_NE))
		samplesz = 2;
	else if (enc & (AFMT_S24_NE | AFMT_U24_NE))
		samplesz = 3;
	else if (enc & (AFMT_S32_NE | AFMT_U32_NE | AFMT_F32_NE))
		samplesz = 4;
	else
		samplesz = 0;

	if (!samplesz || !ch)
		return (-1);

	return (bytes / (samplesz * ch));
}

static int
sysctl_int(const char *buf, const char *arg, int *var)
{
	size_t size;
	int n, prev;

	size = sizeof(int);
	/* Read current value. */
	if (sysctlbyname(buf, &prev, &size, NULL, 0) < 0) {
		warn("sysctlbyname(%s)", buf);
		return (-1);
	}

	/* Read-only. */
	if (arg != NULL) {
		errno = 0;
		n = strtol(arg, NULL, 10);
		if (errno == EINVAL || errno == ERANGE) {
			warn("strtol(%s)", arg);
			return (-1);
		}

		/* Apply new value. */
		if (sysctlbyname(buf, NULL, 0, &n, size) < 0) {
			warn("sysctlbyname(%s, %d)", buf, n);
			return (-1);
		}
	}

	/* Read back applied value for good measure. */
	if (sysctlbyname(buf, &n, &size, NULL, 0) < 0) {
		warn("sysctlbyname(%s)", buf);
		return (-1);
	}

	if (arg != NULL)
		printf("%s: %d -> %d\n", buf, prev, n);
	if (var != NULL)
		*var = n;

	return (0);
}

static int
sysctl_str(const char *buf, const char *arg, char *var, size_t varsz)
{
	size_t size;
	char prev[BUFSIZ];
	char *tmp;

	/* Read current value. */
	size = sizeof(prev);
	if (sysctlbyname(buf, prev, &size, NULL, 0) < 0) {
		warn("sysctlbyname(%s)", buf);
		return (-1);
	}

	/* Read-only. */
	if (arg != NULL) {
		size = strlen(arg);
		/* Apply new value. */
		if (sysctlbyname(buf, NULL, 0, arg, size) < 0) {
			warn("sysctlbyname(%s, %s)", buf, arg);
			return (-1);
		}
		/* Get size of new string. */
		if (sysctlbyname(buf, NULL, &size, NULL, 0) < 0) {
			warn("sysctlbyname(%s)", buf);
			return (-1);
		}
	}

	if ((tmp = calloc(1, size)) == NULL)
		err(1, "calloc");
	/* Read back applied value for good measure. */
	if (sysctlbyname(buf, tmp, &size, NULL, 0) < 0) {
		warn("sysctlbyname(%s)", buf);
		free(tmp);
		return (-1);
	}

	if (arg != NULL)
		printf("%s: %s -> %s\n", buf, prev, tmp);
	if (var != NULL)
		strlcpy(var, tmp, varsz);
	free(tmp);

	return (0);
}

static struct snd_dev *
read_dev(char *path)
{
	nvlist_t *nvl;
	const nvlist_t * const *di;
	const nvlist_t * const *cdi;
	struct sndstioc_nv_arg arg;
	struct snd_dev *dp = NULL;
	struct snd_chan *ch;
	size_t nitems, nchans, i, j;
	int fd, caps, unit, t1, t2, t3;

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

	if (path == NULL || (path != NULL && strcmp(basename(path), "dsp") == 0))
		unit = mixer_get_dunit();
	else
		unit = -1;

	/* Find whether the requested device exists */
	di = nvlist_get_nvlist_array(nvl, SNDST_DSPS, &nitems);
	for (i = 0; i < nitems; i++) {
		if (unit == -1 && strcmp(basename(path),
		    nvlist_get_string(di[i], SNDST_DSPS_DEVNODE)) == 0)
			break;
		else if (nvlist_exists(di[i], SNDST_DSPS_PROVIDER_INFO) &&
		    (int)nvlist_get_number(nvlist_get_nvlist(di[i],
		    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_UNIT) == unit)
			break;;
	}
	if (i == nitems)
		errx(1, "device not found");

#define NV(type, item)	\
	nvlist_get_ ## type (di[i], SNDST_DSPS_ ## item)
	if ((dp = calloc(1, sizeof(struct snd_dev))) == NULL)
		err(1, "calloc");

	dp->unit = -1;
	strlcpy(dp->name, NV(string, NAMEUNIT), sizeof(dp->name));
	strlcpy(dp->desc, NV(string, DESC), sizeof(dp->desc));
	strlcpy(dp->devnode, NV(string, DEVNODE), sizeof(dp->devnode));
	dp->from_user = NV(bool, FROM_USER);
	dp->play.pchans = NV(number, PCHAN);
	dp->rec.pchans = NV(number, RCHAN);
#undef NV

	if (dp->play.pchans && !nvlist_exists(di[i], SNDST_DSPS_INFO_PLAY))
		errx(1, "%s: playback channel list empty", dp->name);
	if (dp->rec.pchans && !nvlist_exists(di[i], SNDST_DSPS_INFO_REC))
		errx(1, "%s: recording channel list empty", dp->name);

#define NV(type, mode, item)						\
	nvlist_get_ ## type (nvlist_get_nvlist(di[i],			\
	    SNDST_DSPS_INFO_ ## mode), SNDST_DSPS_INFO_ ## item)
	if (dp->play.pchans) {
		dp->play.min_rate = NV(number, PLAY, MIN_RATE);
		dp->play.max_rate = NV(number, PLAY, MAX_RATE);
		dp->play.min_chans = NV(number, PLAY, MIN_CHN);
		dp->play.max_chans = NV(number, PLAY, MAX_CHN);
		fmt2str(dp->play.formats, sizeof(dp->play.formats),
		    NV(number, PLAY, FORMATS));
	}
	if (dp->rec.pchans) {
		dp->rec.min_rate = NV(number, REC, MIN_RATE);
		dp->rec.max_rate = NV(number, REC, MAX_RATE);
		dp->rec.min_chans = NV(number, REC, MIN_CHN);
		dp->rec.max_chans = NV(number, REC, MAX_CHN);
		fmt2str(dp->rec.formats, sizeof(dp->rec.formats),
		    NV(number, REC, FORMATS));
	}
#undef NV

	/*
	 * Skip further parsing if the provider is not sound(4), as the
	 * following code is sound(4)-specific.
	 */
	if (strcmp(nvlist_get_string(di[i], SNDST_DSPS_PROVIDER),
	    SNDST_DSPS_SOUND4_PROVIDER) != 0)
		goto done;

	if (!nvlist_exists(di[i], SNDST_DSPS_PROVIDER_INFO))
		errx(1, "%s: provider_info list empty", dp->name);

#define NV(type, item)							\
	nvlist_get_ ## type (nvlist_get_nvlist(di[i],			\
	    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_ ## item)
	strlcpy(dp->status, NV(string, STATUS), sizeof(dp->status));
	dp->unit = NV(number, UNIT);
	dp->bitperfect = NV(bool, BITPERFECT);
	dp->play.vchans = NV(bool, PVCHAN);
	dp->play.rate = NV(number, PVCHANRATE);
	fmt2str(dp->play.format, sizeof(dp->play.format),
	    NV(number, PVCHANFORMAT));
	dp->rec.vchans = NV(bool, RVCHAN);
	dp->rec.rate = NV(number, RVCHANRATE);
	fmt2str(dp->rec.format, sizeof(dp->rec.format),
	    NV(number, RVCHANFORMAT));
#undef NV

	dp->autoconv = (dp->play.vchans || dp->rec.vchans) && !dp->bitperfect;

	if (sysctl_int("hw.snd.latency", NULL, &t1) ||
	    sysctl_int("hw.snd.latency_profile", NULL, &t2) ||
	    sysctl_int("kern.timecounter.alloweddeviation", NULL, &t3))
		err(1, "%s: sysctl", dp->name);
	if (t1 == 0 && t2 == 0 && t3 == 0)
		dp->realtime = 1;

	if (!nvlist_exists(nvlist_get_nvlist(di[i],
	    SNDST_DSPS_PROVIDER_INFO), SNDST_DSPS_SOUND4_CHAN_INFO))
		errx(1, "%s: channel info list empty", dp->name);

	cdi = nvlist_get_nvlist_array(
	    nvlist_get_nvlist(di[i], SNDST_DSPS_PROVIDER_INFO),
	    SNDST_DSPS_SOUND4_CHAN_INFO, &nchans);

	TAILQ_INIT(&dp->chans);
	caps = 0;
	for (j = 0; j < nchans; j++) {
#define NV(type, item)	\
	nvlist_get_ ## type (cdi[j], SNDST_DSPS_SOUND4_CHAN_ ## item)
		if ((ch = calloc(1, sizeof(struct snd_chan))) == NULL)
			err(1, "calloc");

		strlcpy(ch->name, NV(string, NAME), sizeof(ch->name));
		strlcpy(ch->parentchan, NV(string, PARENTCHAN),
		    sizeof(ch->parentchan));
		ch->unit = NV(number, UNIT);
		ch->direction = (NV(number, CAPS) & PCM_CAP_INPUT) ?
		    INPUT : OUTPUT;
		cap2str(ch->caps, sizeof(ch->caps), NV(number, CAPS));
		ch->latency = NV(number, LATENCY);
		ch->rate = NV(number, RATE);
		fmt2str(ch->format, sizeof(ch->format), NV(number, FORMAT));
		ch->pid = NV(number, PID);
		strlcpy(ch->proc, NV(string, COMM), sizeof(ch->proc));
		ch->interrupts = NV(number, INTR);
		ch->xruns = NV(number, XRUNS);
		ch->feedcount = NV(number, FEEDCNT);
		ch->volume = NV(number, LEFTVOL) |
		    NV(number, RIGHTVOL) << 8;
		fmt2str(ch->hwbuf.format, sizeof(ch->hwbuf.format),
		    NV(number, HWBUF_FORMAT));
		ch->hwbuf.rate = NV(number, HWBUF_RATE);
		ch->hwbuf.size_bytes = NV(number, HWBUF_SIZE);
		ch->hwbuf.size_frames =
		    bytes2frames(ch->hwbuf.size_bytes, NV(number, HWBUF_FORMAT));
		ch->hwbuf.blksz = NV(number, HWBUF_BLKSZ);
		ch->hwbuf.blkcnt = NV(number, HWBUF_BLKCNT);
		ch->hwbuf.free = NV(number, HWBUF_FREE);
		ch->hwbuf.ready = NV(number, HWBUF_READY);
		fmt2str(ch->swbuf.format, sizeof(ch->swbuf.format),
		    NV(number, SWBUF_FORMAT));
		ch->swbuf.rate = NV(number, SWBUF_RATE);
		ch->swbuf.size_bytes = NV(number, SWBUF_SIZE);
		ch->swbuf.size_frames =
		    bytes2frames(ch->swbuf.size_bytes, NV(number, SWBUF_FORMAT));
		ch->swbuf.blksz = NV(number, SWBUF_BLKSZ);
		ch->swbuf.blkcnt = NV(number, SWBUF_BLKCNT);
		ch->swbuf.free = NV(number, SWBUF_FREE);
		ch->swbuf.ready = NV(number, SWBUF_READY);
		strlcpy(ch->feederchain, NV(string, FEEDERCHAIN),
		    sizeof(ch->feederchain));
		ch->dev = dp;

		caps |= NV(number, CAPS);
		TAILQ_INSERT_TAIL(&dp->chans, ch, next);

		if (!dp->rec.vchans && ch->direction == INPUT) {
			strlcpy(dp->rec.format, ch->hwbuf.format,
			    sizeof(dp->rec.format));
			dp->rec.rate = ch->hwbuf.rate;
		} else if (!dp->play.vchans && ch->direction == OUTPUT) {
			strlcpy(dp->play.format, ch->hwbuf.format,
			    sizeof(dp->play.format));
			dp->play.rate = ch->hwbuf.rate;
		}
#undef NV
	}
	cap2str(dp->caps, sizeof(dp->caps), caps);

done:
	free(arg.buf);
	nvlist_destroy(nvl);
	close(fd);

	return (dp);
}

static void
free_dev(struct snd_dev *dp)
{
	struct snd_chan *ch;

	while (!TAILQ_EMPTY(&dp->chans)) {
		ch = TAILQ_FIRST(&dp->chans);
		TAILQ_REMOVE(&dp->chans, ch, next);
		free(ch);
	}
	free(dp);
}

static void
print_dev_ctl(struct snd_dev *dp, struct snd_ctl *ctl, bool simple,
    bool showgrp)
{
	struct snd_ctl *cp;
	size_t len;

	if (ctl->type != GRP) {
		if (simple)
			printf("%s=", ctl->name);
		else
			printf("    %-20s= ", ctl->name);
	}

	switch (ctl->type) {
	case STR:
		printf("%s\n", (char *)dp + ctl->off);
		break;
	case NUM:
		printf("%d\n", *(int *)((intptr_t)dp + ctl->off));
		break;
	case VOL:
		break;
	case GRP:
		if (!simple || !showgrp)
			break;
		for (cp = dev_ctls; cp->name != NULL; cp++) {
			len = strlen(ctl->name);
			if (strncmp(ctl->name, cp->name, len) == 0 &&
			    cp->name[len] == '.' && cp->type != GRP)
				print_dev_ctl(dp, cp, simple, showgrp);
		}
		break;
	}
}

static void
print_chan_ctl(struct snd_chan *ch, struct snd_ctl *ctl, bool simple,
    bool showgrp)
{
	struct snd_ctl *cp;
	size_t len;
	int v;

	if (ctl->type != GRP) {
		if (simple)
			printf("%s.%s=", ch->name, ctl->name);
		else
			printf("        %-20s= ", ctl->name);
	}

	switch (ctl->type) {
	case STR:
		printf("%s\n", (char *)ch + ctl->off);
		break;
	case NUM:
		printf("%d\n", *(int *)((intptr_t)ch + ctl->off));
		break;
	case VOL:
		v = *(int *)((intptr_t)ch + ctl->off);
		printf("%.2f:%.2f\n",
		    MIX_VOLNORM(v & 0x00ff), MIX_VOLNORM((v >> 8) & 0x00ff));
		break;
	case GRP:
		if (!simple || !showgrp)
			break;
		for (cp = chan_ctls; cp->name != NULL; cp++) {
			len = strlen(ctl->name);
			if (strncmp(ctl->name, cp->name, len) == 0 &&
			    cp->name[len] == '.' && cp->type != GRP)
				print_chan_ctl(ch, cp, simple, showgrp);
		}
		break;
	}
}

static void
print_dev(struct snd_dev *dp)
{
	struct snd_chan *ch;
	struct snd_ctl *ctl;

	if (!oflag) {
		printf("%s: <%s> %s", dp->name, dp->desc, dp->status);

		printf(" (");
		if (dp->play.pchans)
			printf("play");
		if (dp->play.pchans && dp->rec.pchans)
			printf("/");
		if (dp->rec.pchans)
			printf("rec");
		printf(")\n");
	}

	for (ctl = dev_ctls; ctl->name != NULL; ctl++)
		print_dev_ctl(dp, ctl, oflag, false);

	if (vflag) {
		TAILQ_FOREACH(ch, &dp->chans, next) {
			if (!oflag)
				printf("    %s\n", ch->name);
			for (ctl = chan_ctls; ctl->name != NULL; ctl++)
				print_chan_ctl(ch, ctl, oflag, false);
		}
	}
}

static int
mod_bitperfect(struct snd_dev *dp, void *arg)
{
	char buf[64];

	if (dp->from_user)
		return (-1);

	snprintf(buf, sizeof(buf), "dev.pcm.%d.bitperfect", dp->unit);

	return (sysctl_int(buf, arg, &dp->bitperfect));
}

static int
mod_autoconv(struct snd_dev *dp, void *arg)
{
	const char *val = arg;
	const char *zero = "0";
	const char *one = "1";
	int rc = -1;

	if (dp->from_user)
		return (rc);

	if (strcmp(val, zero) == 0) {
		rc = mod_play_vchans(dp, __DECONST(char *, zero)) ||
		    mod_rec_vchans(dp, __DECONST(char *, zero)) ||
		    mod_bitperfect(dp, __DECONST(char *, one));
		if (rc == 0)
			dp->autoconv = 0;
	} else if (strcmp(val, one) == 0) {
		rc = mod_play_vchans(dp, __DECONST(char *, one)) ||
		    mod_rec_vchans(dp, __DECONST(char *, one)) ||
		    mod_bitperfect(dp, __DECONST(char *, zero));
		if (rc == 0)
			dp->autoconv = 1;
	}

	return (rc);
}

static int
mod_realtime(struct snd_dev *dp, void *arg)
{
	const char *val = arg;
	int rc = -1;

	if (dp->from_user)
		return (-1);

	if (strcmp(val, "0") == 0) {
		/* TODO */
		rc = sysctl_int("hw.snd.latency", "2", NULL) ||
		    sysctl_int("hw.snd.latency_profile", "1", NULL) ||
		    sysctl_int("kern.timecounter.alloweddeviation", "5", NULL);
		if (rc == 0)
			dp->realtime = 0;
	} else if (strcmp(val, "1") == 0) {
		rc = sysctl_int("hw.snd.latency", "0", NULL) ||
		    sysctl_int("hw.snd.latency_profile", "0", NULL) ||
		    sysctl_int("kern.timecounter.alloweddeviation", "0", NULL);
		if (rc == 0)
			dp->realtime = 1;
	}

	return (rc);
}

static int
mod_play_vchans(struct snd_dev *dp, void *arg)
{
	char buf[64];

	if (dp->from_user)
		return (-1);
	if (!dp->play.pchans)
		return (0);

	snprintf(buf, sizeof(buf), "dev.pcm.%d.play.vchans", dp->unit);

	return (sysctl_int(buf, arg, &dp->play.vchans));
}

static int
mod_play_rate(struct snd_dev *dp, void *arg)
{
	char buf[64];

	if (dp->from_user)
		return (-1);
	if (!dp->play.vchans)
		return (0);

	snprintf(buf, sizeof(buf), "dev.pcm.%d.play.vchanrate", dp->unit);

	return (sysctl_int(buf, arg, &dp->play.rate));
}

static int
mod_play_format(struct snd_dev *dp, void *arg)
{
	char buf[64];

	if (dp->from_user)
		return (-1);
	if (!dp->play.vchans)
		return (0);

	snprintf(buf, sizeof(buf), "dev.pcm.%d.play.vchanformat", dp->unit);

	return (sysctl_str(buf, arg, dp->play.format, sizeof(dp->play.format)));
}

static int
mod_rec_vchans(struct snd_dev *dp, void *arg)
{
	char buf[64];

	if (dp->from_user)
		return (-1);
	if (!dp->rec.pchans)
		return (0);

	snprintf(buf, sizeof(buf), "dev.pcm.%d.rec.vchans", dp->unit);

	return (sysctl_int(buf, arg, &dp->rec.vchans));
}

static int
mod_rec_rate(struct snd_dev *dp, void *arg)
{
	char buf[64];

	if (dp->from_user)
		return (-1);
	if (!dp->rec.vchans)
		return (0);

	snprintf(buf, sizeof(buf), "dev.pcm.%d.rec.vchanrate", dp->unit);

	return (sysctl_int(buf, arg, &dp->rec.rate));
}

static int
mod_rec_format(struct snd_dev *dp, void *arg)
{
	char buf[64];

	if (dp->from_user)
		return (-1);
	if (!dp->rec.vchans)
		return (0);

	snprintf(buf, sizeof(buf), "dev.pcm.%d.rec.vchanformat", dp->unit);

	return (sysctl_str(buf, arg, dp->rec.format, sizeof(dp->rec.format)));
}

static void __dead2
usage(void)
{
	fprintf(stderr, "usage: %s [-f device] [-hov] [control[=value] ...]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct snd_dev *dp;
	struct snd_chan *ch;
	struct snd_ctl *ctl;
	char *path = NULL;
	char *s, *propstr;
	bool show = true, found;
	int c;

	while ((c = getopt(argc, argv, "f:hov")) != -1) {
		switch (c) {
		case 'f':
			path = optarg;
			break;
		case 'o':
			oflag = true;
			break;
		case 'v':
			vflag = true;
			break;
		case 'h':	/* FALLTHROUGH */
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	dp = read_dev(path);

	while (argc > 0) {
		if ((s = strdup(*argv)) == NULL)
			err(1, "strdup(%s)", *argv);

		propstr = strsep(&s, "=");
		if (propstr == NULL)
			goto next;

		found = false;
		for (ctl = dev_ctls; ctl->name != NULL; ctl++) {
			if (strcmp(ctl->name, propstr) != 0)
				continue;
			if (s == NULL) {
				print_dev_ctl(dp, ctl, true, true);
				show = false;
			} else if (ctl->mod != NULL && ctl->mod(dp, s) < 0)
				warnx("%s(%s) failed", ctl->name, s);
			found = true;
			break;
		}
		TAILQ_FOREACH(ch, &dp->chans, next) {
			for (ctl = chan_ctls; ctl->name != NULL; ctl++) {
				if (strcmp(ctl->name, propstr) != 0)
					continue;
				print_chan_ctl(ch, ctl, true, true);
				show = false;
				found = true;
				break;
			}
		}
		if (!found)
			warnx("%s: no such property", propstr);
next:
		free(s);
		argc--;
		argv++;
	}

	free_dev(dp);

	if (show) {
		/*
		 * Re-read dev to reflect new state in case we changed some
		 * property.
		 */
		dp = read_dev(path);
		print_dev(dp);
		free_dev(dp);
	}

	return (0);
}
