/*-
 * Copyright (c) 1994 Sen Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: pcaudio.c,v 1.14 1998/01/24 02:54:38 eivind Exp $
 */

#include "pca.h"
#if NPCA > 0
#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/filio.h>
#include <sys/poll.h>
#include <sys/vnode.h>

#include <machine/clock.h>
#include <machine/pcaudioio.h>

#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/isa_device.h>
#include <i386/isa/timerreg.h>

#define	DSP_ULAW_NOT_WANTED
#include <i386/isa/sound/ulaw.h>
#define	LINEAR_ALAW_NOT_WANTED
#include <i386/isa/sound/alaw.h>

#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */

#define BUF_SIZE 	8192
#define SAMPLE_RATE	8000
#define INTERRUPT_RATE	16000

static struct pca_status {
	char		open;		/* device open */
	char		queries;	/* did others try opening */
	unsigned char	*buf[3];	/* triple buffering */
	unsigned char   *buffer; 	/* current buffer ptr */
	unsigned	in_use[3];	/* buffers fill */
	unsigned	index;		/* index in current buffer */
	unsigned	counter;	/* sample counter */
	unsigned	scale;		/* sample counter scale */
	unsigned	sample_rate;	/* sample rate */
	unsigned	processed;	/* samples processed */
	unsigned	volume;		/* volume for pc-speaker */
	char		encoding;	/* Ulaw, Alaw or linear */
	char		current;	/* current buffer */
	unsigned char	oldval;		/* old timer port value */
	char		timer_on;	/* is playback running */
	struct selinfo	wsel;		/* select/poll status */
} pca_status;

static char buffer1[BUF_SIZE];
static char buffer2[BUF_SIZE];
static char buffer3[BUF_SIZE];
static char volume_table[256];

#ifdef DEVFS
static	void	*pca_devfs_token;
static	void	*pcac_devfs_token;
#endif

static int pca_sleep = 0;
static int pca_initialized = 0;

static void pcaintr(struct clockframe *frame);
static int pcaprobe(struct isa_device *dvp);
static int pcaattach(struct isa_device *dvp);

struct	isa_driver pcadriver = {
	pcaprobe, pcaattach, "pca",
};

static	d_open_t	pcaopen;
static	d_close_t	pcaclose;
static	d_write_t	pcawrite;
static	d_ioctl_t	pcaioctl;
static	d_poll_t	pcapoll;

#define CDEV_MAJOR 24
static struct cdevsw pca_cdevsw = 
 	{ pcaopen,      pcaclose,       noread,         pcawrite,       /*24*/
 	  pcaioctl,     nostop,         nullreset,      nodevtotty,/* pcaudio */
 	  pcapoll,	nommap,		NULL,	"pca",	NULL,	-1 };

static void pca_continue __P((void));
static void pca_init __P((void));
static void pca_pause __P((void));

static __inline void
conv(const void *table, void *buff, unsigned long n)
{
  __asm__("1:\tmovb (%2), %3\n"
          "\txlatb\n"
          "\tmovb %3, (%2)\n"
	  "\tinc %2\n"
	  "\tdec %1\n"
	  "\tjnz 1b\n"
          :
          :"b" ((long)table), "c" (n), "D" ((long)buff), "a" ((char)n)
          :"bx","cx","di","ax");
}


static void
pca_volume(int volume)
{
	int i, j;

	for (i=0; i<256; i++) {
		j = ((i-128)*volume)/25;
/* XXX
		j = ((i-128)*volume)/100;
*/
		if (j<-128)
			j = -128;
		if (j>127)
			j = 127;
		volume_table[i] = (((255-(j + 128))/4)+1);
	}
}


static void
pca_init(void)
{
	pca_status.open = 0;
	pca_status.queries = 0;
	pca_status.timer_on = 0;
	pca_status.buf[0] = (unsigned char *)&buffer1[0];
	pca_status.buf[1] = (unsigned char *)&buffer2[0];
	pca_status.buf[2] = (unsigned char *)&buffer3[0];
	pca_status.buffer = pca_status.buf[0];
	pca_status.in_use[0] = pca_status.in_use[1] = pca_status.in_use[3] = 0;
	pca_status.current = 0;
	pca_status.sample_rate = SAMPLE_RATE;
	pca_status.scale = (pca_status.sample_rate << 8) / INTERRUPT_RATE;
	pca_status.encoding = AUDIO_ENCODING_ULAW;
	pca_status.volume = 100;

	pca_volume(pca_status.volume);
}


