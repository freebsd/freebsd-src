/*
 * Minimally compliant SunOS compatible audio driver front-end
 * for use with VAT.
 *
 * This is a front end for the voxware based drivers that form the standard
 * audio driver system for FreeBSD.  It will not operate without the voxware
 * package.
 *
 * This is not a full implementation of the SunOS audio driver, don't
 * expect anything other than vat to operate with it.
 *
 * ---WARNING
 * ---WARNING this work is not complete, it still doesn't work
 * ---WARNING
 *
 * Copyright (C) 1993-1994 Amancio Hasty.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.
 *
 * This software is provided `as-is'.  The author disclaims all
 * warranties with regard to this software, including without
 * limitation all implied warranties of merchantability, fitness for
 * a particular purpose, or noninfringement.  In no event shall the
 * author be liable for any damages whatsoever, including
 * special, incidental or consequential damages, including loss of
 * use, data, or profits, even if advised of the possibility thereof,
 * and regardless of whether in an action in contract, tort or
 * negligence, arising out of or in connection with the use or
 * performance of this software.
 *
 */

#include "vat_audio.h"
#include "snd.h"                 /* Generic Sound Driver (voxware) */

#if (NVAT_AUDIO > 0) && (NSND > 0)

#include "sound_config.h"
#include "os.h"
#include "vat_audioio.h"

#define	splaudio	splclock

extern int sndopen  (dev_t dev, int flags);
extern int sndclose (dev_t dev, int flags); 
extern int sndioctl (dev_t dev, int cmd, void *arg, int mode);
extern int sndread  (int dev, struct uio *uio); 
extern int sndwrite (int dev, struct uio *uio);

struct va_softc	{
	dev_t	rdev;		/* record device */
	dev_t	pdev;		/* playback device */
	dev_t	mixer;		/* mixer device */
	struct	selinfo wsel;	/* write select info */
	struct	selinfo	rsel;	/* read select info */
	int	rlevel;
	int	plevel;
	int	open;
} va_softc;

#define	DEF_SAMPLE_RATE	8007

#ifndef AUDIOBLOCKSIZE
#define	AUDIOBLOCKSIZE	160	/* 20ms at 8khz */
#endif

static int iblocksize = AUDIOBLOCKSIZE;
static int oblocksize = 1024;

static u_int record_devices = (SOUND_MASK_MIC|SOUND_MASK_LINE|SOUND_MASK_CD);

static int default_level[SOUND_MIXER_NRDEVICES] = { /* max = 0x64 */
	0x3232,		/* Master Volume */
	0x3232,		/* Bass */
	0x3232,		/* Treble */
	0x0000,		/* FM */
	0x6464,		/* PCM */
	0x0000,		/* PC Speaker */
	0x6464,		/* Ext Line */
	0x6464,		/* Mic */
	0x4b4b,		/* CD */
	0x0000,		/* Recording monitor (input mixer)  -- avoid feedback */
	0x4b4b,		/* SB PCM */
	0x6464,		/* Record Level -- to ADC */
};


static void
setpgain(int level)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;
	int	arg;

	level = (level * 100 / 255) & 0x7f;
	arg   = (level << 8) | level;

	sndioctl(va->mixer, MIXER_WRITE(SOUND_MIXER_PCM), &arg, 0);
}

static void
setrgain(int level)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;
	int	arg, arg1;

	level = (level * 100 / 255) & 0x7f;
	arg   = (level << 8) | level;

	sndioctl(va->mixer, SOUND_MIXER_WRITE_LINE, &arg, 0);
	sndioctl(va->mixer, SOUND_MIXER_WRITE_MIC,  &arg, 0);
	sndioctl(va->mixer, SOUND_MIXER_WRITE_CD,   &arg, 0);
}

static int
setprate(int rate)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;
	register int dev;

	dev = va->pdev >> 4;
	return (audio_devs[dev]->ioctl(dev, SOUND_PCM_WRITE_RATE, rate, 1));
}

static int
setrrate(int rate)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;
	register int dev;

	dev = va->rdev >> 4;
	return (audio_devs[dev]->ioctl(dev, SOUND_PCM_WRITE_RATE, rate, 1));
}

int
vaopen(dev_t dev, int flags)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;
	int	s;

	if (va->open)
		return(EBUSY);
	else
		va->open = 1;

#ifdef	SND_BIDIR
	va->rdev  = SND_DEV_AUDIO | (1<<4);	/* first and second device */
	va->pdev  = SND_DEV_AUDIO | (0<<4);
	va->mixer = SND_DEV_CTL;

	s = sndopen(va->rdev, FREAD);
	if (s) {
		va->open = 0;
		return(s);
	}

	s = sndopen(va->pdev, FWRITE);
	if (s) {
		va->open = 0;
		sndclose(va->rdev, FREAD);
		return(s);
	}
#else
	va->rdev  = SND_DEV_AUDIO | (0<<4);	/* first attached device */
	va->pdev  = SND_DEV_AUDIO | (0<<4);
	va->mixer = SND_DEV_CTL;

	s = sndopen(va->rdev, flags);
	if (s) {
		va->open = 0;
		return(s);
	}
