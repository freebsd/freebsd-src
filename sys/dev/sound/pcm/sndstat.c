/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * Copyright (c) 2001 Cameron Grant <cg@FreeBSD.org>
 * Copyright (c) 2020 The FreeBSD Foundation
 * All rights reserved.
 * Copyright (c) 2024-2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Ka Ho Ng
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

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/nv.h>
#include <sys/dnv.h>
#include <sys/sx.h>

#include <dev/sound/pcm/sound.h>

#include "feeder_if.h"

#define	SS_TYPE_PCM		1
#define	SS_TYPE_MIDI		2
#define	SS_TYPE_SEQUENCER	3

static d_open_t sndstat_open;
static void sndstat_close(void *);
static d_read_t sndstat_read;
static d_write_t sndstat_write;
static d_ioctl_t sndstat_ioctl;

static struct cdevsw sndstat_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	sndstat_open,
	.d_read =	sndstat_read,
	.d_write =	sndstat_write,
	.d_ioctl =	sndstat_ioctl,
	.d_name =	"sndstat",
	.d_flags =	D_TRACKCLOSE,
};

struct sndstat_entry {
	TAILQ_ENTRY(sndstat_entry) link;
	device_t dev;
	char *str;
	int type, unit;
};

struct sndstat_userdev {
	TAILQ_ENTRY(sndstat_userdev) link;
	char *provider;
	char *nameunit;
	char *devnode;
	char *desc;
	unsigned int pchan;
	unsigned int rchan;
	struct {
		uint32_t min_rate;
		uint32_t max_rate;
		uint32_t formats;
		uint32_t min_chn;
		uint32_t max_chn;
	} info_play, info_rec;
	nvlist_t *provider_nvl;
};

struct sndstat_file {
	TAILQ_ENTRY(sndstat_file) entry;
	struct sbuf sbuf;
	struct sx lock;
	void *devs_nvlbuf;	/* (l) */
	size_t devs_nbytes;	/* (l) */
	TAILQ_HEAD(, sndstat_userdev) userdev_list;	/* (l) */
	int out_offset;
  	int in_offset;
	int fflags;
};

static struct sx sndstat_lock;
static struct cdev *sndstat_dev;

#define	SNDSTAT_LOCK() sx_xlock(&sndstat_lock)
#define	SNDSTAT_UNLOCK() sx_xunlock(&sndstat_lock)

static TAILQ_HEAD(, sndstat_entry) sndstat_devlist = TAILQ_HEAD_INITIALIZER(sndstat_devlist);
static TAILQ_HEAD(, sndstat_file) sndstat_filelist = TAILQ_HEAD_INITIALIZER(sndstat_filelist);

int snd_verbose = 0;

static int sndstat_prepare(struct sndstat_file *);
static struct sndstat_userdev *
sndstat_line2userdev(struct sndstat_file *, const char *, int);

static int
sysctl_hw_sndverbose(SYSCTL_HANDLER_ARGS)
{
	int error, verbose;

	verbose = snd_verbose;
	error = sysctl_handle_int(oidp, &verbose, 0, req);
	if (error == 0 && req->newptr != NULL) {
		if (verbose < 0 || verbose > 4)
			error = EINVAL;
		else
			snd_verbose = verbose;
	}
	return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, verbose,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, 0, sizeof(int),
    sysctl_hw_sndverbose, "I",
    "verbosity level");

static int
sndstat_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct sndstat_file *pf;

	pf = malloc(sizeof(*pf), M_DEVBUF, M_WAITOK | M_ZERO);

	sbuf_new(&pf->sbuf, NULL, 4096, SBUF_AUTOEXTEND);

	pf->fflags = flags;
	TAILQ_INIT(&pf->userdev_list);
	sx_init(&pf->lock, "sndstat_file");

	SNDSTAT_LOCK();
	TAILQ_INSERT_TAIL(&sndstat_filelist, pf, entry);
	SNDSTAT_UNLOCK();

	devfs_set_cdevpriv(pf, &sndstat_close);

	return (0);
}

/*
 * Should only be called either when:
 * * Closing
 * * pf->lock held
 */
static void
sndstat_remove_all_userdevs(struct sndstat_file *pf)
{
	struct sndstat_userdev *ud;

	KASSERT(
	    sx_xlocked(&pf->lock), ("%s: Called without pf->lock", __func__));
	while ((ud = TAILQ_FIRST(&pf->userdev_list)) != NULL) {
		TAILQ_REMOVE(&pf->userdev_list, ud, link);
		free(ud->provider, M_DEVBUF);
		free(ud->desc, M_DEVBUF);
		free(ud->devnode, M_DEVBUF);
		free(ud->nameunit, M_DEVBUF);
		nvlist_destroy(ud->provider_nvl);
		free(ud, M_DEVBUF);
	}
}

static void
sndstat_close(void *sndstat_file)
{
	struct sndstat_file *pf = (struct sndstat_file *)sndstat_file;

	SNDSTAT_LOCK();
	sbuf_delete(&pf->sbuf);
	TAILQ_REMOVE(&sndstat_filelist, pf, entry);
	SNDSTAT_UNLOCK();

	free(pf->devs_nvlbuf, M_NVLIST);
	sx_xlock(&pf->lock);
	sndstat_remove_all_userdevs(pf);
	sx_xunlock(&pf->lock);
	sx_destroy(&pf->lock);

	free(pf, M_DEVBUF);
}

static int
sndstat_read(struct cdev *i_dev, struct uio *buf, int flag)
{
	struct sndstat_file *pf;
	int err;
	int len;

	err = devfs_get_cdevpriv((void **)&pf);
	if (err != 0)
		return (err);

	/* skip zero-length reads */
	if (buf->uio_resid == 0)
		return (0);

	SNDSTAT_LOCK();
	if (pf->out_offset != 0) {
		/* don't allow both reading and writing */
		err = EINVAL;
		goto done;
	} else if (pf->in_offset == 0) {
		err = sndstat_prepare(pf);
		if (err <= 0) {
			err = ENOMEM;
			goto done;
		}
	}
	len = sbuf_len(&pf->sbuf) - pf->in_offset;
	if (len > buf->uio_resid)
		len = buf->uio_resid;
	if (len > 0)
		err = uiomove(sbuf_data(&pf->sbuf) + pf->in_offset, len, buf);
	pf->in_offset += len;
done:
	SNDSTAT_UNLOCK();
	return (err);
}