static int
pca_start(void)
{
	int x = splhigh();
	int rv = 0;

	/* use the first buffer */
	pca_status.current  = 0;
	pca_status.index = 0;
	pca_status.counter = 0;
        pca_status.buffer  = pca_status.buf[pca_status.current];
#ifdef PC98
        pca_status.oldval = inb(IO_PPI) & ~0x08;
#else
        pca_status.oldval = inb(IO_PPI) | 0x03;
#endif
        /* acquire the timers */
#ifdef PC98
	if (acquire_timer1(TIMER_LSB|TIMER_ONESHOT))
#else
	if (acquire_timer2(TIMER_LSB|TIMER_ONESHOT))
#endif
		rv = -1;
	else if (acquire_timer0(INTERRUPT_RATE, pcaintr)) {
#ifdef PC98
		release_timer1();
#else
		release_timer2();
#endif
		rv =  -1;
	} else
		pca_status.timer_on = 1;

	splx(x);
	return rv;
}


static void
pca_stop(void)
{
	int x = splhigh();

	/* release the timers */
	release_timer0();
#ifdef PC98
	release_timer1();
#else
	release_timer2();
#endif
	/* reset the buffer */
	pca_status.in_use[0] = pca_status.in_use[1] = pca_status.in_use[2] = 0;
	pca_status.index = 0;
	pca_status.counter = 0;
	pca_status.current = 0;
	pca_status.buffer = pca_status.buf[pca_status.current];
	pca_status.timer_on = 0;
	splx(x);
}


static void
pca_pause(void)
{
	int x = splhigh();

	release_timer0();
#ifdef PC98
	release_timer1();
#else
	release_timer2();
#endif
	pca_status.timer_on = 0;
	splx(x);
}


static void
pca_continue(void)
{
	int x = splhigh();

#ifdef PC98
        pca_status.oldval = inb(IO_PPI) & ~0x08;
	acquire_timer1(TIMER_LSB|TIMER_ONESHOT);
#else
        pca_status.oldval = inb(IO_PPI) | 0x03;
	acquire_timer2(TIMER_LSB|TIMER_ONESHOT);
#endif
	acquire_timer0(INTERRUPT_RATE, pcaintr);
	pca_status.timer_on = 1;
	splx(x);
}


static int
pca_wait(void)
{
	int error, x;

	if (!pca_status.timer_on)
		return 0;

	while (pca_status.in_use[0] || pca_status.in_use[1] ||
	    pca_status.in_use[2]) {
		x = spltty();
		pca_sleep = 1;
		error = tsleep(&pca_sleep, PZERO|PCATCH, "pca_drain", 0);
		pca_sleep = 0;
		splx(x);
		if (error != 0 && error != ERESTART) {
			pca_stop();
			return error;
		}
        }
        return 0;
}


static int
pcaprobe(struct isa_device *dvp)
{
	return(-1);
}


static int
pcaattach(struct isa_device *dvp)
{
	printf("pca%d: PC speaker audio driver\n", dvp->id_unit);
	pca_init();
#ifdef DEVFS
	pca_devfs_token = 
		devfs_add_devswf(&pca_cdevsw, 0, DV_CHR, 0, 0, 0600, "pcaudio");
	pcac_devfs_token = 
		devfs_add_devswf(&pca_cdevsw, 128, DV_CHR, 0, 0, 0600, 
				 "pcaudioctl");
#endif /*DEVFS*/

	return 1;
}


static int
pcaopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	/* audioctl device can always be opened */
	if (minor(dev) == 128)
		return 0;
	if (minor(dev) > 0)
		return ENXIO;

	if (!pca_initialized) {
		pca_init();
		pca_initialized = 1;
	}

	/* audio device can only be open by one process */
	if (pca_status.open) {
		pca_status.queries = 1;
		return EBUSY;
	}
	pca_status.buffer = pca_status.buf[0];
	pca_status.in_use[0] = pca_status.in_use[1] = pca_status.in_use[2] = 0;
	pca_status.timer_on = 0;
	pca_status.open = 1;
	pca_status.processed = 0;
	return 0;
}


static int
pcaclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	/* audioctl device can always be closed */
	if (minor(dev) == 128)
		return 0;
	if (minor(dev) > 0)
		return ENXIO;
	/* audio device close drains all output and restores timers */
	pca_wait();
	pca_stop();
	pca_status.open = 0;
	return 0;
}


static int
pcawrite(dev_t dev, struct uio *uio, int flag)
{
	int count, error, which, x;

	/* only audio device can be written */
	if (minor(dev) > 0)
		return ENXIO;

	while ((count = min(BUF_SIZE, uio->uio_resid)) > 0) {
		if (pca_status.in_use[0] && pca_status.in_use[1] &&
		    pca_status.in_use[2]) {
			if (flag & IO_NDELAY)
				return EWOULDBLOCK;
			x = spltty();
			pca_sleep = 1;
			error = tsleep(&pca_sleep, PZERO|PCATCH, "pca_wait", 0);
			pca_sleep = 0;
			splx(x);
			if (error != 0 && error != ERESTART) {
				pca_stop();
				return error;
			}
		}
		if (!pca_status.in_use[0])
			which = 0;
		else if (!pca_status.in_use[1])
			which = 1;
		else
			which = 2;
		if (count && !pca_status.in_use[which]) {
			uiomove(pca_status.buf[which], count, uio);
			pca_status.processed += count;
			switch (pca_status.encoding) {
			case AUDIO_ENCODING_ULAW:
				conv(ulaw_dsp, pca_status.buf[which], count);
				break;

			case AUDIO_ENCODING_ALAW:
				conv(alaw_linear, pca_status.buf[which], count);
				break;

			case AUDIO_ENCODING_RAW:
				break;
			}
			pca_status.in_use[which] = count;
			if (!pca_status.timer_on)
				if (pca_start())
					return EBUSY;
		}
	}
	return 0;
}