#endif

	/* set sample rates */
	setprate(DEF_SAMPLE_RATE);
	setrrate(DEF_SAMPLE_RATE);

	/* set block size for I/O samples */
	sndioctl(va->rdev, SNDCTL_DSP_GETBLKSIZE, &iblocksize, 0);
	sndioctl(va->pdev, SNDCTL_DSP_GETBLKSIZE, &oblocksize, 0);

	/* initialize mixer controls the way we want them */
	sndioctl(va->mixer, SOUND_MIXER_WRITE_RECSRC, &record_devices, 0);

	for (s = 0; s < SOUND_MIXER_NRDEVICES; s++)
		sndioctl(va->mixer, MIXER_WRITE(s), &default_level[s], 0);

	va->rlevel = (default_level[SOUND_MASK_MIC] & 0x7f) * 255 / 100;
	va->plevel = (default_level[SOUND_MASK_PCM] & 0x7f) * 255 / 100;

	if (flags & FREAD)		/* start the read process */
		DMAbuf_start_input(va->rdev>>4);
	
	return(0);
}

int
vaclose(dev_t dev, int flags)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;

	va->open = 0;

	sndioctl(va->mixer, SNDCTL_DSP_RESET, NULL, 0);

#ifdef	SND_BIDIR
	sndclose(va->pdev, FWRITE);
	sndclose(va->rdev, FREAD);
#else
	sndclose(va->rdev, flags);
#endif
	return (0);
}

int
varead(dev_t dev, struct uio *buf)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;

	return sndread(va->rdev, buf);
}

int
vawrite(dev_t dev, struct uio *buf)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;

	return sndwrite(va->pdev, buf);
}

void
audio_get_info(struct va_softc *va, struct audio_info *ai)
{
	struct	audio_prinfo	*r, *p;
	int	rdev = va->rdev >> 4;
	int	pdev = va->pdev >> 4;

	r = &ai->record;
	p = &ai->play;

	p->sample_rate =
		audio_devs[pdev]->ioctl(pdev, SOUND_PCM_READ_RATE, 0, 1);
	r->sample_rate =
		audio_devs[rdev]->ioctl(rdev, SOUND_PCM_READ_RATE, 0, 1);

	p->channels =
		audio_devs[pdev]->ioctl(pdev, SOUND_PCM_READ_CHANNELS, 0, 1);
	r->channels =
		audio_devs[rdev]->ioctl(rdev, SOUND_PCM_READ_CHANNELS, 0, 1);

	p->precision = audio_devs[pdev]->ioctl(pdev, SOUND_PCM_READ_BITS, 0, 1);
	r->precision = audio_devs[rdev]->ioctl(rdev, SOUND_PCM_READ_BITS, 0, 1);

	p->encoding = r->encoding = AUDIO_ENCODING_ULAW;

	ai->monitor_gain = 0;

	r->gain = va->rlevel;
	p->gain = va->plevel;

	r->port = 1;
	p->port = AUDIO_SPEAKER;

	p->open = r->open = va->open;
}

void
audio_set_info(struct va_softc *va, struct audio_info *ai)
{
	struct	audio_prinfo	*r, *p;
	int	rdev = va->rdev >> 4;
	int	pdev = va->pdev >> 4;

	r = &ai->record;
	p = &ai->play;

	/* Only set gains if mode == -1, I think this is a bug in vat. */

	if (ai->mode == ~0) {
		if (p->gain != ~0) {
			va->plevel = p->gain;
			setpgain(va->plevel);
		}
		if (r->gain != ~0) {
			va->rlevel = r->gain;
			setrgain(va->rlevel);
		}
	}

	if (p->sample_rate != ~0)
		p->sample_rate = setprate(p->sample_rate);

	if (r->sample_rate != ~0)
		r->sample_rate = setrrate(r->sample_rate);

	DMAbuf_start_input(rdev);
}

int
vaioctl(dev_t dev, int cmd, caddr_t arg, int mode)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;
	int	s;

	switch(cmd) {
	case FIONBIO:
		break;	/* handled above in file i/o routines */
	case AUDIO_GETINFO:
		audio_get_info(va, (struct audio_info *)arg);
		break;
	case AUDIO_SETINFO:
		audio_set_info(va, (struct audio_info *)arg);
		break;
	default:
		printf("vaioctl: cmd=0x%x, '%c', num = %d, len=%d, %s\n",
			cmd, IOCGROUP(cmd), cmd & 0xff, IOCPARM_LEN(cmd),
			cmd&IOC_IN ? "in" : "out");

		s = sndioctl(va->rdev, cmd, arg, mode);

		if (s == 0)
			s = sndioctl(va->pdev, cmd, arg, mode);
		break;
	}
	return(0);
}

int
vaselect(dev_t dev, int rw, struct proc *p)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;
	int	s, r;

	r = 0;
	s = splaudio();

	switch (rw) {
	case FREAD:
		if (DMAbuf_input_ready(va->rdev>>4))
			r = 1;
		else
			selrecord(p, &va->rsel);
		break;
	case FWRITE:
		if (DMAbuf_output_ready(va->pdev>>4))
			r = 1;
		else
			selrecord(p, &va->wsel);
		break;
	}

	splx(s);
	return(r);
}

void
audio_rint(void)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;

	if (!va->open)
		return;

	selwakeup(&va->rsel);
}

void
audio_pint(void)
{
	register struct va_softc *va = (struct va_softc *)&va_softc;

	if (!va->open)
		return;

	selwakeup(&va->wsel);
}
#else

void audio_rint(void) {}
void audio_pint(void) {}

#endif /* NVAT_AUDIO && NSND */