static int
sndstat_write(struct cdev *i_dev, struct uio *buf, int flag)
{
	struct sndstat_file *pf;
	uint8_t temp[64];
	int err;
	int len;

	err = devfs_get_cdevpriv((void **)&pf);
	if (err != 0)
		return (err);

	/* skip zero-length writes */
	if (buf->uio_resid == 0)
		return (0);

	/* don't allow writing more than 64Kbytes */
	if (buf->uio_resid > 65536)
		return (ENOMEM);

	SNDSTAT_LOCK();
	if (pf->in_offset != 0) {
		/* don't allow both reading and writing */
		err = EINVAL;
	} else {
		/* only remember the last write - allows for updates */
		sx_xlock(&pf->lock);
		sndstat_remove_all_userdevs(pf);
		sx_xunlock(&pf->lock);

		while (1) {
			len = sizeof(temp);
			if (len > buf->uio_resid)
				len = buf->uio_resid;
			if (len > 0) {
				err = uiomove(temp, len, buf);
				if (err)
					break;
			} else {
				break;
			}
			if (sbuf_bcat(&pf->sbuf, temp, len) < 0) {
				err = ENOMEM;
				break;
			}
		}
		sbuf_finish(&pf->sbuf);

		if (err == 0) {
			char *line, *str;

			str = sbuf_data(&pf->sbuf);
			while ((line = strsep(&str, "\n")) != NULL) {
				struct sndstat_userdev *ud;

				ud = sndstat_line2userdev(pf, line, strlen(line));
				if (ud == NULL)
					continue;

				sx_xlock(&pf->lock);
				TAILQ_INSERT_TAIL(&pf->userdev_list, ud, link);
				sx_xunlock(&pf->lock);
			}

			pf->out_offset = sbuf_len(&pf->sbuf);
		} else
			pf->out_offset = 0;

		sbuf_clear(&pf->sbuf);
	}
	SNDSTAT_UNLOCK();
	return (err);
}

static void
sndstat_get_caps(struct snddev_info *d, int dir, uint32_t *min_rate,
    uint32_t *max_rate, uint32_t *fmts, uint32_t *minchn, uint32_t *maxchn)
{
	struct pcm_channel *c;
	struct pcmchan_caps *caps;
	int i;

	*fmts = 0;
	*min_rate = UINT32_MAX;
	*max_rate = 0;
	*minchn = UINT32_MAX;
	*maxchn = 0;

	CHN_FOREACH(c, d, channels.pcm) {
		if (c->direction != dir)
			continue;
		CHN_LOCK(c);
		caps = chn_getcaps(c);
		for (i = 0; caps->fmtlist[i]; i++) {
			*fmts |= AFMT_ENCODING(caps->fmtlist[i]);
			*minchn = min(AFMT_CHANNEL(caps->fmtlist[i]), *minchn);
			*maxchn = max(AFMT_CHANNEL(caps->fmtlist[i]), *maxchn);
		}
		if ((c->flags & CHN_F_EXCLUSIVE) ||
		    (pcm_getflags(d->dev) & SD_F_BITPERFECT)) {
			*min_rate = min(*min_rate, caps->minspeed);
			*max_rate = max(*max_rate, caps->maxspeed);
		} else {
			*min_rate = min(*min_rate, feeder_rate_min);
			*max_rate = max(*max_rate, feeder_rate_max);
		}
		CHN_UNLOCK(c);
	}
	if (*min_rate == UINT32_MAX)
		*min_rate = 0;
	if (*minchn == UINT32_MAX)
		*minchn = 0;
}

static nvlist_t *
sndstat_create_diinfo_nv(uint32_t min_rate, uint32_t max_rate, uint32_t formats,
	    uint32_t min_chn, uint32_t max_chn)
{
	nvlist_t *nv;

	nv = nvlist_create(0);
	if (nv == NULL)
		return (NULL);
	nvlist_add_number(nv, SNDST_DSPS_INFO_MIN_RATE, min_rate);
	nvlist_add_number(nv, SNDST_DSPS_INFO_MAX_RATE, max_rate);
	nvlist_add_number(nv, SNDST_DSPS_INFO_FORMATS, formats);
	nvlist_add_number(nv, SNDST_DSPS_INFO_MIN_CHN, min_chn);
	nvlist_add_number(nv, SNDST_DSPS_INFO_MAX_CHN, max_chn);
	return (nv);
}

