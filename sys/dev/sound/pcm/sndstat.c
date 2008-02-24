/*-
 * Copyright (c) 2001 Cameron Grant <cg@freebsd.org>
 * All rights reserved.
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

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>
#include <dev/sound/version.h>
#ifdef	USING_MUTEX
#include <sys/sx.h>
#endif

SND_DECLARE_FILE("$FreeBSD: src/sys/dev/sound/pcm/sndstat.c,v 1.28 2007/06/16 03:37:28 ariff Exp $");

#define	SS_TYPE_MODULE		0
#define	SS_TYPE_FIRST		1
#define	SS_TYPE_PCM		1
#define	SS_TYPE_MIDI		2
#define	SS_TYPE_SEQUENCER	3
#define	SS_TYPE_LAST		3

static d_open_t sndstat_open;
static d_close_t sndstat_close;
static d_read_t sndstat_read;

static struct cdevsw sndstat_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	sndstat_open,
	.d_close =	sndstat_close,
	.d_read =	sndstat_read,
	.d_name =	"sndstat",
};

struct sndstat_entry {
	SLIST_ENTRY(sndstat_entry) link;
	device_t dev;
	char *str;
	sndstat_handler handler;
	int type, unit;
};

#ifdef	USING_MUTEX
static struct mtx sndstat_lock;
#endif
static struct sbuf sndstat_sbuf;
static struct cdev *sndstat_dev = NULL;
static int sndstat_bufptr = -1;
static int sndstat_maxunit = -1;
static int sndstat_files = 0;

#define SNDSTAT_PID(x)		((pid_t)((intptr_t)((x)->si_drv1)))
#define SNDSTAT_PID_SET(x, y)	(x)->si_drv1 = (void *)((intptr_t)(y))
#define SNDSTAT_FLUSH()		do {					\
	if (sndstat_bufptr != -1) {					\
		sbuf_delete(&sndstat_sbuf);				\
		sndstat_bufptr = -1;					\
	}								\
} while(0)

static SLIST_HEAD(, sndstat_entry) sndstat_devlist = SLIST_HEAD_INITIALIZER(none);

int snd_verbose = 1;
#ifdef	USING_MUTEX
TUNABLE_INT("hw.snd.verbose", &snd_verbose);
#else
TUNABLE_INT_DECL("hw.snd.verbose", 1, snd_verbose);
#endif

#ifdef SND_DEBUG
static int
sysctl_hw_snd_sndstat_pid(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	if (sndstat_dev == NULL)
		return (EINVAL);

	mtx_lock(&sndstat_lock);
	val = (int)SNDSTAT_PID(sndstat_dev);
	mtx_unlock(&sndstat_lock);
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err == 0 && req->newptr != NULL && val == 0) {
		mtx_lock(&sndstat_lock);
		SNDSTAT_FLUSH();
		SNDSTAT_PID_SET(sndstat_dev, 0);
		mtx_unlock(&sndstat_lock);
	}
	return (err);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, sndstat_pid, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(int), sysctl_hw_snd_sndstat_pid, "I", "sndstat busy pid");
#endif

static int sndstat_prepare(struct sbuf *s);

static int
sysctl_hw_sndverbose(SYSCTL_HANDLER_ARGS)
{
	int error, verbose;

	verbose = snd_verbose;
	error = sysctl_handle_int(oidp, &verbose, 0, req);
	if (error == 0 && req->newptr != NULL) {
		mtx_lock(&sndstat_lock);
		if (verbose < 0 || verbose > 4)
			error = EINVAL;
		else
			snd_verbose = verbose;
		mtx_unlock(&sndstat_lock);
	}
	return error;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, verbose, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_sndverbose, "I", "verbosity level");

static int
sndstat_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	if (sndstat_dev == NULL || i_dev != sndstat_dev)
		return EBADF;

	mtx_lock(&sndstat_lock);
	if (SNDSTAT_PID(i_dev) != 0) {
		mtx_unlock(&sndstat_lock);
		return EBUSY;
	}
	SNDSTAT_PID_SET(i_dev, td->td_proc->p_pid);
	mtx_unlock(&sndstat_lock);
	if (sbuf_new(&sndstat_sbuf, NULL, 4096, SBUF_AUTOEXTEND) == NULL) {
		mtx_lock(&sndstat_lock);
		SNDSTAT_PID_SET(i_dev, 0);
		mtx_unlock(&sndstat_lock);
		return ENXIO;
	}
	sndstat_bufptr = 0;
	return 0;
}

static int
sndstat_close(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	if (sndstat_dev == NULL || i_dev != sndstat_dev)
		return EBADF;

	mtx_lock(&sndstat_lock);
	if (SNDSTAT_PID(i_dev) == 0) {
		mtx_unlock(&sndstat_lock);
		return EBADF;
	}

	SNDSTAT_FLUSH();
	SNDSTAT_PID_SET(i_dev, 0);

	mtx_unlock(&sndstat_lock);

	return 0;
}

static int
sndstat_read(struct cdev *i_dev, struct uio *buf, int flag)
{
	int l, err;

	if (sndstat_dev == NULL || i_dev != sndstat_dev)
		return EBADF;

	mtx_lock(&sndstat_lock);
	if (SNDSTAT_PID(i_dev) != buf->uio_td->td_proc->p_pid ||
	    sndstat_bufptr == -1) {
		mtx_unlock(&sndstat_lock);
		return EBADF;
	}
	mtx_unlock(&sndstat_lock);

	if (sndstat_bufptr == 0) {
		err = (sndstat_prepare(&sndstat_sbuf) > 0) ? 0 : ENOMEM;
		if (err) {
			mtx_lock(&sndstat_lock);
			SNDSTAT_FLUSH();
			mtx_unlock(&sndstat_lock);
			return err;
		}
	}

    	l = min(buf->uio_resid, sbuf_len(&sndstat_sbuf) - sndstat_bufptr);
	err = (l > 0)? uiomove(sbuf_data(&sndstat_sbuf) + sndstat_bufptr, l, buf) : 0;
	sndstat_bufptr += l;

	return err;
}

/************************************************************************/

