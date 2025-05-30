/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * Portions Copyright (c) Ryan Beasley <ryan.beasley@gmail.com> - GSoC 2006
 * Copyright (c) 1999 Cameron Grant <cg@FreeBSD.org>
 * Copyright (c) 1997 Luigi Rizzo
 * All rights reserved.
 * Copyright (c) 2024-2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <dev/sound/pcm/vchan.h>
#include <dev/sound/pcm/dsp.h>
#include <sys/limits.h>
#include <sys/sysctl.h>

#include "feeder_if.h"

devclass_t pcm_devclass;

int snd_unit = -1;

static int snd_unit_auto = -1;
SYSCTL_INT(_hw_snd, OID_AUTO, default_auto, CTLFLAG_RWTUN,
    &snd_unit_auto, 0, "assign default unit to a newly attached device");

SYSCTL_NODE(_hw, OID_AUTO, snd, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Sound driver");

/**
 * @brief Unit number allocator for syncgroup IDs
 */
struct unrhdr *pcmsg_unrhdr = NULL;

void *
snd_mtxcreate(const char *desc, const char *type)
{
	struct mtx *m;

	m = malloc(sizeof(*m), M_DEVBUF, M_WAITOK | M_ZERO);
	mtx_init(m, desc, type, MTX_DEF);
	return m;
}

void
snd_mtxfree(void *m)
{
	struct mtx *mtx = m;

	mtx_destroy(mtx);
	free(mtx, M_DEVBUF);
}

void
snd_mtxassert(void *m)
{
#ifdef INVARIANTS
	struct mtx *mtx = m;

	mtx_assert(mtx, MA_OWNED);
#endif
}

int
snd_setup_intr(device_t dev, struct resource *res, int flags, driver_intr_t hand, void *param, void **cookiep)
{
	struct snddev_info *d;

	flags &= INTR_MPSAFE;
	flags |= INTR_TYPE_AV;
	d = device_get_softc(dev);
	if (d != NULL && (flags & INTR_MPSAFE))
		d->flags |= SD_F_MPSAFE;

	return bus_setup_intr(dev, res, flags, NULL, hand, param, cookiep);
}

static int
sysctl_hw_snd_default_unit(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	int error, unit;

	unit = snd_unit;
	error = sysctl_handle_int(oidp, &unit, 0, req);
	if (error == 0 && req->newptr != NULL) {
		bus_topo_lock();
		d = devclass_get_softc(pcm_devclass, unit);
		if (!PCM_REGISTERED(d) || CHN_EMPTY(d, channels.pcm)) {
			bus_topo_unlock();
			return EINVAL;
		}
		snd_unit = unit;
		snd_unit_auto = 0;
		bus_topo_unlock();
	}
	return (error);
}
/* XXX: do we need a way to let the user change the default unit? */
SYSCTL_PROC(_hw_snd, OID_AUTO, default_unit,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_ANYBODY | CTLFLAG_MPSAFE, 0,
    sizeof(int), sysctl_hw_snd_default_unit, "I",
    "default sound device");

int
pcm_addchan(device_t dev, int dir, kobj_class_t cls, void *devinfo)
{
	struct snddev_info *d = device_get_softc(dev);
	struct pcm_channel *ch;
	int err = 0;

	PCM_LOCK(d);
	PCM_WAIT(d);
	PCM_ACQUIRE(d);
	ch = chn_init(d, NULL, cls, dir, devinfo);
	if (!ch) {
		device_printf(d->dev, "chn_init(%s, %d, %p) failed\n",
		    cls->name, dir, devinfo);
		err = ENODEV;
	}
	PCM_RELEASE(d);
	PCM_UNLOCK(d);

	return (err);
}

static void
pcm_killchans(struct snddev_info *d)
{
	struct pcm_channel *ch;
	bool again;

	PCM_BUSYASSERT(d);
	KASSERT(!PCM_REGISTERED(d), ("%s(): still registered\n", __func__));

	for (;;) {
		again = false;
		/* Make sure all channels are stopped. */
		CHN_FOREACH(ch, d, channels.pcm) {
			CHN_LOCK(ch);
			if (ch->inprog == 0 && ch->sleeping == 0 &&
			    CHN_STOPPED(ch)) {
				CHN_UNLOCK(ch);
				continue;
			}
			chn_shutdown(ch);
			if (ch->direction == PCMDIR_PLAY)
				chn_flush(ch);
			else
				chn_abort(ch);
			CHN_UNLOCK(ch);
			again = true;
		}
		/*
		 * Some channels are still active. Sleep for a bit and try
		 * again.
		 */
		if (again)
			pause_sbt("pcmkillchans", mstosbt(5), 0, 0);
		else
			break;
	}

	/* All channels are finally dead. */
	while (!CHN_EMPTY(d, channels.pcm)) {
		ch = CHN_FIRST(d, channels.pcm);
		chn_kill(ch);
	}

	if (d->p_unr != NULL)
		delete_unrhdr(d->p_unr);
	if (d->vp_unr != NULL)
		delete_unrhdr(d->vp_unr);
	if (d->r_unr != NULL)
		delete_unrhdr(d->r_unr);
	if (d->vr_unr != NULL)
		delete_unrhdr(d->vr_unr);
}

static int
pcm_best_unit(int old)
{
	struct snddev_info *d;
	int i, best, bestprio, prio;

	best = -1;
	bestprio = -100;
	bus_topo_lock();
	for (i = 0; pcm_devclass != NULL &&
	    i < devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (!PCM_REGISTERED(d))
			continue;
		prio = 0;
		if (d->playcount == 0)
			prio -= 10;
		if (d->reccount == 0)
			prio -= 2;
		if (prio > bestprio || (prio == bestprio && i == old)) {
			best = i;
			bestprio = prio;
		}
	}
	bus_topo_unlock();

	return (best);
}

uint32_t
pcm_getflags(device_t dev)
{
	struct snddev_info *d = device_get_softc(dev);

	return d->flags;
}

void
pcm_setflags(device_t dev, uint32_t val)
{
	struct snddev_info *d = device_get_softc(dev);

	d->flags = val;
}

void *
pcm_getdevinfo(device_t dev)
{
	struct snddev_info *d = device_get_softc(dev);

	return d->devinfo;
}

unsigned int
pcm_getbuffersize(device_t dev, unsigned int minbufsz, unsigned int deflt, unsigned int maxbufsz)
{
	struct snddev_info *d = device_get_softc(dev);
	int sz, x;

	sz = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev), "buffersize", &sz) == 0) {
		x = sz;
		RANGE(sz, minbufsz, maxbufsz);
		if (x != sz)
			device_printf(dev, "'buffersize=%d' hint is out of range (%d-%d), using %d\n", x, minbufsz, maxbufsz, sz);
		x = minbufsz;
		while (x < sz)
			x <<= 1;
		if (x > sz)
			x >>= 1;
		if (x != sz) {
			device_printf(dev, "'buffersize=%d' hint is not a power of 2, using %d\n", sz, x);
			sz = x;
		}
	} else {
		sz = deflt;
	}

	d->bufsz = sz;

	return sz;
}