static int
sndstat_build_sound4_nvlist(struct snddev_info *d, nvlist_t **dip)
{
	struct pcm_channel *c;
	struct pcm_feeder *f;
	struct sbuf sb;
	uint32_t maxrate, minrate, fmts, minchn, maxchn, caps;
	nvlist_t *di = NULL, *sound4di = NULL, *diinfo = NULL, *cdi = NULL;
	int err, nchan;
	char buf[AFMTSTR_LEN];

	di = nvlist_create(0);
	if (di == NULL) {
		err = ENOMEM;
		goto done;
	}
	sound4di = nvlist_create(0);
	if (sound4di == NULL) {
		err = ENOMEM;
		goto done;
	}

	nvlist_add_bool(di, SNDST_DSPS_FROM_USER, false);
	nvlist_add_stringf(di, SNDST_DSPS_NAMEUNIT, "%s",
			device_get_nameunit(d->dev));
	nvlist_add_stringf(di, SNDST_DSPS_DEVNODE, "dsp%d",
			device_get_unit(d->dev));
	nvlist_add_string(
			di, SNDST_DSPS_DESC, device_get_desc(d->dev));

	PCM_ACQUIRE_QUICK(d);
	nvlist_add_number(di, SNDST_DSPS_PCHAN, d->playcount);
	nvlist_add_number(di, SNDST_DSPS_RCHAN, d->reccount);
	if (d->playcount > 0) {
		sndstat_get_caps(d, PCMDIR_PLAY, &minrate, &maxrate, &fmts,
		    &minchn, &maxchn);
		nvlist_add_number(di, "pminrate", minrate);
		nvlist_add_number(di, "pmaxrate", maxrate);
		nvlist_add_number(di, "pfmts", fmts);
		diinfo = sndstat_create_diinfo_nv(minrate, maxrate, fmts,
		    minchn, maxchn);
		if (diinfo == NULL)
			nvlist_set_error(di, ENOMEM);
		else
			nvlist_move_nvlist(di, SNDST_DSPS_INFO_PLAY, diinfo);
	}
	if (d->reccount > 0) {
		sndstat_get_caps(d, PCMDIR_REC, &minrate, &maxrate, &fmts,
		    &minchn, &maxchn);
		nvlist_add_number(di, "rminrate", minrate);
		nvlist_add_number(di, "rmaxrate", maxrate);
		nvlist_add_number(di, "rfmts", fmts);
		diinfo = sndstat_create_diinfo_nv(minrate, maxrate, fmts,
		    minchn, maxchn);
		if (diinfo == NULL)
			nvlist_set_error(di, ENOMEM);
		else
			nvlist_move_nvlist(di, SNDST_DSPS_INFO_REC, diinfo);
	}

	nvlist_add_number(sound4di, SNDST_DSPS_SOUND4_UNIT,
			device_get_unit(d->dev)); // XXX: I want signed integer here
	nvlist_add_string(sound4di, SNDST_DSPS_SOUND4_STATUS, d->status);
	nvlist_add_bool(
	    sound4di, SNDST_DSPS_SOUND4_BITPERFECT, d->flags & SD_F_BITPERFECT);
	nvlist_add_bool(sound4di, SNDST_DSPS_SOUND4_PVCHAN,
	    d->flags & SD_F_PVCHANS);
	nvlist_add_number(sound4di, SNDST_DSPS_SOUND4_PVCHANRATE,
	    d->pvchanrate);
	nvlist_add_number(sound4di, SNDST_DSPS_SOUND4_PVCHANFORMAT,
	    d->pvchanformat);
	nvlist_add_bool(sound4di, SNDST_DSPS_SOUND4_RVCHAN,
	    d->flags & SD_F_RVCHANS);
	nvlist_add_number(sound4di, SNDST_DSPS_SOUND4_RVCHANRATE,
	    d->rvchanrate);
	nvlist_add_number(sound4di, SNDST_DSPS_SOUND4_RVCHANFORMAT,
	    d->rvchanformat);

	nchan = 0;
	CHN_FOREACH(c, d, channels.pcm) {
		sbuf_new(&sb, NULL, 4096, SBUF_AUTOEXTEND);
		cdi = nvlist_create(0);
		if (cdi == NULL) {
			sbuf_delete(&sb);
			PCM_RELEASE_QUICK(d);
			err = ENOMEM;
			goto done;
		}

		CHN_LOCK(c);

		caps = PCM_CAP_REALTIME | PCM_CAP_MMAP | PCM_CAP_TRIGGER |
		    ((c->flags & CHN_F_VIRTUAL) ? PCM_CAP_VIRTUAL : 0) |
		    ((c->direction == PCMDIR_PLAY) ? PCM_CAP_OUTPUT : PCM_CAP_INPUT);

		nvlist_add_string(cdi, SNDST_DSPS_SOUND4_CHAN_NAME, c->name);
		nvlist_add_string(cdi, SNDST_DSPS_SOUND4_CHAN_PARENTCHAN,
		    c->parentchannel != NULL ? c->parentchannel->name : "");
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_UNIT, nchan++);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_CAPS, caps);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_LATENCY,
		    c->latency);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_RATE, c->speed);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_FORMAT,
		    c->format);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_PID, c->pid);
		nvlist_add_string(cdi, SNDST_DSPS_SOUND4_CHAN_COMM, c->comm);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_INTR,
		    c->interrupts);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_FEEDCNT,
		    c->feedcount);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_XRUNS, c->xruns);
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_LEFTVOL,
		    CHN_GETVOLUME(c, SND_VOL_C_PCM, SND_CHN_T_FL));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_RIGHTVOL,
		    CHN_GETVOLUME(c, SND_VOL_C_PCM, SND_CHN_T_FR));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_HWBUF_FORMAT,
		    sndbuf_getfmt(c->bufhard));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_HWBUF_RATE,
		    sndbuf_getspd(c->bufhard));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_HWBUF_SIZE,
		    sndbuf_getsize(c->bufhard));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_HWBUF_BLKSZ,
		    sndbuf_getblksz(c->bufhard));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_HWBUF_BLKCNT,
		    sndbuf_getblkcnt(c->bufhard));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_HWBUF_FREE,
		    sndbuf_getfree(c->bufhard));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_HWBUF_READY,
		    sndbuf_getready(c->bufhard));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_SWBUF_FORMAT,
		    sndbuf_getfmt(c->bufsoft));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_SWBUF_RATE,
		    sndbuf_getspd(c->bufsoft));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_SWBUF_SIZE,
		    sndbuf_getsize(c->bufsoft));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_SWBUF_BLKSZ,
		    sndbuf_getblksz(c->bufsoft));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_SWBUF_BLKCNT,
		    sndbuf_getblkcnt(c->bufsoft));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_SWBUF_FREE,
		    sndbuf_getfree(c->bufsoft));
		nvlist_add_number(cdi, SNDST_DSPS_SOUND4_CHAN_SWBUF_READY,
		    sndbuf_getready(c->bufsoft));

		if (c->parentchannel != NULL) {
			sbuf_printf(&sb, "[%s", (c->direction == PCMDIR_REC) ?
			    c->parentchannel->name : "userland");
		} else {
			sbuf_printf(&sb, "[%s", (c->direction == PCMDIR_REC) ?
			    "hardware" : "userland");
		}
		sbuf_printf(&sb, " -> ");
		f = c->feeder;
		while (f->source != NULL)
			f = f->source;
		while (f != NULL) {
			sbuf_printf(&sb, "%s", f->class->name);
			if (f->desc->type == FEEDER_FORMAT) {
				snd_afmt2str(f->desc->in, buf, sizeof(buf));
				sbuf_printf(&sb, "(%s -> ", buf);
				snd_afmt2str(f->desc->out, buf, sizeof(buf));
				sbuf_printf(&sb, "%s)", buf);
			} else if (f->desc->type == FEEDER_MATRIX) {
				sbuf_printf(&sb, "(%d.%dch -> %d.%dch)",
				    AFMT_CHANNEL(f->desc->in) -
				    AFMT_EXTCHANNEL(f->desc->in),
				    AFMT_EXTCHANNEL(f->desc->in),
				    AFMT_CHANNEL(f->desc->out) -
				    AFMT_EXTCHANNEL(f->desc->out),
				    AFMT_EXTCHANNEL(f->desc->out));
			} else if (f->desc->type == FEEDER_RATE) {
				sbuf_printf(&sb, "(%d -> %d)",
				    FEEDER_GET(f, FEEDRATE_SRC),
				    FEEDER_GET(f, FEEDRATE_DST));
			} else {
				snd_afmt2str(f->desc->out, buf, sizeof(buf));
				sbuf_printf(&sb, "(%s)", buf);
			}
			sbuf_printf(&sb, " -> ");
			f = f->parent;
		}
		if (c->parentchannel != NULL) {
			sbuf_printf(&sb, "%s]", (c->direction == PCMDIR_REC) ?
			    "userland" : c->parentchannel->name);
		} else {
			sbuf_printf(&sb, "%s]", (c->direction == PCMDIR_REC) ?
			    "userland" : "hardware");
		}

		CHN_UNLOCK(c);

		sbuf_finish(&sb);
		nvlist_add_string(cdi, SNDST_DSPS_SOUND4_CHAN_FEEDERCHAIN,
		    sbuf_data(&sb));
		sbuf_delete(&sb);

		nvlist_append_nvlist_array(sound4di,
		    SNDST_DSPS_SOUND4_CHAN_INFO, cdi);
		nvlist_destroy(cdi);
		err = nvlist_error(sound4di);
		if (err) {
			PCM_RELEASE_QUICK(d);
			goto done;
		}
	}
	nvlist_move_nvlist(di, SNDST_DSPS_PROVIDER_INFO, sound4di);
	sound4di = NULL;

	PCM_RELEASE_QUICK(d);
	nvlist_add_string(di, SNDST_DSPS_PROVIDER, SNDST_DSPS_SOUND4_PROVIDER);

	err = nvlist_error(di);
	if (err)
		goto done;

	*dip = di;

