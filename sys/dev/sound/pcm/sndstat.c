/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2005-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * Copyright (c) 2001 Cameron Grant <cg@FreeBSD.org>
 * Copyright (c) 2020 The FreeBSD Foundation
 * All rights reserved.
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
#ifdef COMPAT_FREEBSD32
#include <sys/sysent.h>
#endif

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/pcm.h>
#include <dev/sound/version.h>


SND_DECLARE_FILE("$FreeBSD$");

#define	SS_TYPE_MODULE		0
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
	sndstat_handler handler;
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
	uint32_t pminrate;
	uint32_t pmaxrate;
	uint32_t rminrate;
	uint32_t rmaxrate;
	uint32_t pfmts;
	uint32_t rfmts;
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
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NEEDGIANT, 0, sizeof(int),
    sysctl_hw_sndverbose, "I",
    "verbosity level");

static int
sndstat_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct sndstat_file *pf;

	pf = malloc(sizeof(*pf), M_DEVBUF, M_WAITOK | M_ZERO);

	if (sbuf_new(&pf->sbuf, NULL, 4096, SBUF_AUTOEXTEND) == NULL) {
		free(pf, M_DEVBUF);
		return (ENOMEM);
	}

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
sndstat_get_caps(struct snddev_info *d, bool play, uint32_t *min_rate,
    uint32_t *max_rate, uint32_t *fmts)
{
	struct pcm_channel *c;
	int dir;

	dir = play ? PCMDIR_PLAY : PCMDIR_REC;
	*min_rate = 0;
	*max_rate = 0;
	*fmts = 0;

	if (play && d->pvchancount > 0) {
		*min_rate = *max_rate = d->pvchanrate;
		*fmts = d->pvchanformat;
		return;
	} else if (!play && d->rvchancount > 0) {
		*min_rate = *max_rate = d->rvchanrate;
		*fmts = d->rvchanformat;
		return;
	}

	CHN_FOREACH(c, d, channels.pcm) {
		struct pcmchan_caps *caps;

		if (c->direction != dir || (c->flags & CHN_F_VIRTUAL) != 0)
			continue;

		CHN_LOCK(c);
		caps = chn_getcaps(c);
		*min_rate = caps->minspeed;
		*max_rate = caps->maxspeed;
		*fmts = chn_getformats(c);
		CHN_UNLOCK(c);
	}
}