static int
pcaioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	audio_info_t *auptr;

	switch(cmd) {

	case AUDIO_GETINFO:
		auptr = (audio_info_t *)data;
		auptr->play.sample_rate = pca_status.sample_rate;
		auptr->play.channels = 1;
		auptr->play.precision = 8;
		auptr->play.encoding = pca_status.encoding;

		auptr->play.gain = pca_status.volume;
		auptr->play.port = 0;

		auptr->play.samples = pca_status.processed;
		auptr->play.eof = 0;
		auptr->play.pause = !pca_status.timer_on;
		auptr->play.error = 0;
		auptr->play.waiting = pca_status.queries;

		auptr->play.open = pca_status.open;
		auptr->play.active = pca_status.timer_on;
		return 0;

	case AUDIO_SETINFO:
		auptr = (audio_info_t *)data;
		if (auptr->play.sample_rate != (unsigned int)~0) {
			pca_status.sample_rate = auptr->play.sample_rate;
			pca_status.scale =
				(pca_status.sample_rate << 8) / INTERRUPT_RATE;
		}
		if (auptr->play.encoding != (unsigned int)~0) {
			pca_status.encoding = auptr->play.encoding;
		}
		if (auptr->play.gain != (unsigned int)~0) {
			pca_status.volume = auptr->play.gain;
			pca_volume(pca_status.volume);
		}
		if (auptr->play.pause != (unsigned char)~0) {
			if (auptr->play.pause)
				pca_pause();
			else
				pca_continue();
		}

		return 0;

	case AUDIO_DRAIN:
	case AUDIO_COMPAT_DRAIN:
		return pca_wait();

	case AUDIO_FLUSH:
	case AUDIO_COMPAT_FLUSH:
		pca_stop();
		return 0;
	case FIONBIO:
		return 0;
	}
	return ENXIO;
}


static void
pcaintr(struct clockframe *frame)
{
	if (pca_status.index < pca_status.in_use[pca_status.current]) {
		disable_intr();
#ifdef PC98
		__asm__("outb %0,$0x35\n"
			"orb $0x08,%0\n"
			"outb %0,$0x35"
#else
		__asm__("outb %0,$0x61\n"
			"andb $0xFE,%0\n"
			"outb %0,$0x61"
#endif
			: : "a" ((char)pca_status.oldval) );
		__asm__("xlatb\n"
#ifdef PC98
			"outb %0,$0x3fdb"
#else
			"outb %0,$0x42"
#endif
			: : "a" ((char)pca_status.buffer[pca_status.index]),
			    "b" ((long)volume_table) );
		enable_intr();
		pca_status.counter += pca_status.scale;
		pca_status.index = (pca_status.counter >> 8);
	}
	if (pca_status.index >= pca_status.in_use[pca_status.current]) {
		pca_status.index = pca_status.counter = 0;
		pca_status.in_use[pca_status.current] = 0;
		pca_status.current++;
 		if (pca_status.current > 2)
 			pca_status.current = 0;
		pca_status.buffer = pca_status.buf[pca_status.current];
                if (pca_sleep)
			wakeup(&pca_sleep);
		if (pca_status.wsel.si_pid) {
			selwakeup((struct selinfo *)&pca_status.wsel.si_pid);
			pca_status.wsel.si_pid = 0;
			pca_status.wsel.si_flags = 0;
		}
	}
}


static int
pcapoll(dev_t dev, int events, struct proc *p)
{
 	int s;
 	struct proc *p1;
	int revents = 0;

 	s = spltty();

	if (events & (POLLOUT | POLLWRNORM))
 		if (!pca_status.in_use[0] || !pca_status.in_use[1] ||
 		    !pca_status.in_use[2])
 			revents |= events & (POLLOUT | POLLWRNORM);
 		else {
			if (pca_status.wsel.si_pid &&
			    (p1=pfind(pca_status.wsel.si_pid))
			    && p1->p_wchan == (caddr_t)&selwait)
				pca_status.wsel.si_flags = SI_COLL;
			else
				pca_status.wsel.si_pid = p->p_pid;
		}

	splx(s);
	return (revents);
}

static pca_devsw_installed = 0;

static void 	pca_drvinit(void *unused)
{
	dev_t dev;

	if( ! pca_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&pca_cdevsw, NULL);
		pca_devsw_installed = 1;
    	}
}

SYSINIT(pcadev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,pca_drvinit,NULL)


#endif