done:
	if (err) {
		nvlist_destroy(sound4di);
		nvlist_destroy(di);
	}
	return (err);
}

static int
sndstat_build_userland_nvlist(struct sndstat_userdev *ud, nvlist_t **dip)
{
	nvlist_t *di, *diinfo;
	int err;

	di = nvlist_create(0);
	if (di == NULL) {
		err = ENOMEM;
		goto done;
	}

	nvlist_add_bool(di, SNDST_DSPS_FROM_USER, true);
	nvlist_add_number(di, SNDST_DSPS_PCHAN, ud->pchan);
	nvlist_add_number(di, SNDST_DSPS_RCHAN, ud->rchan);
	nvlist_add_string(di, SNDST_DSPS_NAMEUNIT, ud->nameunit);
	nvlist_add_string(
			di, SNDST_DSPS_DEVNODE, ud->devnode);
	nvlist_add_string(di, SNDST_DSPS_DESC, ud->desc);
	if (ud->pchan != 0) {
		nvlist_add_number(di, "pminrate",
		    ud->info_play.min_rate);
		nvlist_add_number(di, "pmaxrate",
		    ud->info_play.max_rate);
		nvlist_add_number(di, "pfmts",
		    ud->info_play.formats);
		diinfo = sndstat_create_diinfo_nv(ud->info_play.min_rate,
		    ud->info_play.max_rate, ud->info_play.formats,
		    ud->info_play.min_chn, ud->info_play.max_chn);
		if (diinfo == NULL)
			nvlist_set_error(di, ENOMEM);
		else
			nvlist_move_nvlist(di, SNDST_DSPS_INFO_PLAY, diinfo);
	}
	if (ud->rchan != 0) {
		nvlist_add_number(di, "rminrate",
		    ud->info_rec.min_rate);
		nvlist_add_number(di, "rmaxrate",
		    ud->info_rec.max_rate);
		nvlist_add_number(di, "rfmts",
		    ud->info_rec.formats);
		diinfo = sndstat_create_diinfo_nv(ud->info_rec.min_rate,
		    ud->info_rec.max_rate, ud->info_rec.formats,
		    ud->info_rec.min_chn, ud->info_rec.max_chn);
		if (diinfo == NULL)
			nvlist_set_error(di, ENOMEM);
		else
			nvlist_move_nvlist(di, SNDST_DSPS_INFO_REC, diinfo);
	}
	nvlist_add_string(di, SNDST_DSPS_PROVIDER,
	    (ud->provider != NULL) ? ud->provider : "");
	if (ud->provider_nvl != NULL)
		nvlist_add_nvlist(
		    di, SNDST_DSPS_PROVIDER_INFO, ud->provider_nvl);

	err = nvlist_error(di);
	if (err)
		goto done;

	*dip = di;

done:
	if (err)
		nvlist_destroy(di);
	return (err);
}

/*
 * Should only be called with the following locks held:
 * * sndstat_lock
 */
static int
sndstat_create_devs_nvlist(nvlist_t **nvlp)
{
	int err;
	nvlist_t *nvl;
	struct sndstat_entry *ent;
	struct sndstat_file *pf;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (ENOMEM);

	TAILQ_FOREACH(ent, &sndstat_devlist, link) {
		struct snddev_info *d;
		nvlist_t *di;

		d = device_get_softc(ent->dev);
		if (!PCM_REGISTERED(d))
			continue;

		err = sndstat_build_sound4_nvlist(d, &di);
		if (err)
			goto done;

		nvlist_append_nvlist_array(nvl, SNDST_DSPS, di);
		nvlist_destroy(di);
		err = nvlist_error(nvl);
		if (err)
			goto done;
	}

	TAILQ_FOREACH(pf, &sndstat_filelist, entry) {
		struct sndstat_userdev *ud;

		sx_xlock(&pf->lock);

		TAILQ_FOREACH(ud, &pf->userdev_list, link) {
			nvlist_t *di;

			err = sndstat_build_userland_nvlist(ud, &di);
			if (err != 0) {
				sx_xunlock(&pf->lock);
				goto done;
			}
			nvlist_append_nvlist_array(nvl, SNDST_DSPS, di);
			nvlist_destroy(di);

			err = nvlist_error(nvl);
			if (err != 0) {
				sx_xunlock(&pf->lock);
				goto done;
			}
		}

		sx_xunlock(&pf->lock);
	}

	*nvlp = nvl;

done:
	if (err != 0)
		nvlist_destroy(nvl);
	return (err);
}

static int
sndstat_refresh_devs(struct sndstat_file *pf)
{
	sx_xlock(&pf->lock);
	free(pf->devs_nvlbuf, M_NVLIST);
	pf->devs_nvlbuf = NULL;
	pf->devs_nbytes = 0;
	sx_unlock(&pf->lock);

	return (0);
}

