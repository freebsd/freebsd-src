/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * Copyright (c) 1999 Cameron Grant <cg@FreeBSD.org>
 * Copyright (c) 1995 Hannu Savolainen
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

/*
 * first, include kernel header files.
 */

#ifndef _OS_H_
#define _OS_H_

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/fcntl.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/kernel.h> /* for DATA_SET */
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/limits.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/sbuf.h>
#include <sys/soundcard.h>
#include <sys/sndstat.h>
#include <sys/sysctl.h>
#include <sys/kobj.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#ifndef KOBJMETHOD_END
#define KOBJMETHOD_END	{ NULL, NULL }
#endif

struct pcm_channel;
struct pcm_feeder;
struct snd_dbuf;
struct snd_mixer;

#include <dev/sound/pcm/buffer.h>
#include <dev/sound/pcm/matrix.h>
#include <dev/sound/pcm/channel.h>
#include <dev/sound/pcm/feeder.h>
#include <dev/sound/pcm/mixer.h>
#include <dev/sound/pcm/dsp.h>

#define	PCM_SOFTC_SIZE	(sizeof(struct snddev_info))

#define SND_STATUSLEN	64

#define SOUND_MODVER	5

#define SOUND_MINVER	SOUND_MODVER
#define SOUND_PREFVER	SOUND_MODVER
#define SOUND_MAXVER	SOUND_MODVER

#define SD_F_SIMPLEX		0x00000001
/* unused			0x00000002 */
#define SD_F_SOFTPCMVOL		0x00000004
#define SD_F_BUSY		0x00000008
#define SD_F_MPSAFE		0x00000010
#define SD_F_REGISTERED		0x00000020
#define SD_F_BITPERFECT		0x00000040
#define SD_F_VPC		0x00000080	/* volume-per-channel */
#define SD_F_EQ			0x00000100	/* EQ */
#define SD_F_EQ_ENABLED		0x00000200	/* EQ enabled */
#define SD_F_EQ_BYPASSED	0x00000400	/* EQ bypassed */
#define SD_F_EQ_PC		0x00000800	/* EQ per-channel */
#define SD_F_PVCHANS		0x00001000	/* Playback vchans enabled */
#define SD_F_RVCHANS		0x00002000	/* Recording vchans enabled */

#define SD_F_EQ_DEFAULT		(SD_F_EQ | SD_F_EQ_ENABLED)
#define SD_F_EQ_MASK		(SD_F_EQ | SD_F_EQ_ENABLED |		\
				 SD_F_EQ_BYPASSED | SD_F_EQ_PC)

#define SD_F_BITS		"\020"					\
				"\001SIMPLEX"				\
				/* "\002 */				\
				"\003SOFTPCMVOL"			\
				"\004BUSY"				\
				"\005MPSAFE"				\
				"\006REGISTERED"			\
				"\007BITPERFECT"			\
				"\010VPC"				\
				"\011EQ"				\
				"\012EQ_ENABLED"			\
				"\013EQ_BYPASSED"			\
				"\014EQ_PC"				\
				"\015PVCHANS"				\
				"\016RVCHANS"

#define PCM_ALIVE(x)		((x) != NULL && (x)->lock != NULL)
#define PCM_REGISTERED(x)	(PCM_ALIVE(x) && ((x)->flags & SD_F_REGISTERED))

#define	PCM_MAXCHANS		10000
#define	PCM_CHANCOUNT(d)	\
	(d->playcount + d->pvchancount + d->reccount + d->rvchancount)

/* many variables should be reduced to a range. Here define a macro */
#define RANGE(var, low, high) (var) = \
	(((var)<(low))? (low) : ((var)>(high))? (high) : (var))

#define DSP_DEFAULT_SPEED	8000

extern int snd_unit;
extern int snd_verbose;
extern devclass_t pcm_devclass;
extern struct unrhdr *pcmsg_unrhdr;

#ifndef DEB
#define DEB(x)
#endif

SYSCTL_DECL(_hw_snd);

