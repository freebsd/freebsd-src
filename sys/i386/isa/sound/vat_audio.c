#include "vat_audio.h"
#if NVAT_AUDIO > 0

#include "sound_config.h"
#include "os.h"
#include "vat_audioio.h"


#define	PAS_AUDIO	(SND_DEV_AUDIO | 0x10)
#define	SB_AUDIO	(SND_DEV_AUDIO )
#define	MIXER		(SND_DEV_CTL)

struct va_softc	{
	dev_t	rdev;		/* record device */
	dev_t	pdev;		/* playback device */
	dev_t	mixer;		/* mixer device */
	struct	selinfo wsel;	/* write select info */
	struct	selinfo	rsel;	/* read select info */
	int	rlevel;
	int	plevel;
	int	open;
} va_softc = { PAS_AUDIO, SB_AUDIO, MIXER, {0, 0}, {0, 0}, 0, 0, 0 } ;

#define	DEF_SAMPLE_RATE	8007

#ifndef AUDIOBLOCKSIZE
#define	AUDIOBLOCKSIZE	160	/* 20ms at 8khz */
#endif
static int	iblocksize=AUDIOBLOCKSIZE;
static int	oblocksize=1024;

#define	splaudio	splclock

static void
setpgain(int level)
{
register struct va_softc *va = (struct va_softc *)&va_softc;
int	arg;

	level = (level*100/255) & 0x7f;
	arg = (level << 8) | level;
	sndioctl(va->mixer, MIXER_WRITE(SOUND_MIXER_PCM), &arg);
}

static void
setrgain(int level)
{
register struct va_softc *va = (struct va_softc *)&va_softc;
int	arg;
int	arg1;

	level = (level*100/255) & 0x7f;
	arg = (level << 8) | level;

	sndioctl(va->mixer, SOUND_MIXER_WRITE_LINE, &arg);
	sndioctl(va->mixer, SOUND_MIXER_WRITE_MIC, &arg);
	sndioctl(va->mixer, SOUND_MIXER_WRITE_CD, &arg);
}

static int
setprate(int rate)
{
register struct va_softc *va = (struct va_softc *)&va_softc;
register int dev;

	dev = va->pdev >> 4;
	return(audio_devs[dev]->ioctl(dev, SOUND_PCM_WRITE_RATE, rate, 1));
}

static int
setrrate(int rate)
{
register struct va_softc *va = (struct va_softc *)&va_softc;
register int dev;

	dev = va->rdev >> 4;
	return(audio_devs[dev]->ioctl(dev, SOUND_PCM_WRITE_RATE, rate, 1));

}

static int default_level[SOUND_MIXER_NRDEVICES] = { /* max = 0x64 */
	0x3232,		/* Master Volume */
	0x3232,		/* Bass */
	0x3232,		/* Treble */
	0x0,		/* FM */
	0x6464,		/* PCM */
	0x0,		/* PC Speaker */
	0x6464,		/* Ext Line */
	0x6464,		/* Mic */
	0x4b4b,		/* CD */
	0x0,		/* Recording monitor (input mixer)  -- avoid feedback */
	0x4b4b,		/* SB PCM */
	0x6464,		/* Record Level -- to ADC */
};

static u_int record_devices = (SOUND_MASK_MIC|SOUND_MASK_LINE|SOUND_MASK_CD);

int
vaopen(dev_t dev, int flags)
{
register struct va_softc *va = (struct va_softc *)&va_softc;
int	s;

	if(va->open)
		return(EBUSY);
	else
		va->open = 1 ;

	s=sndopen(va->rdev, FREAD);
	if(s) {
		va->open = 0;
		return(s);
	}
	s=sndopen(va->pdev, FWRITE);
	if(s) {
		va->open = 0;
		sndclose(va->rdev, FREAD);
		return(s);
	}

	/* set sample rates */
	setprate(DEF_SAMPLE_RATE);
	setrrate(DEF_SAMPLE_RATE);

	/* set block size for I/O samples */
	sndioctl(va->rdev, SNDCTL_DSP_GETBLKSIZE, &iblocksize);
	sndioctl(va->pdev, SNDCTL_DSP_GETBLKSIZE, &oblocksize);

	/* initialize mixer controls the way we want them */
	sndioctl(va->mixer, SOUND_MIXER_WRITE_RECSRC, &record_devices);
	for(s=0; s< SOUND_MIXER_NRDEVICES; s++)
		sndioctl(va->mixer, MIXER_WRITE(s), &default_level[s]);
	va->rlevel=(default_level[SOUND_MASK_MIC]&0x7f) * 255 / 100;
	va->plevel=(default_level[SOUND_MASK_PCM]&0x7f) * 255 / 100;

	if((flags & FREAD) != 0) {	/* start the read process */
		DMAbuf_start_input(va->rdev>>4);
	}
	
	return(0);
}