static int
sndstat_get_devs(struct sndstat_file *pf, void *arg_buf, size_t *arg_nbytes)
{
	int err;

	SNDSTAT_LOCK();
	sx_xlock(&pf->lock);

	if (pf->devs_nvlbuf == NULL) {
		nvlist_t *nvl;
		void *nvlbuf;
		size_t nbytes;
		int err;

		sx_xunlock(&pf->lock);

		err = sndstat_create_devs_nvlist(&nvl);
		if (err) {
			SNDSTAT_UNLOCK();
			return (err);
		}

		sx_xlock(&pf->lock);

		nvlbuf = nvlist_pack(nvl, &nbytes);
		err = nvlist_error(nvl);
		nvlist_destroy(nvl);
		if (nvlbuf == NULL || err != 0) {
			SNDSTAT_UNLOCK();
			sx_xunlock(&pf->lock);
			if (err == 0)
				return (ENOMEM);
			return (err);
		}

		free(pf->devs_nvlbuf, M_NVLIST);
		pf->devs_nvlbuf = nvlbuf;
		pf->devs_nbytes = nbytes;
	}

	SNDSTAT_UNLOCK();

	if (*arg_nbytes == 0) {
		*arg_nbytes = pf->devs_nbytes;
		err = 0;
		goto done;
	}
	if (*arg_nbytes < pf->devs_nbytes) {
		*arg_nbytes = 0;
		err = 0;
		goto done;
	}

	err = copyout(pf->devs_nvlbuf, arg_buf, pf->devs_nbytes);
	if (err)
		goto done;

	*arg_nbytes = pf->devs_nbytes;

	free(pf->devs_nvlbuf, M_NVLIST);
	pf->devs_nvlbuf = NULL;
	pf->devs_nbytes = 0;

done:
	sx_unlock(&pf->lock);
	return (err);
}

static int
sndstat_unpack_user_nvlbuf(const void *unvlbuf, size_t nbytes, nvlist_t **nvl)
{
	void *nvlbuf;
	int err;

	nvlbuf = malloc(nbytes, M_DEVBUF, M_WAITOK);
	err = copyin(unvlbuf, nvlbuf, nbytes);
	if (err != 0) {
		free(nvlbuf, M_DEVBUF);
		return (err);
	}
	*nvl = nvlist_unpack(nvlbuf, nbytes, 0);
	free(nvlbuf, M_DEVBUF);
	if (*nvl == NULL) {
		return (EINVAL);
	}

	return (0);
}

static bool
sndstat_diinfo_is_sane(const nvlist_t *diinfo)
{
	if (!(nvlist_exists_number(diinfo, SNDST_DSPS_INFO_MIN_RATE) &&
	    nvlist_exists_number(diinfo, SNDST_DSPS_INFO_MAX_RATE) &&
	    nvlist_exists_number(diinfo, SNDST_DSPS_INFO_FORMATS) &&
	    nvlist_exists_number(diinfo, SNDST_DSPS_INFO_MIN_CHN) &&
	    nvlist_exists_number(diinfo, SNDST_DSPS_INFO_MAX_CHN)))
		return (false);
	return (true);
}

static bool
sndstat_dsp_nvlist_is_sane(const nvlist_t *nvlist)
{
	if (!(nvlist_exists_string(nvlist, SNDST_DSPS_DEVNODE) &&
	    nvlist_exists_string(nvlist, SNDST_DSPS_DESC) &&
	    nvlist_exists_number(nvlist, SNDST_DSPS_PCHAN) &&
	    nvlist_exists_number(nvlist, SNDST_DSPS_RCHAN)))
		return (false);

	if (nvlist_get_number(nvlist, SNDST_DSPS_PCHAN) > 0) {
		if (nvlist_exists_nvlist(nvlist, SNDST_DSPS_INFO_PLAY)) {
			if (!sndstat_diinfo_is_sane(nvlist_get_nvlist(nvlist,
			    SNDST_DSPS_INFO_PLAY)))
				return (false);
		} else if (!(nvlist_exists_number(nvlist, "pminrate") &&
		    nvlist_exists_number(nvlist, "pmaxrate") &&
		    nvlist_exists_number(nvlist, "pfmts")))
			return (false);
	}

	if (nvlist_get_number(nvlist, SNDST_DSPS_RCHAN) > 0) {
		if (nvlist_exists_nvlist(nvlist, SNDST_DSPS_INFO_REC)) {
			if (!sndstat_diinfo_is_sane(nvlist_get_nvlist(nvlist,
			    SNDST_DSPS_INFO_REC)))
				return (false);
		} else if (!(nvlist_exists_number(nvlist, "rminrate") &&
		    nvlist_exists_number(nvlist, "rmaxrate") &&
		    nvlist_exists_number(nvlist, "rfmts")))
			return (false);
	}
	
	return (true);

}

static void
sndstat_get_diinfo_nv(const nvlist_t *nv, uint32_t *min_rate,
	    uint32_t *max_rate, uint32_t *formats, uint32_t *min_chn,
	    uint32_t *max_chn)
{
	*min_rate = nvlist_get_number(nv, SNDST_DSPS_INFO_MIN_RATE);
	*max_rate = nvlist_get_number(nv, SNDST_DSPS_INFO_MAX_RATE);
	*formats = nvlist_get_number(nv, SNDST_DSPS_INFO_FORMATS);
	*min_chn = nvlist_get_number(nv, SNDST_DSPS_INFO_MIN_CHN);
	*max_chn = nvlist_get_number(nv, SNDST_DSPS_INFO_MAX_CHN);
}

