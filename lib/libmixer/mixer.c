/*-
 * Copyright (c) 2021 Christos Margiolis <christos@FreeBSD.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mixer.h"

#define	BASEPATH "/dev/mixer"

static int _mixer_readvol(struct mixer *, struct mix_dev *);

/*
 * Fetch volume from the device.
 */
static int
_mixer_readvol(struct mixer *m, struct mix_dev *dev)
{
	int v;

	if (ioctl(m->fd, MIXER_READ(dev->devno), &v) < 0)
		return (-1);
	dev->vol.left = MIX_VOLNORM(v & 0x00ff);
	dev->vol.right = MIX_VOLNORM((v >> 8) & 0x00ff);

	return (0);
}

/*
 * Open a mixer device in `/dev/mixerN`, where N is the number of the mixer.
 * Each device maps to an actual pcm audio card, so `/dev/mixer0` is the
 * mixer for pcm0, and so on.
 *
 * @param name		path to mixer device. NULL or "/dev/mixer" for the
 *			the default mixer (i.e `hw.snd.default_unit`).
 */
struct mixer *
mixer_open(const char *name)
{
	struct mixer *m = NULL;
	struct mix_dev *dp;
	const char *names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;
	int i;

	if ((m = calloc(1, sizeof(struct mixer))) == NULL)
		goto fail;

	if (name != NULL) {
		/* `name` does not start with "/dev/mixer". */
		if (strncmp(name, BASEPATH, strlen(BASEPATH)) != 0) {
			m->unit = -1;
		} else {
			/* `name` is "/dev/mixer" so, we'll use the default unit. */
			if (strncmp(name, BASEPATH, strlen(name)) == 0)
				goto dunit;
			m->unit = strtol(name + strlen(BASEPATH), NULL, 10);
		}
		(void)strlcpy(m->name, name, sizeof(m->name));
	} else {
dunit:
		if ((m->unit = mixer_get_dunit()) < 0)
			goto fail;
		(void)snprintf(m->name, sizeof(m->name), "/dev/mixer%d", m->unit);
	}

	if ((m->fd = open(m->name, O_RDWR)) < 0)
		goto fail;

	m->devmask = m->recmask = m->recsrc = 0;
	m->f_default = m->unit == mixer_get_dunit();
	m->mode = mixer_get_mode(m->unit);
	/* The unit number _must_ be set before the ioctl. */
	m->mi.dev = m->unit;
	m->ci.card = m->unit;
	if (ioctl(m->fd, SNDCTL_MIXERINFO, &m->mi) < 0) {
		memset(&m->mi, 0, sizeof(m->mi));
		strlcpy(m->mi.name, m->name, sizeof(m->mi.name));
	}
	if (ioctl(m->fd, SNDCTL_CARDINFO, &m->ci) < 0)
		memset(&m->ci, 0, sizeof(m->ci));
	if (ioctl(m->fd, SOUND_MIXER_READ_DEVMASK, &m->devmask) < 0 ||
	    ioctl(m->fd, SOUND_MIXER_READ_MUTE, &m->mutemask) < 0 ||
	    ioctl(m->fd, SOUND_MIXER_READ_RECMASK, &m->recmask) < 0 ||
	    ioctl(m->fd, SOUND_MIXER_READ_RECSRC, &m->recsrc) < 0)
		goto fail;

	TAILQ_INIT(&m->devs);
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (!MIX_ISDEV(m, i))
			continue;
		if ((dp = calloc(1, sizeof(struct mix_dev))) == NULL)
			goto fail;
		dp->parent_mixer = m;
		dp->devno = i;
		dp->nctl = 0;
		if (_mixer_readvol(m, dp) < 0)
			goto fail;
		(void)strlcpy(dp->name, names[i], sizeof(dp->name));
		TAILQ_INIT(&dp->ctls);
		TAILQ_INSERT_TAIL(&m->devs, dp, devs);
		m->ndev++;
	}

	/* The default device is always "vol". */
	m->dev = TAILQ_FIRST(&m->devs);

	return (m);
fail:
	if (m != NULL)
		(void)mixer_close(m);

	return (NULL);
}

/*
 * Free resources and close the mixer.
 */
int
mixer_close(struct mixer *m)
{
	struct mix_dev *dp;
	int r;

	r = close(m->fd);
	while (!TAILQ_EMPTY(&m->devs)) {
		dp = TAILQ_FIRST(&m->devs);
		TAILQ_REMOVE(&m->devs, dp, devs);
		while (!TAILQ_EMPTY(&dp->ctls))
			(void)mixer_remove_ctl(TAILQ_FIRST(&dp->ctls));
		free(dp);
	}
	free(m);

	return (r);
}