int pcm_addchan(device_t dev, int dir, kobj_class_t cls, void *devinfo);
unsigned int pcm_getbuffersize(device_t dev, unsigned int minbufsz, unsigned int deflt, unsigned int maxbufsz);
void pcm_init(device_t dev, void *devinfo);
int pcm_register(device_t dev, char *str);
int pcm_unregister(device_t dev);
u_int32_t pcm_getflags(device_t dev);
void pcm_setflags(device_t dev, u_int32_t val);
void *pcm_getdevinfo(device_t dev);

int snd_setup_intr(device_t dev, struct resource *res, int flags,
		   driver_intr_t hand, void *param, void **cookiep);

void *snd_mtxcreate(const char *desc, const char *type);
void snd_mtxfree(void *m);
void snd_mtxassert(void *m);
#define	snd_mtxlock(m) mtx_lock(m)
#define	snd_mtxunlock(m) mtx_unlock(m)

int sndstat_register(device_t dev, char *str);
int sndstat_unregister(device_t dev);

/* These are the function codes assigned to the children of sound cards. */
enum {
	SCF_PCM,
	SCF_MIDI,
	SCF_SYNTH,
};

/*
 * This is the device information struct, used by a bridge device to pass the
 * device function code to the children.
 */
struct sndcard_func {
	int func;	/* The function code. */
	void *varinfo;	/* Bridge-specific information. */
};

/*
 * this is rather kludgey- we need to duplicate these struct def'ns from sound.c
 * so that the macro versions of pcm_{,un}lock can dereference them.
 * we also have to do this now makedev() has gone away.
 */

struct snddev_info {
	struct {
		struct {
			SLIST_HEAD(, pcm_channel) head;
			struct {
				SLIST_HEAD(, pcm_channel) head;
			} busy;
			struct {
				SLIST_HEAD(, pcm_channel) head;
			} opened;
			struct {
				SLIST_HEAD(, pcm_channel) head;
			} primary;
		} pcm;
	} channels;
	unsigned playcount, reccount, pvchancount, rvchancount;
	unsigned flags;
	unsigned int bufsz;
	void *devinfo;
	device_t dev;
	char status[SND_STATUSLEN];
	struct mtx *lock;
	struct cdev *mixer_dev;
	struct cdev *dsp_dev;
	uint32_t pvchanrate, pvchanformat, pvchanmode;
	uint32_t rvchanrate, rvchanformat, rvchanmode;
	int32_t eqpreamp;
	struct sysctl_ctx_list play_sysctl_ctx, rec_sysctl_ctx;
	struct sysctl_oid *play_sysctl_tree, *rec_sysctl_tree;
	struct cv cv;
	struct unrhdr *p_unr;
	struct unrhdr *vp_unr;
	struct unrhdr *r_unr;
	struct unrhdr *vr_unr;
};

void	sound_oss_sysinfo(oss_sysinfo *);
int	sound_oss_card_info(oss_card_info *);

#define	PCM_MODE_MIXER		0x01
#define	PCM_MODE_PLAY		0x02
#define	PCM_MODE_REC		0x04

#define PCM_LOCKOWNED(d)	mtx_owned((d)->lock)
#define	PCM_LOCK(d)		mtx_lock((d)->lock)
#define	PCM_UNLOCK(d)		mtx_unlock((d)->lock)
#define PCM_TRYLOCK(d)		mtx_trylock((d)->lock)
#define PCM_LOCKASSERT(d)	mtx_assert((d)->lock, MA_OWNED)
#define PCM_UNLOCKASSERT(d)	mtx_assert((d)->lock, MA_NOTOWNED)

/*
 * For PCM_[WAIT | ACQUIRE | RELEASE], be sure to surround these
 * with PCM_LOCK/UNLOCK() sequence, or I'll come to gnaw upon you!
 */