static int
sndstat_dsp_unpack_nvlist(const nvlist_t *nvlist, struct sndstat_userdev *ud)
{
	const char *nameunit, *devnode, *desc;
	unsigned int pchan, rchan;
	uint32_t pminrate = 0, pmaxrate = 0;
	uint32_t rminrate = 0, rmaxrate = 0;
	uint32_t pfmts = 0, rfmts = 0;
	uint32_t pminchn = 0, pmaxchn = 0;
	uint32_t rminchn = 0, rmaxchn = 0;
	nvlist_t *provider_nvl = NULL;
	const nvlist_t *diinfo;
	const char *provider;

	devnode = nvlist_get_string(nvlist, SNDST_DSPS_DEVNODE);
	if (nvlist_exists_string(nvlist, SNDST_DSPS_NAMEUNIT))
		nameunit = nvlist_get_string(nvlist, SNDST_DSPS_NAMEUNIT);
	else
		nameunit = devnode;
	desc = nvlist_get_string(nvlist, SNDST_DSPS_DESC);
	pchan = nvlist_get_number(nvlist, SNDST_DSPS_PCHAN);
	rchan = nvlist_get_number(nvlist, SNDST_DSPS_RCHAN);
	if (pchan != 0) {
		if (nvlist_exists_nvlist(nvlist, SNDST_DSPS_INFO_PLAY)) {
			diinfo = nvlist_get_nvlist(nvlist,
			    SNDST_DSPS_INFO_PLAY);
			sndstat_get_diinfo_nv(diinfo, &pminrate, &pmaxrate,
			    &pfmts, &pminchn, &pmaxchn);
		} else {
			pminrate = nvlist_get_number(nvlist, "pminrate");
			pmaxrate = nvlist_get_number(nvlist, "pmaxrate");
			pfmts = nvlist_get_number(nvlist, "pfmts");
		}
	}
	if (rchan != 0) {
		if (nvlist_exists_nvlist(nvlist, SNDST_DSPS_INFO_REC)) {
			diinfo = nvlist_get_nvlist(nvlist,
			    SNDST_DSPS_INFO_REC);
			sndstat_get_diinfo_nv(diinfo, &rminrate, &rmaxrate,
			    &rfmts, &rminchn, &rmaxchn);
		} else {
			rminrate = nvlist_get_number(nvlist, "rminrate");
			rmaxrate = nvlist_get_number(nvlist, "rmaxrate");
			rfmts = nvlist_get_number(nvlist, "rfmts");
		}
	}

	provider = dnvlist_get_string(nvlist, SNDST_DSPS_PROVIDER, "");
	if (provider[0] == '\0')
		provider = NULL;

	if (provider != NULL &&
	    nvlist_exists_nvlist(nvlist, SNDST_DSPS_PROVIDER_INFO)) {
		provider_nvl = nvlist_clone(
		    nvlist_get_nvlist(nvlist, SNDST_DSPS_PROVIDER_INFO));
		if (provider_nvl == NULL)
			return (ENOMEM);
	}

	ud->provider = (provider != NULL) ? strdup(provider, M_DEVBUF) : NULL;
	ud->devnode = strdup(devnode, M_DEVBUF);
	ud->nameunit = strdup(nameunit, M_DEVBUF);
	ud->desc = strdup(desc, M_DEVBUF);
	ud->pchan = pchan;
	ud->rchan = rchan;
	ud->info_play.min_rate = pminrate;
	ud->info_play.max_rate = pmaxrate;
	ud->info_play.formats = pfmts;
	ud->info_play.min_chn = pminchn;
	ud->info_play.max_chn = pmaxchn;
	ud->info_rec.min_rate = rminrate;
	ud->info_rec.max_rate = rmaxrate;
	ud->info_rec.formats = rfmts;
	ud->info_rec.min_chn = rminchn;
	ud->info_rec.max_chn = rmaxchn;
	ud->provider_nvl = provider_nvl;
	return (0);
}

static int
sndstat_add_user_devs(struct sndstat_file *pf, void *nvlbuf, size_t nbytes)
{
	int err;
	nvlist_t *nvl = NULL;
	const nvlist_t * const *dsps;
	size_t i, ndsps;

	if ((pf->fflags & FWRITE) == 0) {
		err = EPERM;
		goto done;
	}

	if (nbytes > SNDST_UNVLBUF_MAX) {
		err = ENOMEM;
		goto done;
	}

	err = sndstat_unpack_user_nvlbuf(nvlbuf, nbytes, &nvl);
	if (err != 0)
		goto done;

	if (!nvlist_exists_nvlist_array(nvl, SNDST_DSPS)) {
		err = EINVAL;
		goto done;
	}
	dsps = nvlist_get_nvlist_array(nvl, SNDST_DSPS, &ndsps);
	for (i = 0; i < ndsps; i++) {
		if (!sndstat_dsp_nvlist_is_sane(dsps[i])) {
			err = EINVAL;
			goto done;
		}
	}
	sx_xlock(&pf->lock);
	for (i = 0; i < ndsps; i++) {
		struct sndstat_userdev *ud =
		    malloc(sizeof(*ud), M_DEVBUF, M_WAITOK);
		err = sndstat_dsp_unpack_nvlist(dsps[i], ud);
		if (err) {
			sx_unlock(&pf->lock);
			goto done;
		}
		TAILQ_INSERT_TAIL(&pf->userdev_list, ud, link);
	}
	sx_unlock(&pf->lock);

done:
	nvlist_destroy(nvl);
	return (err);
}

static int
sndstat_flush_user_devs(struct sndstat_file *pf)
{
	if ((pf->fflags & FWRITE) == 0)
		return (EPERM);

	sx_xlock(&pf->lock);
	sndstat_remove_all_userdevs(pf);
	sx_xunlock(&pf->lock);

	return (0);
}

static int
sndstat_ioctl(
    struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int err;
	struct sndstat_file *pf;
	struct sndstioc_nv_arg *arg;
#ifdef COMPAT_FREEBSD32
	struct sndstioc_nv_arg32 *arg32;
	size_t nbytes;
#endif

	err = devfs_get_cdevpriv((void **)&pf);
	if (err != 0)
		return (err);

	switch (cmd) {
	case SNDSTIOC_GET_DEVS:
		arg = (struct sndstioc_nv_arg *)data;
		err = sndstat_get_devs(pf, arg->buf, &arg->nbytes);
		break;
#ifdef COMPAT_FREEBSD32
	case SNDSTIOC_GET_DEVS32:
		arg32 = (struct sndstioc_nv_arg32 *)data;
		nbytes = arg32->nbytes;
		err = sndstat_get_devs(pf, (void *)(uintptr_t)arg32->buf,
		    &nbytes);
		if (err == 0) {
			KASSERT(nbytes < UINT_MAX, ("impossibly many bytes"));
			arg32->nbytes = nbytes;
		}
		break;
#endif
	case SNDSTIOC_ADD_USER_DEVS:
		arg = (struct sndstioc_nv_arg *)data;
		err = sndstat_add_user_devs(pf, arg->buf, arg->nbytes);
		break;
#ifdef COMPAT_FREEBSD32
	case SNDSTIOC_ADD_USER_DEVS32:
		arg32 = (struct sndstioc_nv_arg32 *)data;
		err = sndstat_add_user_devs(pf, (void *)(uintptr_t)arg32->buf,
		    arg32->nbytes);
		break;
#endif
	case SNDSTIOC_REFRESH_DEVS:
		err = sndstat_refresh_devs(pf);
		break;
	case SNDSTIOC_FLUSH_USER_DEVS:
		err = sndstat_flush_user_devs(pf);
		break;
	default:
		err = ENODEV;
	}

	return (err);
}