static struct sndstat_entry *
sndstat_find(int type, int unit)
{
	struct sndstat_entry *ent;

	SLIST_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->type == type && ent->unit == unit)
			return ent;
	}

	return NULL;
}

int
sndstat_acquire(struct thread *td)
{
	if (sndstat_dev == NULL)
		return EBADF;

	mtx_lock(&sndstat_lock);
	if (SNDSTAT_PID(sndstat_dev) != 0) {
		mtx_unlock(&sndstat_lock);
		return EBUSY;
	}
	SNDSTAT_PID_SET(sndstat_dev, td->td_proc->p_pid);
	mtx_unlock(&sndstat_lock);
	return 0;
}

int
sndstat_release(struct thread *td)
{
	if (sndstat_dev == NULL)
		return EBADF;

	mtx_lock(&sndstat_lock);
	if (SNDSTAT_PID(sndstat_dev) != td->td_proc->p_pid) {
		mtx_unlock(&sndstat_lock);
		return EBADF;
	}
	SNDSTAT_PID_SET(sndstat_dev, 0);
	mtx_unlock(&sndstat_lock);
	return 0;
}

int
sndstat_register(device_t dev, char *str, sndstat_handler handler)
{
	struct sndstat_entry *ent;
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
			return EINVAL;
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

	mtx_lock(&sndstat_lock);
	SLIST_INSERT_HEAD(&sndstat_devlist, ent, link);
	if (type == SS_TYPE_MODULE)
		sndstat_files++;
	sndstat_maxunit = (unit > sndstat_maxunit)? unit : sndstat_maxunit;
	mtx_unlock(&sndstat_lock);

	return 0;
}

int
sndstat_registerfile(char *str)
{
	return sndstat_register(NULL, str, NULL);
}

int
sndstat_unregister(device_t dev)
{
	struct sndstat_entry *ent;

	mtx_lock(&sndstat_lock);
	SLIST_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->dev == dev) {
			SLIST_REMOVE(&sndstat_devlist, ent, sndstat_entry, link);
			mtx_unlock(&sndstat_lock);
			free(ent, M_DEVBUF);

			return 0;
		}
	}
	mtx_unlock(&sndstat_lock);

	return ENXIO;
}