#ifdef SND_DIAGNOSTIC
#define PCM_WAIT(x)		do {					\
	if (!PCM_LOCKOWNED(x))						\
		panic("%s(%d): [PCM WAIT] Mutex not owned!",		\
		    __func__, __LINE__);				\
	while ((x)->flags & SD_F_BUSY) {				\
		if (snd_verbose > 3)					\
			device_printf((x)->dev,				\
			    "%s(%d): [PCM WAIT] calling cv_wait().\n",	\
			    __func__, __LINE__);			\
		cv_wait(&(x)->cv, (x)->lock);				\
	}								\
} while (0)

#define PCM_ACQUIRE(x)		do {					\
	if (!PCM_LOCKOWNED(x))						\
		panic("%s(%d): [PCM ACQUIRE] Mutex not owned!",		\
		    __func__, __LINE__);				\
	if ((x)->flags & SD_F_BUSY)					\
		panic("%s(%d): [PCM ACQUIRE] "				\
		    "Trying to acquire BUSY cv!", __func__, __LINE__);	\
	(x)->flags |= SD_F_BUSY;					\
} while (0)

#define PCM_RELEASE(x)		do {					\
	if (!PCM_LOCKOWNED(x))						\
		panic("%s(%d): [PCM RELEASE] Mutex not owned!",		\
		    __func__, __LINE__);				\
	if ((x)->flags & SD_F_BUSY) {					\
		(x)->flags &= ~SD_F_BUSY;				\
		cv_broadcast(&(x)->cv);					\
	} else								\
		panic("%s(%d): [PCM RELEASE] Releasing non-BUSY cv!",	\
		    __func__, __LINE__);				\
} while (0)

/* Quick version, for shorter path. */
#define PCM_ACQUIRE_QUICK(x)	do {					\
	if (PCM_LOCKOWNED(x))						\
		panic("%s(%d): [PCM ACQUIRE QUICK] Mutex owned!",	\
		    __func__, __LINE__);				\
	PCM_LOCK(x);							\
	PCM_WAIT(x);							\
	PCM_ACQUIRE(x);							\
	PCM_UNLOCK(x);							\
} while (0)

#define PCM_RELEASE_QUICK(x)	do {					\
	if (PCM_LOCKOWNED(x))						\
		panic("%s(%d): [PCM RELEASE QUICK] Mutex owned!",	\
		    __func__, __LINE__);				\
	PCM_LOCK(x);							\
	PCM_RELEASE(x);							\
	PCM_UNLOCK(x);							\
} while (0)

#define PCM_BUSYASSERT(x)	do {					\
	if (!((x) != NULL && ((x)->flags & SD_F_BUSY)))			\
		panic("%s(%d): [PCM BUSYASSERT] "			\
		    "Failed, snddev_info=%p", __func__, __LINE__, x);	\
} while (0)

#define PCM_GIANT_ENTER(x)	do {					\
	int _pcm_giant = 0;						\
	if (PCM_LOCKOWNED(x))						\
		panic("%s(%d): [GIANT ENTER] PCM lock owned!",		\
		    __func__, __LINE__);				\
	if (mtx_owned(&Giant) != 0 && snd_verbose > 3)			\
		device_printf((x)->dev,					\
		    "%s(%d): [GIANT ENTER] Giant owned!\n",		\
		    __func__, __LINE__);				\
	if (!((x)->flags & SD_F_MPSAFE) && mtx_owned(&Giant) == 0)	\
		do {							\
			mtx_lock(&Giant);				\
			_pcm_giant = 1;					\
		} while (0)