static int
sysctl_dev_pcm_bitperfect(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	int err, val;

	d = oidp->oid_arg1;
	if (!PCM_REGISTERED(d))
		return (ENODEV);

	PCM_LOCK(d);
	PCM_WAIT(d);
	val = (d->flags & SD_F_BITPERFECT) ? 1 : 0;
	PCM_ACQUIRE(d);
	PCM_UNLOCK(d);

	err = sysctl_handle_int(oidp, &val, 0, req);

	if (err == 0 && req->newptr != NULL) {
		if (!(val == 0 || val == 1)) {
			PCM_RELEASE_QUICK(d);
			return (EINVAL);
		}

		PCM_LOCK(d);

		d->flags &= ~SD_F_BITPERFECT;
		d->flags |= (val != 0) ? SD_F_BITPERFECT : 0;

		PCM_RELEASE(d);
		PCM_UNLOCK(d);
	} else
		PCM_RELEASE_QUICK(d);

	return (err);
}

static int
sysctl_dev_pcm_mode(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	int mode = 0;

	d = oidp->oid_arg1;
	if (!PCM_REGISTERED(d))
		return (ENODEV);

	PCM_LOCK(d);
	if (d->playcount > 0)
		mode |= PCM_MODE_PLAY;
	if (d->reccount > 0)
		mode |= PCM_MODE_REC;
	if (d->mixer_dev != NULL)
		mode |= PCM_MODE_MIXER;
	PCM_UNLOCK(d);

	return (sysctl_handle_int(oidp, &mode, 0, req));
}

