/*-
 * Copyright (c) 1994-1998 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/filio.h>
#include <sys/poll.h>
#include <sys/vnode.h>

#include <machine/clock.h>
#include <machine/pcaudioio.h>

#include <isa/isareg.h>
#include <isa/isavar.h>
#include <i386/isa/timerreg.h>

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
	u_char		current;	/* current buffer */
	unsigned char	oldval;		/* old timer port value */
	char		timer_on;	/* is playback running */
	struct selinfo	wsel;		/* select/poll status */
} pca_status;

static char buffer1[BUF_SIZE];
static char buffer2[BUF_SIZE];
static char buffer3[BUF_SIZE];
static char volume_table[256];

static unsigned char ulaw_dsp[] = {
     3,    7,   11,   15,   19,   23,   27,   31,
    35,   39,   43,   47,   51,   55,   59,   63,
    66,   68,   70,   72,   74,   76,   78,   80,
    82,   84,   86,   88,   90,   92,   94,   96,
    98,   99,  100,  101,  102,  103,  104,  105,
   106,  107,  108,  109,  110,  111,  112,  113,
   113,  114,  114,  115,  115,  116,  116,  117,
   117,  118,  118,  119,  119,  120,  120,  121,
   121,  121,  122,  122,  122,  122,  123,  123,
   123,  123,  124,  124,  124,  124,  125,  125,
   125,  125,  125,  125,  126,  126,  126,  126,
   126,  126,  126,  126,  127,  127,  127,  127,
   127,  127,  127,  127,  127,  127,  127,  127,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
   253,  249,  245,  241,  237,  233,  229,  225,
   221,  217,  213,  209,  205,  201,  197,  193,
   190,  188,  186,  184,  182,  180,  178,  176,
   174,  172,  170,  168,  166,  164,  162,  160,
   158,  157,  156,  155,  154,  153,  152,  151,
   150,  149,  148,  147,  146,  145,  144,  143,
   143,  142,  142,  141,  141,  140,  140,  139,
   139,  138,  138,  137,  137,  136,  136,  135,
   135,  135,  134,  134,  134,  134,  133,  133,
   133,  133,  132,  132,  132,  132,  131,  131,
   131,  131,  131,  131,  130,  130,  130,  130,
   130,  130,  130,  130,  129,  129,  129,  129,
   129,  129,  129,  129,  129,  129,  129,  129,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
};

static unsigned char alaw_linear[] = {
	45, 	214, 	122, 	133, 	0, 		255, 	107, 	149, 
	86, 	171, 	126, 	129, 	0, 		255, 	117, 	138, 
	13, 	246, 	120, 	135, 	0, 		255, 	99, 	157, 
	70, 	187, 	124, 	131, 	0, 		255, 	113, 	142, 
	61, 	198, 	123, 	132,  	0, 		255, 	111, 	145, 
	94, 	163, 	127, 	128, 	0, 		255, 	119, 	136, 
	29, 	230, 	121, 	134, 	0, 		255, 	103, 	153, 
	78, 	179, 	125, 	130, 	0, 		255, 	115, 	140, 
	37, 	222, 	122, 	133, 	0, 		255, 	105, 	151, 
	82, 	175, 	126, 	129, 	0, 		255, 	116, 	139, 
	5, 	254, 	120, 	135, 	0, 		255, 	97, 	159, 
	66, 	191, 	124, 	131, 	0, 		255, 	112,	143, 
	53, 	206, 	123, 	132, 	0, 		255, 	109, 	147, 
	90, 	167, 	127, 	128, 	0, 		255,	118, 	137, 
	21, 	238, 	121, 	134, 	0, 		255, 	101,	155, 
	74, 	183, 	125, 	130, 	0, 		255, 	114, 	141, 
	49, 	210, 	123, 	133, 	0, 		255, 	108, 	148, 
	88, 	169, 	127, 	129, 	0, 		255, 	118, 	138, 
	17, 	242, 	121, 	135, 	0, 		255, 	100, 	156, 
	72, 	185, 	125, 	131, 	0, 		255, 	114, 	142, 
	64, 	194, 	124, 	132, 	0, 		255, 	112, 	144, 
	96, 	161, 	128, 	128, 	1, 		255, 	120, 	136, 
	33, 	226, 	122, 	134, 	0, 		255, 	104, 	152, 
	80, 	177, 	126, 	130, 	0, 		255, 	116, 	140, 
	41, 	218, 	122, 	133, 	0, 		255, 	106, 	150, 
	84, 	173, 	126, 	129, 	0, 		255, 	117, 	139, 
	9, 	250, 	120, 	135, 	0, 		255, 	98, 	158, 
	68, 	189, 	124, 	131, 	0, 		255, 	113, 	143, 
	57, 	202, 	123, 	132, 	0, 		255, 	110, 	146, 
	92, 	165, 	127, 	128, 	0, 		255, 	119, 	137, 
	25, 	234, 	121, 	134, 	0, 		255, 	102, 	154, 
	76, 	181, 	125, 	130, 	0, 		255, 	115, 	141, 
};