#define PCM_GIANT_EXIT(x)	do {					\
	if (PCM_LOCKOWNED(x))						\
		panic("%s(%d): [GIANT EXIT] PCM lock owned!",		\
		    __func__, __LINE__);				\
	if (!(_pcm_giant == 0 || _pcm_giant == 1))			\
		panic("%s(%d): [GIANT EXIT] _pcm_giant screwed!",	\
		    __func__, __LINE__);				\
	if ((x)->flags & SD_F_MPSAFE) {					\
		if (_pcm_giant == 1)					\
			panic("%s(%d): [GIANT EXIT] MPSAFE Giant?",	\
			    __func__, __LINE__);			\
		if (mtx_owned(&Giant) != 0 && snd_verbose > 3)		\
			device_printf((x)->dev,				\
			    "%s(%d): [GIANT EXIT] Giant owned!\n",	\
			    __func__, __LINE__);			\
	}								\
	if (_pcm_giant != 0) {						\
		if (mtx_owned(&Giant) == 0)				\
			panic("%s(%d): [GIANT EXIT] Giant not owned!",	\
			    __func__, __LINE__);			\
		_pcm_giant = 0;						\
		mtx_unlock(&Giant);					\
	}								\
} while (0)
#else /* !SND_DIAGNOSTIC */
#define PCM_WAIT(x)		do {					\
	PCM_LOCKASSERT(x);						\
	while ((x)->flags & SD_F_BUSY)					\
		cv_wait(&(x)->cv, (x)->lock);				\
} while (0)

#define PCM_ACQUIRE(x)		do {					\
	PCM_LOCKASSERT(x);						\
	KASSERT(!((x)->flags & SD_F_BUSY),				\
	    ("%s(%d): [PCM ACQUIRE] Trying to acquire BUSY cv!",	\
	    __func__, __LINE__));					\
	(x)->flags |= SD_F_BUSY;					\
} while (0)

#define PCM_RELEASE(x)		do {					\
	PCM_LOCKASSERT(x);						\
	KASSERT((x)->flags & SD_F_BUSY,					\
	    ("%s(%d): [PCM RELEASE] Releasing non-BUSY cv!",		\
	    __func__, __LINE__));					\
	(x)->flags &= ~SD_F_BUSY;					\
	cv_broadcast(&(x)->cv);						\
} while (0)

/* Quick version, for shorter path. */
#define PCM_ACQUIRE_QUICK(x)	do {					\
	PCM_UNLOCKASSERT(x);						\
	PCM_LOCK(x);							\
	PCM_WAIT(x);							\
	PCM_ACQUIRE(x);							\
	PCM_UNLOCK(x);							\
} while (0)

#define PCM_RELEASE_QUICK(x)	do {					\
	PCM_UNLOCKASSERT(x);						\
	PCM_LOCK(x);							\
	PCM_RELEASE(x);							\
	PCM_UNLOCK(x);							\
} while (0)

#define PCM_BUSYASSERT(x)	KASSERT(x != NULL &&			\
				    ((x)->flags & SD_F_BUSY),		\
				    ("%s(%d): [PCM BUSYASSERT] "	\
				    "Failed, snddev_info=%p",		\
				    __func__, __LINE__, x))

#define PCM_GIANT_ENTER(x)	do {					\
	int _pcm_giant = 0;						\
	PCM_UNLOCKASSERT(x);						\
	if (!((x)->flags & SD_F_MPSAFE) && mtx_owned(&Giant) == 0)	\
		do {							\
			mtx_lock(&Giant);				\
			_pcm_giant = 1;					\
		} while (0)

#define PCM_GIANT_EXIT(x)	do {					\
	PCM_UNLOCKASSERT(x);						\
	KASSERT(_pcm_giant == 0 || _pcm_giant == 1,			\
	    ("%s(%d): [GIANT EXIT] _pcm_giant screwed!",		\
	    __func__, __LINE__));					\
	KASSERT(!((x)->flags & SD_F_MPSAFE) ||				\
	    (((x)->flags & SD_F_MPSAFE) && _pcm_giant == 0),		\
	    ("%s(%d): [GIANT EXIT] MPSAFE Giant?",			\
	    __func__, __LINE__));					\
	if (_pcm_giant != 0) {						\
		mtx_assert(&Giant, MA_OWNED);				\
		_pcm_giant = 0;						\
		mtx_unlock(&Giant);					\
	}								\
} while (0)
#endif /* SND_DIAGNOSTIC */

#define PCM_GIANT_LEAVE(x)						\
	PCM_GIANT_EXIT(x);						\
} while (0)

#endif /* _KERNEL */

/* make figuring out what a format is easier. got AFMT_STEREO already */
#define AFMT_32BIT (AFMT_S32_LE | AFMT_S32_BE | AFMT_U32_LE | AFMT_U32_BE | \
			AFMT_F32_LE | AFMT_F32_BE)