/*
 * Basic initialization so that drivers can use pcm_addchan() before
 * pcm_register().
 */
void
pcm_init(device_t dev, void *devinfo)
{
	struct snddev_info *d;
	int i;

	d = device_get_softc(dev);
	d->dev = dev;
	d->lock = snd_mtxcreate(device_get_nameunit(dev), "sound cdev");
	cv_init(&d->cv, device_get_nameunit(dev));

	i = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "vpc", &i) != 0 || i != 0)
		d->flags |= SD_F_VPC;

	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "bitperfect", &i) == 0 && i != 0)
		d->flags |= SD_F_BITPERFECT;

	d->devinfo = devinfo;
	d->reccount = 0;
	d->playcount = 0;
	d->pvchancount = 0;
	d->rvchancount = 0;
	d->pvchanrate = 0;
	d->pvchanformat = 0;
	d->rvchanrate = 0;
	d->rvchanformat = 0;
	d->p_unr = new_unrhdr(0, INT_MAX, NULL);
	d->vp_unr = new_unrhdr(0, INT_MAX, NULL);
	d->r_unr = new_unrhdr(0, INT_MAX, NULL);
	d->vr_unr = new_unrhdr(0, INT_MAX, NULL);

	CHN_INIT(d, channels.pcm);
	CHN_INIT(d, channels.pcm.busy);
	CHN_INIT(d, channels.pcm.opened);
	CHN_INIT(d, channels.pcm.primary);
}

int
pcm_register(device_t dev, char *str)
{
	struct snddev_info *d = device_get_softc(dev);

	/* should only be called once */
	if (d->flags & SD_F_REGISTERED)
		return (EINVAL);

	if (d->playcount == 0 || d->reccount == 0)
		d->flags |= SD_F_SIMPLEX;
	if (d->playcount > 0)
		d->flags |= SD_F_PVCHANS;
	if (d->reccount > 0)
		d->flags |= SD_F_RVCHANS;

	strlcpy(d->status, str, SND_STATUSLEN);

	/* Done, we're ready.. */
	d->flags |= SD_F_REGISTERED;

	/*
	 * Create all sysctls once SD_F_REGISTERED is set else
	 * tunable sysctls won't work:
	 */
	sysctl_ctx_init(&d->play_sysctl_ctx);
	d->play_sysctl_tree = SYSCTL_ADD_NODE(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "play",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "playback channels node");
	sysctl_ctx_init(&d->rec_sysctl_ctx);
	d->rec_sysctl_tree = SYSCTL_ADD_NODE(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "rec",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "recording channels node");

	/* XXX: a user should be able to set this with a control tool, the
	   sysadmin then needs min+max sysctls for this */
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
            OID_AUTO, "buffersize", CTLFLAG_RD, &d->bufsz, 0,
	    "allocated buffer size");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "bitperfect", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, d,
	    sizeof(d), sysctl_dev_pcm_bitperfect, "I",
	    "bit-perfect playback/recording (0=disable, 1=enable)");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "mode", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, d, sizeof(d),
	    sysctl_dev_pcm_mode, "I",
	    "mode (1=mixer, 2=play, 4=rec. The values are OR'ed if more than "
	    "one mode is supported)");
	vchan_initsys(dev);
	if (d->flags & SD_F_EQ)
		feeder_eq_initsys(dev);

	if (snd_unit_auto < 0)
		snd_unit_auto = (snd_unit < 0) ? 1 : 0;
	if (snd_unit < 0 || snd_unit_auto > 1)
		snd_unit = device_get_unit(dev);
	else if (snd_unit_auto == 1)
		snd_unit = pcm_best_unit(snd_unit);

	sndstat_register(dev, d->status);

	return (dsp_make_dev(dev));
}