static struct sndstat_userdev *
sndstat_line2userdev(struct sndstat_file *pf, const char *line, int n)
{
	struct sndstat_userdev *ud;
	const char *e, *m;

	ud = malloc(sizeof(*ud), M_DEVBUF, M_WAITOK|M_ZERO);

	ud->provider = NULL;
	ud->provider_nvl = NULL;
	e = strchr(line, ':');
	if (e == NULL)
		goto fail;
	ud->nameunit = strndup(line, e - line, M_DEVBUF);
	ud->devnode = malloc(e - line + 1, M_DEVBUF, M_WAITOK | M_ZERO);
	strlcat(ud->devnode, ud->nameunit, e - line + 1);
	line = e + 1;

	e = strchr(line, '<');
	if (e == NULL)
		goto fail;
	line = e + 1;
	e = strrchr(line, '>');
	if (e == NULL)
		goto fail;
	ud->desc = strndup(line, e - line, M_DEVBUF);
	line = e + 1;

	e = strchr(line, '(');
	if (e == NULL)
		goto fail;
	line = e + 1;
	e = strrchr(line, ')');
	if (e == NULL)
		goto fail;
	m = strstr(line, "play");
	if (m != NULL && m < e)
		ud->pchan = 1;
	m = strstr(line, "rec");
	if (m != NULL && m < e)
		ud->rchan = 1;

	return (ud);

fail:
	free(ud->nameunit, M_DEVBUF);
	free(ud->devnode, M_DEVBUF);
	free(ud->desc, M_DEVBUF);
	free(ud, M_DEVBUF);
	return (NULL);
}

/************************************************************************/

int
sndstat_register(device_t dev, char *str)
{
	struct sndstat_entry *ent;
	struct sndstat_entry *pre;
	const char *devtype;
	int type, unit;

	unit = device_get_unit(dev);
	devtype = device_get_name(dev);
	if (!strcmp(devtype, "pcm"))
		type = SS_TYPE_PCM;
	else if (!strcmp(devtype, "midi"))
		type = SS_TYPE_MIDI;
	else if (!strcmp(devtype, "sequencer"))
		type = SS_TYPE_SEQUENCER;
	else
		return (EINVAL);

	ent = malloc(sizeof *ent, M_DEVBUF, M_WAITOK | M_ZERO);
	ent->dev = dev;
	ent->str = str;
	ent->type = type;
	ent->unit = unit;

	SNDSTAT_LOCK();
	/* sorted list insertion */
	TAILQ_FOREACH(pre, &sndstat_devlist, link) {
		if (pre->unit > unit)
			break;
		else if (pre->unit < unit)
			continue;
		if (pre->type > type)
			break;
		else if (pre->type < unit)
			continue;
	}
	if (pre == NULL) {
		TAILQ_INSERT_TAIL(&sndstat_devlist, ent, link);
	} else {
		TAILQ_INSERT_BEFORE(pre, ent, link);
	}
	SNDSTAT_UNLOCK();

	return (0);
}

int
sndstat_unregister(device_t dev)
{
	struct sndstat_entry *ent;
	int error = ENXIO;

	SNDSTAT_LOCK();
	TAILQ_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->dev == dev) {
			TAILQ_REMOVE(&sndstat_devlist, ent, link);
			free(ent, M_DEVBUF);
			error = 0;
			break;
		}
	}
	SNDSTAT_UNLOCK();

	return (error);
}

/************************************************************************/