#define AFMT_24BIT (AFMT_S24_LE | AFMT_S24_BE | AFMT_U24_LE | AFMT_U24_BE)
#define AFMT_16BIT (AFMT_S16_LE | AFMT_S16_BE | AFMT_U16_LE | AFMT_U16_BE)
#define AFMT_G711  (AFMT_MU_LAW | AFMT_A_LAW)
#define AFMT_8BIT (AFMT_G711 | AFMT_U8 | AFMT_S8)
#define AFMT_SIGNED (AFMT_S32_LE | AFMT_S32_BE | AFMT_F32_LE | AFMT_F32_BE | \
			AFMT_S24_LE | AFMT_S24_BE | \
			AFMT_S16_LE | AFMT_S16_BE | AFMT_S8)
#define AFMT_BIGENDIAN (AFMT_S32_BE | AFMT_U32_BE | AFMT_F32_BE | \
			AFMT_S24_BE | AFMT_U24_BE | AFMT_S16_BE | AFMT_U16_BE)

#define AFMT_CONVERTIBLE	(AFMT_8BIT | AFMT_16BIT | AFMT_24BIT |	\
				 AFMT_32BIT)

/* Supported vchan mixing formats */
#define AFMT_VCHAN		(AFMT_CONVERTIBLE & ~AFMT_G711)

#define AFMT_PASSTHROUGH		AFMT_AC3
#define AFMT_PASSTHROUGH_RATE		48000
#define AFMT_PASSTHROUGH_CHANNEL	2
#define AFMT_PASSTHROUGH_EXTCHANNEL	0

/*
 * We're simply using unused, contiguous bits from various AFMT_ definitions.
 * ~(0xb00ff7ff)
 */
#define AFMT_ENCODING_MASK	0xf00fffff
#define AFMT_CHANNEL_MASK	0x07f00000
#define AFMT_CHANNEL_SHIFT	20
#define AFMT_CHANNEL_MAX	0x7f
#define AFMT_EXTCHANNEL_MASK	0x08000000
#define AFMT_EXTCHANNEL_SHIFT	27
#define AFMT_EXTCHANNEL_MAX	1

#define AFMT_ENCODING(v)	((v) & AFMT_ENCODING_MASK)

#define AFMT_EXTCHANNEL(v)	(((v) & AFMT_EXTCHANNEL_MASK) >>	\
				AFMT_EXTCHANNEL_SHIFT)

#define AFMT_CHANNEL(v)		(((v) & AFMT_CHANNEL_MASK) >>		\
				AFMT_CHANNEL_SHIFT)

#define AFMT_BIT(v)		(((v) & AFMT_32BIT) ? 32 :		\
				(((v) & AFMT_24BIT) ? 24 :		\
				((((v) & AFMT_16BIT) ||			\
				((v) & AFMT_PASSTHROUGH)) ? 16 : 8)))

#define AFMT_BPS(v)		(AFMT_BIT(v) >> 3)
#define AFMT_ALIGN(v)		(AFMT_BPS(v) * AFMT_CHANNEL(v))

#define SND_FORMAT(f, c, e)	(AFMT_ENCODING(f) |		\
				(((c) << AFMT_CHANNEL_SHIFT) &		\
				AFMT_CHANNEL_MASK) |			\
				(((e) << AFMT_EXTCHANNEL_SHIFT) &	\
				AFMT_EXTCHANNEL_MASK))

#define AFMT_U8_NE	AFMT_U8
#define AFMT_S8_NE	AFMT_S8

#define AFMT_SIGNED_NE	(AFMT_S8_NE | AFMT_S16_NE | AFMT_S24_NE | \
			AFMT_S32_NE | AFMT_F32_NE)

#define AFMT_NE		(AFMT_SIGNED_NE | AFMT_U8_NE | AFMT_U16_NE |	\
			 AFMT_U24_NE | AFMT_U32_NE)

#endif	/* _OS_H_ */