int
pcm_unregister(device_t dev)
{
	struct snddev_info *d;

	d = device_get_softc(dev);

	if (!PCM_ALIVE(d)) {
		device_printf(dev, "unregister: device not configured\n");
		return (0);
	}

	PCM_LOCK(d);
	PCM_WAIT(d);

	d->flags &= ~SD_F_REGISTERED;

	PCM_ACQUIRE(d);
	PCM_UNLOCK(d);

	pcm_killchans(d);

	PCM_RELEASE_QUICK(d);

	if (d->play_sysctl_tree != NULL) {
		sysctl_ctx_free(&d->play_sysctl_ctx);
		d->play_sysctl_tree = NULL;
	}
	if (d->rec_sysctl_tree != NULL) {
		sysctl_ctx_free(&d->rec_sysctl_ctx);
		d->rec_sysctl_tree = NULL;
	}

	sndstat_unregister(dev);
	mixer_uninit(dev);
	dsp_destroy_dev(dev);

	cv_destroy(&d->cv);
	snd_mtxfree(d->lock);

	if (snd_unit == device_get_unit(dev)) {
		snd_unit = pcm_best_unit(-1);
		if (snd_unit_auto == 0)
			snd_unit_auto = 1;
	}

	return (0);
}

/************************************************************************/

/**
 * @brief	Handle OSSv4 SNDCTL_SYSINFO ioctl.
 *
 * @param si	Pointer to oss_sysinfo struct where information about the
 * 		sound subsystem will be written/copied.
 *
 * This routine returns information about the sound system, such as the
 * current OSS version, number of audio, MIDI, and mixer drivers, etc.
 * Also includes a bitmask showing which of the above types of devices
 * are open (busy).
 *
 * @note
 * Calling threads must not hold any snddev_info or pcm_channel locks.
 *
 * @author	Ryan Beasley <ryanb@FreeBSD.org>
 */
void
sound_oss_sysinfo(oss_sysinfo *si)
{
	static char si_product[] = "FreeBSD native OSS ABI";
	static char si_version[] = __XSTRING(__FreeBSD_version);
	static char si_license[] = "BSD";
	static int intnbits = sizeof(int) * 8;	/* Better suited as macro?
						   Must pester a C guru. */

	struct snddev_info *d;
	struct pcm_channel *c;
	int j;
	size_t i;

	strlcpy(si->product, si_product, sizeof(si->product));
	strlcpy(si->version, si_version, sizeof(si->version));
	si->versionnum = SOUND_VERSION;
	strlcpy(si->license, si_license, sizeof(si->license));

	/*
	 * Iterate over PCM devices and their channels, gathering up data
	 * for the numaudioengines and openedaudio fields.
	 */
	si->numaudioengines = 0;
	bzero((void *)&si->openedaudio, sizeof(si->openedaudio));

	j = 0;

	bus_topo_lock();
	for (i = 0; pcm_devclass != NULL &&
	    i < devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (!PCM_REGISTERED(d))
			continue;

		/* XXX Need Giant magic entry ??? */

		/* See note in function's docblock */
		PCM_UNLOCKASSERT(d);
		PCM_LOCK(d);

		si->numaudioengines += PCM_CHANCOUNT(d);

		CHN_FOREACH(c, d, channels.pcm) {
			CHN_UNLOCKASSERT(c);
			CHN_LOCK(c);
			if (c->flags & CHN_F_BUSY)
				si->openedaudio[j / intnbits] |=
				    (1 << (j % intnbits));
			CHN_UNLOCK(c);
			j++;
		}

		PCM_UNLOCK(d);
	}
	bus_topo_unlock();

	si->numsynths = 0;	/* OSSv4 docs:  this field is obsolete */
	/**
	 * @todo	Collect num{midis,timers}.
	 *
	 * Need access to sound/midi/midi.c::midistat_lock in order
	 * to safely touch midi_devices and get a head count of, well,
	 * MIDI devices.  midistat_lock is a global static (i.e., local to
	 * midi.c), but midi_devices is a regular global; should the mutex
	 * be publicized, or is there another way to get this information?
	 *
	 * NB:	MIDI/sequencer stuff is currently on hold.
	 */
	si->nummidis = 0;
	si->numtimers = 0;
	/*
	 * Set this to the maximum unit number so that applications will not
	 * break if they try to loop through all mixers and some of them are
	 * not available.
	 */
	bus_topo_lock();
	si->nummixers = devclass_get_maxunit(pcm_devclass);
	si->numcards = devclass_get_maxunit(pcm_devclass);
	si->numaudios = devclass_get_maxunit(pcm_devclass);
	bus_topo_unlock();
		/* OSSv4 docs:	Intended only for test apps; API doesn't
		   really have much of a concept of cards.  Shouldn't be
		   used by applications. */

	/**
	 * @todo	Fill in "busy devices" fields.
	 *
	 *  si->openedmidi = " MIDI devices
	 */
	bzero((void *)&si->openedmidi, sizeof(si->openedmidi));

	/*
	 * Si->filler is a reserved array, but according to docs each
	 * element should be set to -1.
	 */
	for (i = 0; i < nitems(si->filler); i++)
		si->filler[i] = -1;
}