int
sndstat_unregisterfile(char *str)
{
	struct sndstat_entry *ent;

	mtx_lock(&sndstat_lock);
	SLIST_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->dev == NULL && ent->str == str) {
			SLIST_REMOVE(&sndstat_devlist, ent, sndstat_entry, link);
			sndstat_files--;
			mtx_unlock(&sndstat_lock);
			free(ent, M_DEVBUF);

			return 0;
		}
	}
	mtx_unlock(&sndstat_lock);

	return ENXIO;
}

/************************************************************************/

static int
sndstat_prepare(struct sbuf *s)
{
	struct sndstat_entry *ent;
	struct snddev_info *d;
    	int i, j;

	sbuf_printf(s, "FreeBSD Audio Driver (newpcm: %ubit %d/%s)\n",
	    (u_int)sizeof(intpcm_t) << 3, SND_DRV_VERSION, MACHINE_ARCH);
	if (SLIST_EMPTY(&sndstat_devlist)) {
		sbuf_printf(s, "No devices installed.\n");
		sbuf_finish(s);
    		return sbuf_len(s);
	}

	sbuf_printf(s, "Installed devices:\n");

    	for (i = 0; i <= sndstat_maxunit; i++) {
		for (j = SS_TYPE_FIRST; j <= SS_TYPE_LAST; j++) {
			ent = sndstat_find(j, i);
			if (!ent)
				continue;
			d = device_get_softc(ent->dev);
			if (!PCM_REGISTERED(d))
				continue;
			/* XXX Need Giant magic entry ??? */
			PCM_ACQUIRE_QUICK(d);
			sbuf_printf(s, "%s:", device_get_nameunit(ent->dev));
			sbuf_printf(s, " <%s>", device_get_desc(ent->dev));
			sbuf_printf(s, " %s [%s]", ent->str,
			    (d->flags & SD_F_MPSAFE) ? "MPSAFE" : "GIANT");
			if (ent->handler)
				ent->handler(s, ent->dev, snd_verbose);
			else
				sbuf_printf(s, " [no handler]");
			sbuf_printf(s, "\n");
			PCM_RELEASE_QUICK(d);
		}
    	}

	if (snd_verbose >= 3 && sndstat_files > 0) {
		sbuf_printf(s, "\nFile Versions:\n");

		SLIST_FOREACH(ent, &sndstat_devlist, link) {
			if (ent->dev == NULL && ent->str != NULL)
				sbuf_printf(s, "%s\n", ent->str);
		}
	}

	sbuf_finish(s);
    	return sbuf_len(s);
}

static int
sndstat_init(void)
{
	if (sndstat_dev != NULL)
		return EINVAL;
	mtx_init(&sndstat_lock, "sndstat", "sndstat lock", MTX_DEF);
	sndstat_dev = make_dev(&sndstat_cdevsw, SND_DEV_STATUS,
	    UID_ROOT, GID_WHEEL, 0444, "sndstat");
	return 0;
}

static int
sndstat_uninit(void)
{
	if (sndstat_dev == NULL)
		return EINVAL;

	mtx_lock(&sndstat_lock);
	if (SNDSTAT_PID(sndstat_dev) != curthread->td_proc->p_pid) {
		mtx_unlock(&sndstat_lock);
		return EBUSY;
	}

	SNDSTAT_FLUSH();

	mtx_unlock(&sndstat_lock);

	destroy_dev(sndstat_dev);
	sndstat_dev = NULL;

	mtx_destroy(&sndstat_lock);
	return 0;
}

static void
sndstat_sysinit(void *p)
{
	sndstat_init();
}

static void
sndstat_sysuninit(void *p)
{
	int error;

	error = sndstat_uninit();
	KASSERT(error == 0, ("%s: error = %d", __func__, error));
}

SYSINIT(sndstat_sysinit, SI_SUB_DRIVERS, SI_ORDER_FIRST, sndstat_sysinit, NULL);
SYSUNINIT(sndstat_sysuninit, SI_SUB_DRIVERS, SI_ORDER_FIRST, sndstat_sysuninit, NULL);
