/*-
 * Copyright (c) 1994 Søren Schmidt
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
 *	$Id: 
 */

#include "param.h"
#include "uio.h"
#include "ioctl.h"
#include "sound/ulaw.h"
#include "machine/cpufunc.h"
#include "machine/pio.h"
#include "machine/pcaudioio.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/timerreg.h"

#include "pca.h"
#if NPCA > 0

#define BUF_SIZE 	8192
#define SAMPLE_RATE	8000
#define INTERRUPT_RATE	16000

static struct pca_status {
	char		open;		/* device open */
	char		queries;	/* did others try opening */
	unsigned char	*buf[2];	/* double buffering */
	unsigned char   *buffer; 	/* current buffer ptr */
	unsigned	in_use[2];	/* buffers fill */
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
} pca_status;

static char buffer1[BUF_SIZE];
static char buffer2[BUF_SIZE];
static char volume_table[256];

static int pca_sleep = 0;
static int pca_initialized = 0;

void pcaintr(int regs);
int pcaprobe(struct isa_device *dvp);
int pcaattach(struct isa_device *dvp);
int pcaclose(dev_t dev, int flag);
int pcaopen(dev_t dev, int flag);
int pcawrite(dev_t dev, struct uio *uio, int flag);
int pcaioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p);

struct	isa_driver pcadriver = {
	pcaprobe, pcaattach, "pca",
};


inline void conv(const void *table, void *buff, unsigned long n)
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
		j = ((i-128)*volume)/100;
		if (j<-128)
			j = -128;
		if (j>127)
			j = 127;
		volume_table[i] = (((255-(j + 128))/4)+1);
	}
}


static void
pca_init()
{
	pca_status.open = 0;
	pca_status.queries = 0;
	pca_status.timer_on = 0;
	pca_status.buf[0] = (unsigned char *)&buffer1[0];
	pca_status.buf[1] = (unsigned char *)&buffer2[0];
	pca_status.buffer = pca_status.buf[0];
	pca_status.in_use[0] = pca_status.in_use[1] = 0;
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
	/* use the first buffer */
	pca_status.current  = 0;
	pca_status.index = 0;
	pca_status.counter = 0;
        pca_status.buffer  = pca_status.buf[pca_status.current];
        pca_status.oldval = inb(IO_PPI) | 0x03;
        /* acquire the timers */
	if (acquire_timer2(TIMER_LSB|TIMER_ONESHOT)) {
		return -1;
	}
	if (acquire_timer0(INTERRUPT_RATE, pcaintr)) {
		release_timer2();
		return -1;
	}
	pca_status.timer_on = 1;
	return 0;
}


static void 
pca_stop(void)
{
	/* release the timers */
	release_timer0();
	release_timer2();
	/* reset the buffer */
	pca_status.in_use[0] = pca_status.in_use[1] = 0;
	pca_status.index = 0; 
	pca_status.counter = 0; 
	pca_status.current = 0;
	pca_status.buffer = pca_status.buf[pca_status.current];
	pca_status.timer_on = 0;
}


static void 
pca_pause()
{
	release_timer0();
	release_timer2(); 
	pca_status.timer_on = 0;
}


static void 
pca_continue()
{
        pca_status.oldval = inb(IO_PPI) | 0x03;
	acquire_timer2(TIMER_LSB|TIMER_ONESHOT);
	acquire_timer0(INTERRUPT_RATE, pcaintr);
	pca_status.timer_on = 1;
}


static void 
pca_wait(void)
{
	while (pca_status.in_use[0] || pca_status.in_use[1]) {
		pca_sleep = 1;
		tsleep((caddr_t)&pca_sleep, PZERO|PCATCH, "pca_drain", 0);
        }
}


int
pcaprobe(struct isa_device *dvp)
{
	return(-1);
}


int
pcaattach(struct isa_device *dvp)
{
	printf(" PCM audio driver\n", dvp->id_unit);
	pca_init();
	return 1;
}


int
pcaopen(dev_t dev, int flag)
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
	pca_status.in_use[0] = pca_status.in_use[1] = 0;
	pca_status.timer_on = 0;
	pca_status.open = 1;
	pca_status.processed = 0;
	return 0;
}


int
pcaclose(dev_t dev, int flag)
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


int
pcawrite(dev_t dev, struct uio *uio, int flag)
{
	int count, which;

	/* only audio device can be written */
	if (minor(dev) > 0)
		return ENXIO;

	while ((count = min(BUF_SIZE, uio->uio_resid)) > 0) {
		which = pca_status.in_use[0] ? 1 : 0;
		if (count && !pca_status.in_use[which]) {
			uiomove(pca_status.buf[which], count, uio);
			pca_status.processed += count;
			switch (pca_status.encoding) {
			case AUDIO_ENCODING_ULAW:
				conv(ulaw_dsp, pca_status.buf[which], count);
				break;

			case AUDIO_ENCODING_ALAW:
				break;

			case AUDIO_ENCODING_RAW:
				break;
			}
			pca_status.in_use[which] = count;
			if (!pca_status.timer_on)
				if (pca_start()) 
					return EBUSY;
		}
		if (pca_status.in_use[0] && pca_status.in_use[1]) {
			pca_sleep = 1;
			tsleep((caddr_t)&pca_sleep, PZERO|PCATCH, "pca_wait",0);
		}
	} 
	return 0;
}


int
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
		pca_wait();
		return 0;

	case AUDIO_FLUSH:
		pca_stop();
		return 0;

	}
	return ENXIO;
}


void 
pcaintr(int regs)
{
	if (pca_status.index < pca_status.in_use[pca_status.current]) {
#if 1
		disable_intr();
		__asm__("outb %0,$0x61\n"
			"andb $0xFE,%0\n"
			"outb %0,$0x61"
			: : "a" ((char)pca_status.oldval) );
		__asm__("xlatb\n"
			"outb %0,$0x42"
			: : "a" ((char)pca_status.buffer[pca_status.index]),
			    "b" ((long)volume_table) );
		enable_intr();
#else
		disable_intr();
		outb(IO_PPI, pca_status.oldval);
		outb(IO_PPI, pca_status.oldval & 0xFE);
		outb(TIMER_CNTR2, 
			volume_table[pca_status.buffer[pca_status.index]]);
		enable_intr();
#endif
		pca_status.counter += pca_status.scale;
		pca_status.index = (pca_status.counter >> 8);
	}
	else {
		pca_status.index = pca_status.counter = 0;
		pca_status.in_use[pca_status.current] = 0;
		pca_status.current ^= 1;
		pca_status.buffer = pca_status.buf[pca_status.current];
                if (pca_sleep) {
			wakeup((caddr_t)&pca_sleep);
			pca_sleep = 0;
		}
	}
}

#endif