int
sound_oss_card_info(oss_card_info *si)
{
	struct snddev_info *d;
	int i;

	bus_topo_lock();
	for (i = 0; pcm_devclass != NULL &&
	    i < devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (i != si->card)
			continue;

		if (!PCM_REGISTERED(d)) {
			snprintf(si->shortname, sizeof(si->shortname),
			    "pcm%d (n/a)", i);
			strlcpy(si->longname, "Device unavailable",
			    sizeof(si->longname));
			si->hw_info[0] = '\0';
			si->intr_count = si->ack_count = 0;
		} else {
			PCM_UNLOCKASSERT(d);
			PCM_LOCK(d);

			strlcpy(si->shortname, device_get_nameunit(d->dev),
			    sizeof(si->shortname));
			strlcpy(si->longname, device_get_desc(d->dev),
			    sizeof(si->longname));
			strlcpy(si->hw_info, d->status, sizeof(si->hw_info));
			si->intr_count = si->ack_count = 0;

			PCM_UNLOCK(d);
		}

		bus_topo_unlock();
		return (0);
	}
	bus_topo_unlock();

	return (ENXIO);
}

/************************************************************************/

static void
sound_global_init(void)
{
	if (snd_verbose < 0 || snd_verbose > 4)
		snd_verbose = 1;

	if (snd_unit < 0)
		snd_unit = -1;

	snd_vchans_enable = true;

	if (chn_latency < CHN_LATENCY_MIN ||
	    chn_latency > CHN_LATENCY_MAX)
		chn_latency = CHN_LATENCY_DEFAULT;

	if (chn_latency_profile < CHN_LATENCY_PROFILE_MIN ||
	    chn_latency_profile > CHN_LATENCY_PROFILE_MAX)
		chn_latency_profile = CHN_LATENCY_PROFILE_DEFAULT;

	if (feeder_rate_min < FEEDRATE_MIN ||
		    feeder_rate_max < FEEDRATE_MIN ||
		    feeder_rate_min > FEEDRATE_MAX ||
		    feeder_rate_max > FEEDRATE_MAX ||
		    !(feeder_rate_min < feeder_rate_max)) {
		feeder_rate_min = FEEDRATE_RATEMIN;
		feeder_rate_max = FEEDRATE_RATEMAX;
	}

	if (feeder_rate_round < FEEDRATE_ROUNDHZ_MIN ||
		    feeder_rate_round > FEEDRATE_ROUNDHZ_MAX)
		feeder_rate_round = FEEDRATE_ROUNDHZ;

	if (bootverbose)
		printf("%s: snd_unit=%d snd_vchans_enable=%d "
		    "latency=%d "
		    "feeder_rate_min=%d feeder_rate_max=%d "
		    "feeder_rate_round=%d\n",
		    __func__, snd_unit, snd_vchans_enable,
		    chn_latency,
		    feeder_rate_min, feeder_rate_max,
		    feeder_rate_round);
}

static int
sound_modevent(module_t mod, int type, void *data)
{
	int ret;

	ret = 0;
	switch (type) {
	case MOD_LOAD:
		pcm_devclass = devclass_create("pcm");
		pcmsg_unrhdr = new_unrhdr(1, INT_MAX, NULL);
		sound_global_init();
		break;
	case MOD_UNLOAD:
		if (pcmsg_unrhdr != NULL) {
			delete_unrhdr(pcmsg_unrhdr);
			pcmsg_unrhdr = NULL;
		}
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		ret = ENOTSUP;
	}

	return ret;
}

DEV_MODULE(sound, sound_modevent, NULL);
MODULE_VERSION(sound, SOUND_MODVER);