static int
sndstat_prepare_pcm(struct sbuf *s, device_t dev, int verbose)
{
	struct snddev_info *d;
	struct pcm_channel *c;
	struct pcm_feeder *f;

	d = device_get_softc(dev);
	PCM_BUSYASSERT(d);

	if (CHN_EMPTY(d, channels.pcm)) {
		sbuf_printf(s, " (mixer only)");
		return (0);
	}

	if (verbose < 1) {
		sbuf_printf(s, " (%s%s%s",
		    d->playcount ? "play" : "",
		    (d->playcount && d->reccount) ? "/" : "",
		    d->reccount ? "rec" : "");
	} else {
		sbuf_printf(s, " (%dp:%dv/%dr:%dv",
		    d->playcount, d->pvchancount,
		    d->reccount, d->rvchancount);
	}
	sbuf_printf(s, "%s)%s",
	    ((d->playcount != 0 && d->reccount != 0) &&
	    (d->flags & SD_F_SIMPLEX)) ? " simplex" : "",
	    (device_get_unit(dev) == snd_unit) ? " default" : "");

	if (verbose <= 1)
		return (0);

	sbuf_printf(s, "\n\t");
	sbuf_printf(s, "snddev flags=0x%b", d->flags, SD_F_BITS);

	CHN_FOREACH(c, d, channels.pcm) {
		KASSERT(c->bufhard != NULL && c->bufsoft != NULL,
		    ("hosed pcm channel setup"));

		CHN_LOCK(c);

		sbuf_printf(s, "\n\t");

		sbuf_printf(s, "%s[%s]: ",
		    (c->parentchannel != NULL) ?
		    c->parentchannel->name : "", c->name);
		sbuf_printf(s, "spd %d", c->speed);
		if (c->speed != sndbuf_getspd(c->bufhard)) {
			sbuf_printf(s, "/%d",
			    sndbuf_getspd(c->bufhard));
		}
		sbuf_printf(s, ", fmt 0x%08x", c->format);
		if (c->format != sndbuf_getfmt(c->bufhard)) {
			sbuf_printf(s, "/0x%08x",
			    sndbuf_getfmt(c->bufhard));
		}
		sbuf_printf(s, ", flags 0x%08x, 0x%08x",
		    c->flags, c->feederflags);
		if (c->pid != -1) {
			sbuf_printf(s, ", pid %d (%s)",
			    c->pid, c->comm);
		}
		sbuf_printf(s, "\n\t");

		sbuf_printf(s, "\tinterrupts %d, ", c->interrupts);

		if (c->direction == PCMDIR_REC)	{
			sbuf_printf(s,
			    "overruns %d, feed %u, hfree %d, "
			    "sfree %d\n\t\t[b:%d/%d/%d|bs:%d/%d/%d]",
				c->xruns, c->feedcount,
				sndbuf_getfree(c->bufhard),
				sndbuf_getfree(c->bufsoft),
				sndbuf_getsize(c->bufhard),
				sndbuf_getblksz(c->bufhard),
				sndbuf_getblkcnt(c->bufhard),
				sndbuf_getsize(c->bufsoft),
				sndbuf_getblksz(c->bufsoft),
				sndbuf_getblkcnt(c->bufsoft));
		} else {
			sbuf_printf(s,
			    "underruns %d, feed %u, ready %d "
			    "\n\t\t[b:%d/%d/%d|bs:%d/%d/%d]",
				c->xruns, c->feedcount,
				sndbuf_getready(c->bufsoft),
				sndbuf_getsize(c->bufhard),
				sndbuf_getblksz(c->bufhard),
				sndbuf_getblkcnt(c->bufhard),
				sndbuf_getsize(c->bufsoft),
				sndbuf_getblksz(c->bufsoft),
				sndbuf_getblkcnt(c->bufsoft));
		}
		sbuf_printf(s, "\n\t");

		sbuf_printf(s, "\tchannel flags=0x%b", c->flags, CHN_F_BITS);
		sbuf_printf(s, "\n\t");

		if (c->parentchannel != NULL) {
			sbuf_printf(s, "\t{%s}", (c->direction == PCMDIR_REC) ?
			    c->parentchannel->name : "userland");
		} else {
			sbuf_printf(s, "\t{%s}", (c->direction == PCMDIR_REC) ?
			    "hardware" : "userland");
		}
		sbuf_printf(s, " -> ");
		f = c->feeder;
		while (f->source != NULL)
			f = f->source;
		while (f != NULL) {
			sbuf_printf(s, "%s", f->class->name);
			if (f->desc->type == FEEDER_FORMAT) {
				sbuf_printf(s, "(0x%08x -> 0x%08x)",
				    f->desc->in, f->desc->out);
			} else if (f->desc->type == FEEDER_MATRIX) {
				sbuf_printf(s, "(%d.%d -> %d.%d)",
				    AFMT_CHANNEL(f->desc->in) -
				    AFMT_EXTCHANNEL(f->desc->in),
				    AFMT_EXTCHANNEL(f->desc->in),
				    AFMT_CHANNEL(f->desc->out) -
				    AFMT_EXTCHANNEL(f->desc->out),
				    AFMT_EXTCHANNEL(f->desc->out));
			} else if (f->desc->type == FEEDER_RATE) {
				sbuf_printf(s,
				    "(0x%08x q:%d %d -> %d)",
				    f->desc->out,
				    FEEDER_GET(f, FEEDRATE_QUALITY),
				    FEEDER_GET(f, FEEDRATE_SRC),
				    FEEDER_GET(f, FEEDRATE_DST));
			} else {
				sbuf_printf(s, "(0x%08x)",
				    f->desc->out);
			}
			sbuf_printf(s, " -> ");
			f = f->parent;
		}
		if (c->parentchannel != NULL) {
			sbuf_printf(s, "{%s}", (c->direction == PCMDIR_REC) ?
			    "userland" : c->parentchannel->name);
		} else {
			sbuf_printf(s, "{%s}", (c->direction == PCMDIR_REC) ?
			    "userland" : "hardware");
		}

		CHN_UNLOCK(c);
	}

	return (0);
}

static int
sndstat_prepare(struct sndstat_file *pf_self)
{
	struct sbuf *s = &pf_self->sbuf;
	struct sndstat_entry *ent;
	struct snddev_info *d;
	struct sndstat_file *pf;
    	int k;

	/* make sure buffer is reset */
	sbuf_clear(s);

	if (snd_verbose > 0)
		sbuf_printf(s, "FreeBSD Audio Driver\n");

	/* generate list of installed devices */
	k = 0;
	TAILQ_FOREACH(ent, &sndstat_devlist, link) {
		d = device_get_softc(ent->dev);
		if (!PCM_REGISTERED(d))
			continue;
		if (!k++)
			sbuf_printf(s, "Installed devices:\n");
		sbuf_printf(s, "%s:", device_get_nameunit(ent->dev));
		sbuf_printf(s, " <%s>", device_get_desc(ent->dev));
		if (snd_verbose > 0)
			sbuf_printf(s, " %s", ent->str);
		/* XXX Need Giant magic entry ??? */
		PCM_ACQUIRE_QUICK(d);
		sndstat_prepare_pcm(s, ent->dev, snd_verbose);
		PCM_RELEASE_QUICK(d);
		sbuf_printf(s, "\n");
	}
	if (k == 0)
		sbuf_printf(s, "No devices installed.\n");

	/* append any input from userspace */
	k = 0;
	TAILQ_FOREACH(pf, &sndstat_filelist, entry) {
		struct sndstat_userdev *ud;

		if (pf == pf_self)
			continue;
		sx_xlock(&pf->lock);
		if (TAILQ_EMPTY(&pf->userdev_list)) {
			sx_unlock(&pf->lock);
			continue;
		}
		if (!k++)
			sbuf_printf(s, "Installed devices from userspace:\n");
		TAILQ_FOREACH(ud, &pf->userdev_list, link) {
			const char *caps = (ud->pchan && ud->rchan) ?
			    "play/rec" :
			    (ud->pchan ? "play" : (ud->rchan ? "rec" : ""));
			sbuf_printf(s, "%s: <%s>", ud->nameunit, ud->desc);
			sbuf_printf(s, " (%s)", caps);
			sbuf_printf(s, "\n");
		}
		sx_unlock(&pf->lock);
	}
	if (k == 0)
		sbuf_printf(s, "No devices installed from userspace.\n");

	sbuf_finish(s);
    	return (sbuf_len(s));
}

static void
sndstat_sysinit(void *p)
{
	sx_init(&sndstat_lock, "sndstat lock");
	sndstat_dev = make_dev(&sndstat_cdevsw, SND_DEV_STATUS,
	    UID_ROOT, GID_WHEEL, 0644, "sndstat");
}
SYSINIT(sndstat_sysinit, SI_SUB_DRIVERS, SI_ORDER_FIRST, sndstat_sysinit, NULL);

static void
sndstat_sysuninit(void *p)
{
	if (sndstat_dev != NULL) {
		/* destroy_dev() will wait for all references to go away */
		destroy_dev(sndstat_dev);
	}
	sx_destroy(&sndstat_lock);
}
SYSUNINIT(sndstat_sysuninit, SI_SUB_DRIVERS, SI_ORDER_FIRST, sndstat_sysuninit, NULL);