static int
sndstat_build_sound4_nvlist(struct snddev_info *d, nvlist_t **dip)
{
	uint32_t maxrate, minrate, fmts;
	nvlist_t *di = NULL, *sound4di = NULL;
	int err;

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

	nvlist_add_bool(di, SNDSTAT_LABEL_FROM_USER, false);
	nvlist_add_stringf(di, SNDSTAT_LABEL_NAMEUNIT, "%s",
			device_get_nameunit(d->dev));
	nvlist_add_stringf(di, SNDSTAT_LABEL_DEVNODE, "dsp%d",
			device_get_unit(d->dev));
	nvlist_add_string(
			di, SNDSTAT_LABEL_DESC, device_get_desc(d->dev));

	PCM_ACQUIRE_QUICK(d);
	nvlist_add_number(di, SNDSTAT_LABEL_PCHAN, d->playcount);
	nvlist_add_number(di, SNDSTAT_LABEL_RCHAN, d->reccount);
	if (d->playcount > 0) {
		sndstat_get_caps(d, true, &minrate, &maxrate, &fmts);
		nvlist_add_number(di, SNDSTAT_LABEL_PMINRATE, minrate);
		nvlist_add_number(di, SNDSTAT_LABEL_PMAXRATE, maxrate);
		nvlist_add_number(di, SNDSTAT_LABEL_PFMTS, fmts);
	}
	if (d->reccount > 0) {
		sndstat_get_caps(d, false, &minrate, &maxrate, &fmts);
		nvlist_add_number(di, SNDSTAT_LABEL_RMINRATE, minrate);
		nvlist_add_number(di, SNDSTAT_LABEL_RMAXRATE, maxrate);
		nvlist_add_number(di, SNDSTAT_LABEL_RFMTS, fmts);
	}

	nvlist_add_number(sound4di, SNDSTAT_LABEL_SOUND4_UNIT,
			device_get_unit(d->dev)); // XXX: I want signed integer here
	nvlist_add_bool(
	    sound4di, SNDSTAT_LABEL_SOUND4_BITPERFECT, d->flags & SD_F_BITPERFECT);
	nvlist_add_number(sound4di, SNDSTAT_LABEL_SOUND4_PVCHAN, d->pvchancount);
	nvlist_add_number(sound4di, SNDSTAT_LABEL_SOUND4_RVCHAN, d->rvchancount);
	nvlist_move_nvlist(di, SNDSTAT_LABEL_PROVIDER_INFO, sound4di);
	sound4di = NULL;
	PCM_RELEASE_QUICK(d);
	nvlist_add_string(di, SNDSTAT_LABEL_PROVIDER, SNDSTAT_LABEL_SOUND4_PROVIDER);

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
	nvlist_t *di;
	int err;

	di = nvlist_create(0);
	if (di == NULL) {
		err = ENOMEM;
		goto done;
	}

	nvlist_add_bool(di, SNDSTAT_LABEL_FROM_USER, true);
	nvlist_add_number(di, SNDSTAT_LABEL_PCHAN, ud->pchan);
	nvlist_add_number(di, SNDSTAT_LABEL_RCHAN, ud->rchan);
	nvlist_add_string(di, SNDSTAT_LABEL_NAMEUNIT, ud->nameunit);
	nvlist_add_string(
			di, SNDSTAT_LABEL_DEVNODE, ud->devnode);
	nvlist_add_string(di, SNDSTAT_LABEL_DESC, ud->desc);
	if (ud->pchan != 0) {
		nvlist_add_number(
				di, SNDSTAT_LABEL_PMINRATE, ud->pminrate);
		nvlist_add_number(
				di, SNDSTAT_LABEL_PMAXRATE, ud->pmaxrate);
		nvlist_add_number(
				di, SNDSTAT_LABEL_PFMTS, ud->pfmts);
	}
	if (ud->rchan != 0) {
		nvlist_add_number(
				di, SNDSTAT_LABEL_RMINRATE, ud->rminrate);
		nvlist_add_number(
				di, SNDSTAT_LABEL_RMAXRATE, ud->rmaxrate);
		nvlist_add_number(
				di, SNDSTAT_LABEL_RFMTS, ud->rfmts);
	}
	nvlist_add_string(di, SNDSTAT_LABEL_PROVIDER,
	    (ud->provider != NULL) ? ud->provider : "");
	if (ud->provider_nvl != NULL)
		nvlist_add_nvlist(
		    di, SNDSTAT_LABEL_PROVIDER_INFO, ud->provider_nvl);

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

		if (ent->dev == NULL)
			continue;
		d = device_get_softc(ent->dev);
		if (!PCM_REGISTERED(d))
			continue;

		err = sndstat_build_sound4_nvlist(d, &di);
		if (err)
			goto done;

		nvlist_append_nvlist_array(nvl, SNDSTAT_LABEL_DSPS, di);
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
			nvlist_append_nvlist_array(nvl, SNDSTAT_LABEL_DSPS, di);
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
sndstat_get_devs(struct sndstat_file *pf, caddr_t data)
{
	int err;
	struct sndstat_nvlbuf_arg *arg = (struct sndstat_nvlbuf_arg *)data;

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

	if (!arg->nbytes) {
		arg->nbytes = pf->devs_nbytes;
		err = 0;
		goto done;
	}
	if (arg->nbytes < pf->devs_nbytes) {
		arg->nbytes = 0;
		err = 0;
		goto done;
	}

	err = copyout(pf->devs_nvlbuf, arg->buf, pf->devs_nbytes);
	if (err)
		goto done;

	arg->nbytes = pf->devs_nbytes;

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
	if (nvl == NULL) {
		return (EINVAL);
	}

	return (0);
}

static bool
sndstat_dsp_nvlist_is_sane(const nvlist_t *nvlist)
{
	if (!(nvlist_exists_string(nvlist, SNDSTAT_LABEL_DEVNODE) &&
	    nvlist_exists_string(nvlist, SNDSTAT_LABEL_DESC) &&
	    nvlist_exists_number(nvlist, SNDSTAT_LABEL_PCHAN) &&
	    nvlist_exists_number(nvlist, SNDSTAT_LABEL_RCHAN)))
		return (false);

	if (nvlist_get_number(nvlist, SNDSTAT_LABEL_PCHAN) > 0)
		if (!(nvlist_exists_number(nvlist, SNDSTAT_LABEL_PMINRATE) &&
		    nvlist_exists_number(nvlist, SNDSTAT_LABEL_PMAXRATE) &&
		    nvlist_exists_number(nvlist, SNDSTAT_LABEL_PFMTS)))
			return (false);

	if (nvlist_get_number(nvlist, SNDSTAT_LABEL_RCHAN) > 0)
		if (!(nvlist_exists_number(nvlist, SNDSTAT_LABEL_RMINRATE) &&
		    nvlist_exists_number(nvlist, SNDSTAT_LABEL_RMAXRATE) &&
		    nvlist_exists_number(nvlist, SNDSTAT_LABEL_RFMTS)))
			return (false);

	return (true);

}

static int
sndstat_dsp_unpack_nvlist(const nvlist_t *nvlist, struct sndstat_userdev *ud)
{
	const char *nameunit, *devnode, *desc;
	unsigned int pchan, rchan;
	uint32_t pminrate = 0, pmaxrate = 0;
	uint32_t rminrate = 0, rmaxrate = 0;
	uint32_t pfmts = 0, rfmts = 0;
	nvlist_t *provider_nvl = NULL;
	const char *provider;

	devnode = nvlist_get_string(nvlist, SNDSTAT_LABEL_DEVNODE);
	if (nvlist_exists_string(nvlist, SNDSTAT_LABEL_NAMEUNIT))
		nameunit = nvlist_get_string(nvlist, SNDSTAT_LABEL_NAMEUNIT);
	else
		nameunit = devnode;
	desc = nvlist_get_string(nvlist, SNDSTAT_LABEL_DESC);
	pchan = nvlist_get_number(nvlist, SNDSTAT_LABEL_PCHAN);
	rchan = nvlist_get_number(nvlist, SNDSTAT_LABEL_RCHAN);
	if (pchan != 0) {
		pminrate = nvlist_get_number(nvlist, SNDSTAT_LABEL_PMINRATE);
		pmaxrate = nvlist_get_number(nvlist, SNDSTAT_LABEL_PMAXRATE);
		pfmts = nvlist_get_number(nvlist, SNDSTAT_LABEL_PFMTS);
	}
	if (rchan != 0) {
		rminrate = nvlist_get_number(nvlist, SNDSTAT_LABEL_RMINRATE);
		rmaxrate = nvlist_get_number(nvlist, SNDSTAT_LABEL_RMAXRATE);
		rfmts = nvlist_get_number(nvlist, SNDSTAT_LABEL_RFMTS);
	}

	provider = dnvlist_get_string(nvlist, SNDSTAT_LABEL_PROVIDER, "");
	if (provider[0] == '\0')
		provider = NULL;

	if (provider != NULL &&
	    nvlist_exists_nvlist(nvlist, SNDSTAT_LABEL_PROVIDER_INFO)) {
		provider_nvl = nvlist_clone(
		    nvlist_get_nvlist(nvlist, SNDSTAT_LABEL_PROVIDER_INFO));
		if (provider_nvl == NULL)
			return (ENOMEM);
	}

	ud->provider = (provider != NULL) ? strdup(provider, M_DEVBUF) : NULL;
	ud->devnode = strdup(devnode, M_DEVBUF);
	ud->nameunit = strdup(nameunit, M_DEVBUF);
	ud->desc = strdup(desc, M_DEVBUF);
	ud->pchan = pchan;
	ud->rchan = rchan;
	ud->pminrate = pminrate;
	ud->pmaxrate = pmaxrate;
	ud->rminrate = rminrate;
	ud->rmaxrate = rmaxrate;
	ud->pfmts = pfmts;
	ud->rfmts = rfmts;
	ud->provider_nvl = provider_nvl;
	return (0);
}

static int
sndstat_add_user_devs(struct sndstat_file *pf, caddr_t data)
{
	int err;
	nvlist_t *nvl = NULL;
	const nvlist_t * const *dsps;
	size_t i, ndsps;
	struct sndstat_nvlbuf_arg *arg = (struct sndstat_nvlbuf_arg *)data;

	if ((pf->fflags & FWRITE) == 0) {
		err = EPERM;
		goto done;
	}

	err = sndstat_unpack_user_nvlbuf(arg->buf, arg->nbytes, &nvl);
	if (err != 0)
		goto done;

	if (!nvlist_exists_nvlist_array(nvl, SNDSTAT_LABEL_DSPS)) {
		err = EINVAL;
		goto done;
	}
	dsps = nvlist_get_nvlist_array(nvl, SNDSTAT_LABEL_DSPS, &ndsps);
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

#ifdef COMPAT_FREEBSD32
static int
compat_sndstat_get_devs32(struct sndstat_file *pf, caddr_t data)
{
	struct sndstat_nvlbuf_arg32 *arg32 = (struct sndstat_nvlbuf_arg32 *)data;
	struct sndstat_nvlbuf_arg arg;
	int err;

	arg.buf = (void *)(uintptr_t)arg32->buf;
	arg.nbytes = arg32->nbytes;

	err = sndstat_get_devs(pf, (caddr_t)&arg);
	if (err == 0) {
		arg32->buf = (uint32_t)(uintptr_t)arg.buf;
		arg32->nbytes = arg.nbytes;
	}

	return (err);
}

static int
compat_sndstat_add_user_devs32(struct sndstat_file *pf, caddr_t data)
{
	struct sndstat_nvlbuf_arg32 *arg32 = (struct sndstat_nvlbuf_arg32 *)data;
	struct sndstat_nvlbuf_arg arg;
	int err;

	arg.buf = (void *)(uintptr_t)arg32->buf;
	arg.nbytes = arg32->nbytes;

	err = sndstat_add_user_devs(pf, (caddr_t)&arg);
	if (err == 0) {
		arg32->buf = (uint32_t)(uintptr_t)arg.buf;
		arg32->nbytes = arg.nbytes;
	}

	return (err);
}
#endif

static int
sndstat_ioctl(
    struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int err;
	struct sndstat_file *pf;

	err = devfs_get_cdevpriv((void **)&pf);
	if (err != 0)
		return (err);

	switch (cmd) {
	case SNDSTAT_GET_DEVS:
		err = sndstat_get_devs(pf, data);
		break;
#ifdef COMPAT_FREEBSD32
	case SNDSTAT_GET_DEVS32:
		if (!SV_CURPROC_FLAG(SV_ILP32)) {
			err = ENODEV;
			break;
		}
		err = compat_sndstat_get_devs32(pf, data);
		break;
#endif
	case SNDSTAT_ADD_USER_DEVS:
		err = sndstat_add_user_devs(pf, data);
		break;
#ifdef COMPAT_FREEBSD32
	case SNDSTAT_ADD_USER_DEVS32:
		if (!SV_CURPROC_FLAG(SV_ILP32)) {
			err = ENODEV;
			break;
		}
		err = compat_sndstat_add_user_devs32(pf, data);
		break;
#endif
	case SNDSTAT_REFRESH_DEVS:
		err = sndstat_refresh_devs(pf);
		break;
	case SNDSTAT_FLUSH_USER_DEVS:
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
	ud->devnode = (char *)malloc(e - line + 1, M_DEVBUF, M_WAITOK | M_ZERO);
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
sndstat_register(device_t dev, char *str, sndstat_handler handler)
{
	struct sndstat_entry *ent;
	struct sndstat_entry *pre;
	const char *devtype;
	int type, unit;

	if (dev) {
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
	} else {
		type = SS_TYPE_MODULE;
		unit = -1;
	}

	ent = malloc(sizeof *ent, M_DEVBUF, M_WAITOK | M_ZERO);
	ent->dev = dev;
	ent->str = str;
	ent->type = type;
	ent->unit = unit;
	ent->handler = handler;

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
sndstat_registerfile(char *str)
{
	return (sndstat_register(NULL, str, NULL));
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

int
sndstat_unregisterfile(char *str)
{
	struct sndstat_entry *ent;
	int error = ENXIO;

	SNDSTAT_LOCK();
	TAILQ_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->dev == NULL && ent->str == str) {
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
sndstat_prepare(struct sndstat_file *pf_self)
{
	struct sbuf *s = &pf_self->sbuf;
	struct sndstat_entry *ent;
	struct snddev_info *d;
	struct sndstat_file *pf;
    	int k;

	/* make sure buffer is reset */
	sbuf_clear(s);

	if (snd_verbose > 0) {
		sbuf_printf(s, "FreeBSD Audio Driver (%ubit %d/%s)\n",
		    (u_int)sizeof(intpcm32_t) << 3, SND_DRV_VERSION,
		    MACHINE_ARCH);
	}

	/* generate list of installed devices */
	k = 0;
	TAILQ_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->dev == NULL)
			continue;
		d = device_get_softc(ent->dev);
		if (!PCM_REGISTERED(d))
			continue;
		if (!k++)
			sbuf_printf(s, "Installed devices:\n");
		sbuf_printf(s, "%s:", device_get_nameunit(ent->dev));
		sbuf_printf(s, " <%s>", device_get_desc(ent->dev));
		if (snd_verbose > 0)
			sbuf_printf(s, " %s", ent->str);
		if (ent->handler) {
			/* XXX Need Giant magic entry ??? */
			PCM_ACQUIRE_QUICK(d);
			ent->handler(s, ent->dev, snd_verbose);
			PCM_RELEASE_QUICK(d);
		}
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

	/* append any file versions */
	if (snd_verbose >= 3) {
		k = 0;
		TAILQ_FOREACH(ent, &sndstat_devlist, link) {
			if (ent->dev == NULL && ent->str != NULL) {
				if (!k++)
					sbuf_printf(s, "\nFile Versions:\n");
				sbuf_printf(s, "%s\n", ent->str);
			}
		}
		if (k == 0)
			sbuf_printf(s, "\nNo file versions.\n");
	}
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