int
vaclose(dev_t dev, int flags)
{
register struct va_softc *va = (struct va_softc *)&va_softc;

	va->open = 0;
	sndioctl(va->mixer, SNDCTL_DSP_RESET, 0);
	sndclose(va->pdev, FWRITE);
	sndclose(va->rdev, FREAD);
	return(0);
}

int
varead(dev_t dev, struct uio *buf, int ioflag)
{
register struct va_softc *va = (struct va_softc *)&va_softc;

	return(sndread(va->rdev, buf, ioflag));
}

int
vawrite(dev_t dev, struct uio *buf, int ioflag)
{
register struct va_softc *va = (struct va_softc *)&va_softc;

	return(sndwrite(va->pdev, buf, ioflag));
}

void
audio_get_info(struct va_softc *va, struct audio_info *ai)
{
struct	audio_prinfo	*r, *p;
int	rdev = va->rdev>>4;
int	pdev = va->pdev>>4;

	r = &ai->record;
	p = &ai->play;

	p->sample_rate = audio_devs[pdev]->ioctl(pdev, SOUND_PCM_READ_RATE, 0,1);
	r->sample_rate = audio_devs[rdev]->ioctl(rdev, SOUND_PCM_READ_RATE, 0,1);
	p->channels = audio_devs[pdev]->ioctl(pdev, SOUND_PCM_READ_CHANNELS, 0,1);
	r->channels = audio_devs[rdev]->ioctl(rdev, SOUND_PCM_READ_CHANNELS, 0,1);
	p->precision = audio_devs[pdev]->ioctl(pdev, SOUND_PCM_READ_BITS, 0, 1);
	r->precision = audio_devs[rdev]->ioctl(rdev, SOUND_PCM_READ_BITS, 0, 1);
	p->encoding = r->encoding = AUDIO_ENCODING_ULAW;

	ai->monitor_gain = 0;

	r->gain = va->rlevel;
	p->gain = va->plevel;

	r->port = 1;
	p->port = AUDIO_SPEAKER;

	p->open = r->open = va->open;

	return;

}

void
audio_set_info(struct va_softc *va, struct audio_info *ai)
{
struct	audio_prinfo	*r, *p;
int	rdev = va->rdev>>4;
int	pdev = va->pdev>>4;

	r = &ai->record;
	p = &ai->play;

	 /* Only set gains if mode == -1, I think this is a bug in vat. */
	if(ai->mode == ~0) {
		if(p->gain != ~0) {
			va->plevel = p->gain;
			setpgain(va->plevel);
		}
		if(r->gain != ~0) {
			va->rlevel = r->gain;
			setrgain(va->rlevel);
		}
	}
	if(p->sample_rate != ~0)
		p->sample_rate = setprate(p->sample_rate);

	if(r->sample_rate != ~0)
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
		if(s == 0)
			s = sndioctl(va->pdev, cmd, arg, mode);
		break;
	}
	return(0);
}

int
vaselect(dev_t dev, int rw, struct proc *p)
{
register struct va_softc *va = (struct va_softc *)&va_softc;
int			s;
int	r;

	s = splaudio();
	r = 0;
	switch(rw) {
	case FREAD:
		if(DMAbuf_input_ready(va->rdev>>4))
			r = 1;
		else
			selrecord(p, &va->rsel);
		break;
	case FWRITE:
		if(DMAbuf_output_ready(va->pdev>>4))
			r = 1;
		else
			selrecord(p, &va->wsel);

		break;
	}
	splx(s);
	return(r);
}

void
audio_rint()
{
register struct va_softc *va = (struct va_softc *)&va_softc;
register	u_char	*tp;
register	int	cc;

	if(!va->open) return;
	selwakeup(&va->rsel);
}

void
audio_pint()
{
register struct va_softc *va = (struct va_softc *)&va_softc;
register	u_char	*tp;
register	int	cc;

	if(!va->open) return;
	selwakeup(&va->wsel);
}
#else
void
audio_rint()
{
}
void
audio_pint()
{
}
#endif /* NVAT_AUDIO */