static int pca_sleep = 0;

static void pcaintr(struct clockframe *frame);

static	d_open_t	pcaopen;
static	d_close_t	pcaclose;
static	d_write_t	pcawrite;
static	d_ioctl_t	pcaioctl;
static	d_poll_t	pcapoll;

#define CDEV_MAJOR 24
static struct cdevsw pca_cdevsw = {
	.d_open =	pcaopen,
	.d_close =	pcaclose,
	.d_write =	pcawrite,
	.d_ioctl =	pcaioctl,
	.d_poll =	pcapoll,
	.d_name =	"pca",
	.d_maj =	CDEV_MAJOR,
};

static void pca_continue(void);
static void pca_init(void);
static void pca_pause(void);

static void
conv(const unsigned char *table, unsigned char *buff, unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++)
		buff[i] = table[buff[i]];
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
	pca_status.in_use[0] = pca_status.in_use[1] = pca_status.in_use[2] = 0;
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
        pca_status.oldval = inb(IO_PPI) | 0x03;
        /* acquire the timers */
	if (acquire_timer2(TIMER_LSB|TIMER_ONESHOT))
		rv = -1;
	else if (acquire_timer0(INTERRUPT_RATE, pcaintr)) {
		release_timer2();
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
	release_timer2();
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
	release_timer2();
	pca_status.timer_on = 0;
	splx(x);
}


static void
pca_continue(void)
{
	int x = splhigh();

        pca_status.oldval = inb(IO_PPI) | 0x03;
	acquire_timer2(TIMER_LSB|TIMER_ONESHOT);
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


static struct isa_pnp_id pca_ids[] = {
	{0x0008d041, "AT-style speaker sound"},	/* PNP0800 */
	{0}
};

static int
pcaprobe(device_t dev)
{
	/* Check isapnp ids */
	return ISA_PNP_PROBE(device_get_parent(dev), dev, pca_ids);
}


static int
pcaattach(device_t dev)
{
	pca_init();
	make_dev(&pca_cdevsw, 0, 0, 0, 0600, "pcaudio");
	make_dev(&pca_cdevsw, 128, 0, 0, 0600, "pcaudioctl");
	return 0;
}

static device_method_t pca_methods[] = {
	DEVMETHOD(device_probe,		pcaprobe),
	DEVMETHOD(device_attach,	pcaattach),
	{ 0, 0 }
};

static driver_t pca_driver = {
	"pca",
	pca_methods,
	1
};

static devclass_t pca_devclass;

DRIVER_MODULE(pca, isa, pca_driver, pca_devclass, 0, 0);


static int
pcaopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	/* audioctl device can always be opened */
	if (minor(dev) == 128)
		return 0;
	if (minor(dev) > 0)
		return ENXIO;

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
pcaclose(dev_t dev, int flags, int fmt, struct thread *td)
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
pcaioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
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
		__asm__("outb %0,$0x61\n"
			"andb $0xFE,%0\n"
			"outb %0,$0x61"
			: : "a" ((char)pca_status.oldval) );
		__asm__("xlatb\n"
			"outb %0,$0x42"
			: : "a" ((char)pca_status.buffer[pca_status.index]),
			    "b" (volume_table) );
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
		if (SEL_WAITING(&pca_status.wsel))
			selwakeup(&pca_status.wsel);
	}
}


static int
pcapoll(dev_t dev, int events, struct thread *td)
{
 	int s;
	int revents = 0;

 	s = spltty();

	if (events & (POLLOUT | POLLWRNORM)) {
 		if (!pca_status.in_use[0] || !pca_status.in_use[1] ||
 		    !pca_status.in_use[2])
 			revents |= events & (POLLOUT | POLLWRNORM);
 		else
			selrecord(td, &pca_status.wsel);
	}
	splx(s);
	return (revents);
}