/*
 * Select a mixer device. The mixer structure keeps a list of all the devices
 * the mixer has, but only one can be manipulated at a time -- this is what
 * the `dev` in the mixer structure field is for. Each time a device is to be
 * manipulated, `dev` has to point to it first.
 *
 * The caller must manually assign the return value to `m->dev`.
 */
struct mix_dev *
mixer_get_dev(struct mixer *m, int dev)
{
	struct mix_dev *dp;

	if (dev < 0 || dev >= m->ndev) {
		errno = ERANGE;
		return (NULL);
	}
	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (dp->devno == dev)
			return (dp);
	}
	errno = EINVAL;

	return (NULL);
}

/*
 * Select a device by name.
 *
 * @param name		device name (e.g vol, pcm, ...)
 */
struct mix_dev *
mixer_get_dev_byname(struct mixer *m, const char *name)
{
	struct mix_dev *dp;

	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (!strncmp(dp->name, name, sizeof(dp->name)))
			return (dp);
	}
	errno = EINVAL;

	return (NULL);
}

/*
 * Add a mixer control to a device.
 */
int
mixer_add_ctl(struct mix_dev *parent_dev, int id, const char *name,
    int (*mod)(struct mix_dev *, void *),
    int (*print)(struct mix_dev *, void *))
{
	struct mix_dev *dp;
	mix_ctl_t *ctl, *cp;

	/* XXX: should we accept NULL name? */
	if (parent_dev == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if ((ctl = calloc(1, sizeof(mix_ctl_t))) == NULL)
		return (-1);
	ctl->parent_dev = parent_dev;
	ctl->id = id;
	if (name != NULL)
		(void)strlcpy(ctl->name, name, sizeof(ctl->name));
	ctl->mod = mod;
	ctl->print = print;
	dp = ctl->parent_dev;
	/* Make sure the same ID or name doesn't exist already. */
	TAILQ_FOREACH(cp, &dp->ctls, ctls) {
		if (!strncmp(cp->name, name, sizeof(cp->name)) || cp->id == id) {
			errno = EINVAL;
			return (-1);
		}
	}
	TAILQ_INSERT_TAIL(&dp->ctls, ctl, ctls);
	dp->nctl++;

	return (0);
}

/*
 * Same as `mixer_add_ctl`.
 */
int
mixer_add_ctl_s(mix_ctl_t *ctl)
{
	if (ctl == NULL)
		return (-1);

	return (mixer_add_ctl(ctl->parent_dev, ctl->id, ctl->name,
	    ctl->mod, ctl->print));
}

/*
 * Remove a mixer control from a device.
 */
int
mixer_remove_ctl(mix_ctl_t *ctl)
{
	struct mix_dev *p;

	if (ctl == NULL) {
		errno = EINVAL;
		return (-1);
	}
	p = ctl->parent_dev;
	if (!TAILQ_EMPTY(&p->ctls)) {
		TAILQ_REMOVE(&p->ctls, ctl, ctls);
		free(ctl);
	}

	return (0);
}

/*
 * Get a mixer control by id.
 */
mix_ctl_t *
mixer_get_ctl(struct mix_dev *d, int id)
{
	mix_ctl_t *cp;

	TAILQ_FOREACH(cp, &d->ctls, ctls) {
		if (cp->id == id)
			return (cp);
	}
	errno = EINVAL;

	return (NULL);
}

/*
 * Get a mixer control by name.
 */
mix_ctl_t *
mixer_get_ctl_byname(struct mix_dev *d, const char *name)
{
	mix_ctl_t *cp;

	TAILQ_FOREACH(cp, &d->ctls, ctls) {
		if (!strncmp(cp->name, name, sizeof(cp->name)))
			return (cp);
	}
	errno = EINVAL;

	return (NULL);
}

/*
 * Change the mixer's left and right volume. The allowed volume values are
 * between MIX_VOLMIN and MIX_VOLMAX. The `ioctl` for volume change requires
 * an integer value between 0 and 100 stored as `lvol | rvol << 8` --  for
 * that reason, we de-normalize the 32-bit float volume value, before
 * we pass it to the `ioctl`.
 *
 * Volume clumping should be done by the caller.
 */
int
mixer_set_vol(struct mixer *m, mix_volume_t vol)
{
	int v;

	if (vol.left < MIX_VOLMIN || vol.left > MIX_VOLMAX ||
	    vol.right < MIX_VOLMIN || vol.right > MIX_VOLMAX) {
		errno = ERANGE;
		return (-1);
	}
	v = MIX_VOLDENORM(vol.left) | MIX_VOLDENORM(vol.right) << 8;
	if (ioctl(m->fd, MIXER_WRITE(m->dev->devno), &v) < 0)
		return (-1);
	if (_mixer_readvol(m, m->dev) < 0)
		return (-1);

	return (0);
}

/*
 * Manipulate a device's mute.
 *
 * @param opt		MIX_MUTE mute device
 *			MIX_UNMUTE unmute device
 *			MIX_TOGGLEMUTE toggle device's mute
 */
int
mixer_set_mute(struct mixer *m, int opt)
{
	switch (opt) {
	case MIX_MUTE:
		m->mutemask |= (1 << m->dev->devno);
		break;
	case MIX_UNMUTE:
		m->mutemask &= ~(1 << m->dev->devno);
		break;
	case MIX_TOGGLEMUTE:
		m->mutemask ^= (1 << m->dev->devno);
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	if (ioctl(m->fd, SOUND_MIXER_WRITE_MUTE, &m->mutemask) < 0)
		return (-1);
	if (ioctl(m->fd, SOUND_MIXER_READ_MUTE, &m->mutemask) < 0)
		return (-1);

	return 0;
}

/*
 * Modify a recording device. The selected device has to be a recording device,
 * otherwise the function will fail.
 *
 * @param opt		MIX_ADDRECSRC add device to recording sources
 *			MIX_REMOVERECSRC remove device from recording sources
 *			MIX_SETRECSRC set device as the only recording source
 *			MIX_TOGGLERECSRC toggle device from recording sources
 */
int
mixer_mod_recsrc(struct mixer *m, int opt)
{
	if (!m->recmask || !MIX_ISREC(m, m->dev->devno)) {
		errno = ENODEV;
		return (-1);
	}
	switch (opt) {
	case MIX_ADDRECSRC:
		m->recsrc |= (1 << m->dev->devno);
		break;
	case MIX_REMOVERECSRC:
		m->recsrc &= ~(1 << m->dev->devno);
		break;
	case MIX_SETRECSRC:
		m->recsrc = (1 << m->dev->devno);
		break;
	case MIX_TOGGLERECSRC:
		m->recsrc ^= (1 << m->dev->devno);
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	if (ioctl(m->fd, SOUND_MIXER_WRITE_RECSRC, &m->recsrc) < 0)
		return (-1);
	if (ioctl(m->fd, SOUND_MIXER_READ_RECSRC, &m->recsrc) < 0)
		return (-1);

	return (0);
}

/*
 * Get default audio card's number. This is used to open the default mixer
 * and set the mixer structure's `f_default` flag.
 */
int
mixer_get_dunit(void)
{
	size_t size;
	int unit;

	size = sizeof(int);
	if (sysctlbyname("hw.snd.default_unit", &unit, &size, NULL, 0) < 0)
		return (-1);

	return (unit);
}

/*
 * Change the default audio card. This is normally _not_ a mixer feature, but
 * it's useful to have, so the caller can avoid having to manually use
 * the sysctl API.
 *
 * @param unit		the audio card number (e.g pcm0, pcm1, ...).
 */
int
mixer_set_dunit(struct mixer *m, int unit)
{
	size_t size;

	size = sizeof(int);
	if (sysctlbyname("hw.snd.default_unit", NULL, 0, &unit, size) < 0)
		return (-1);
	/* XXX: how will other mixers get updated? */
	m->f_default = m->unit == unit;

	return (0);
}

/*
 * Get sound device mode (none, play, rec, play+rec). Userland programs can
 * use the MIX_MODE_* flags to determine the mode of the device.
 */
int
mixer_get_mode(int unit)
{
	char buf[64];
	size_t size;
	unsigned int mode;

	(void)snprintf(buf, sizeof(buf), "dev.pcm.%d.mode", unit);
	size = sizeof(unsigned int);
	if (sysctlbyname(buf, &mode, &size, NULL, 0) < 0)
		return (0);

	return (mode);
}

/*
 * Get the total number of mixers in the system.
 */
int
mixer_get_nmixers(void)
{
	struct mixer *m;
	oss_sysinfo si;

	/*
	 * Open a dummy mixer because we need the `fd` field for the
	 * `ioctl` to work.
	 */
	if ((m = mixer_open(NULL)) == NULL)
		return (-1);
	if (ioctl(m->fd, OSS_SYSINFO, &si) < 0) {
		(void)mixer_close(m);
		return (-1);
	}
	(void)mixer_close(m);

	return (si.nummixers);
}
