/*
 * sound/sb16_dsp.c
 * 
 * The low level driver for the SoundBlaster DSP chip.
 * 
 * (C) 1993 J. Schubert (jsb@sth.ruhr-uni-bochum.de)
 * 
 * based on SB-driver by (C) Hannu Savolainen
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 * 
 */

#define DEB(x)
#define DEB1(x)
#include <i386/isa/sound/sound_config.h>
#include "sb.h"
#include <i386/isa/sound/sb_mixer.h>
#include <i386/isa/sound/sbcard.h>

#if defined(CONFIG_SB16) && (NSB > 0) && defined(CONFIG_AUDIO) && defined(CONFIG_SBPRO)


extern sound_os_info *sb_osp;
extern int      sbc_base;

static int      sb16_dsp_ok = 0;
static int      dsp_16bit = 0;
static int      dsp_stereo = 0;
static int      dsp_current_speed = 8000;
static int      dsp_busy = 0;
static int      dma16, dma8;


static int      trigger_bits = 0;
static u_long dsp_count = 0;

static int      irq_mode = IMODE_NONE;
static int      my_dev = 0;

static volatile int intr_active = 0;

static int      sb16_dsp_open(int dev, int mode);
static void     sb16_dsp_close(int dev);
static void     sb16_dsp_output_block(int dev, u_long buf, int count, int intrflag, int dma_restart);
static void     sb16_dsp_start_input(int dev, u_long buf, int count, int intrflag, int dma_restart);
static int      sb16_dsp_ioctl(int dev, u_int cmd, ioctl_arg arg, int local);
static int      sb16_dsp_prepare_for_input(int dev, int bsize, int bcount);
static int      sb16_dsp_prepare_for_output(int dev, int bsize, int bcount);
static void     sb16_dsp_reset(int dev);
static void     sb16_dsp_halt(int dev);
static void     sb16_dsp_trigger(int dev, int bits);
static int      dsp_set_speed(int);
static int      dsp_set_stereo(int);
static void     dsp_cleanup(void);

static struct audio_operations sb16_dsp_operations =
{
	"SoundBlaster 16",
	DMA_AUTOMODE,
	AFMT_U8 | AFMT_S16_LE,
	NULL,
	sb16_dsp_open,
	sb16_dsp_close,
	sb16_dsp_output_block,
	sb16_dsp_start_input,
	sb16_dsp_ioctl,
	sb16_dsp_prepare_for_input,
	sb16_dsp_prepare_for_output,
	sb16_dsp_reset,
	sb16_dsp_halt,
	NULL,
	NULL,
	NULL,
	NULL,
	sb16_dsp_trigger
};

static int
sb_dsp_command01(u_char val)
{
    int             i = 1 << 16;

    while (--i & (!inb(DSP_STATUS) & 0x80));
    if (!i)
	printf("SB16 sb_dsp_command01 Timeout\n");
    return sb_dsp_command(val);
}

static int
dsp_set_speed(int mode)
{
    DEB(printf("dsp_set_speed(%d)\n", mode));
    if (mode) {
	RANGE (mode, 5000, 44100);
	dsp_current_speed = mode;
    }
    return mode;
}

static int
dsp_set_stereo(int mode)
{
    DEB(printf("dsp_set_stereo(%d)\n", mode));
    dsp_stereo = mode;
    return mode;
}

static int
dsp_set_bits(int arg)
{
    DEB(printf("dsp_set_bits(%d)\n", arg));

    if (arg)
	dsp_16bit =  (arg == 16) ? 1 : 0 ;
    return dsp_16bit ? 16 : 8;
}

static int
sb16_dsp_ioctl(int dev, u_int cmd, ioctl_arg arg, int local)
{
    switch (cmd) {
    case SOUND_PCM_WRITE_RATE:
	if (local)
	    return dsp_set_speed((int) arg);
	return *(int *) arg = dsp_set_speed((*(int *) arg));

    case SOUND_PCM_READ_RATE:
	if (local)
	    return dsp_current_speed;
	return *(int *) arg = dsp_current_speed;

    case SNDCTL_DSP_STEREO:
	if (local)
	    return dsp_set_stereo((int) arg);
	return *(int *) arg = dsp_set_stereo((*(int *) arg));

    case SOUND_PCM_WRITE_CHANNELS:
	if (local)
	    return dsp_set_stereo((int) arg - 1) + 1;
	return *(int *) arg = dsp_set_stereo((*(int *) arg) - 1) + 1;

    case SOUND_PCM_READ_CHANNELS:
	if (local)
	    return dsp_stereo + 1;
	return *(int *) arg = dsp_stereo + 1;

    case SNDCTL_DSP_SETFMT:
	if (local)
	    return dsp_set_bits((int) arg);
	return *(int *) arg = dsp_set_bits((*(int *) arg));

    case SOUND_PCM_READ_BITS:
	if (local)
	    return dsp_16bit ? 16 : 8;
	return *(int *) arg = dsp_16bit ? 16 : 8;

    case SOUND_PCM_WRITE_FILTER:	/* NOT YET IMPLEMENTED */
	if ((*(int *) arg) > 1)
	    return *(int *) arg = -(EINVAL);

    case FIOASYNC:
	if (local)
	    return 1;
	return *(int *) arg = 1;

    case FIONBIO:
	if (local)
	    return 1;
	return *(int *) arg = 1;

    default:
	return -(EINVAL);
    }

    return -(EINVAL);
}

static int
sb16_dsp_open(int dev, int mode)
{
    DEB(printf("sb16_dsp_open()\n"));

    if (!sb16_dsp_ok) {
	printf("SB16 Error: SoundBlaster board not installed\n");
	return -(ENXIO);
    }
    if (intr_active)
	return -(EBUSY);

    sb_reset_dsp();


    irq_mode = IMODE_NONE;
    dsp_busy = 1;
    trigger_bits = 0;

    return 0;
}

static void
sb16_dsp_close(int dev)
{
    u_long   flags;

    DEB(printf("sb16_dsp_close()\n"));
    sb_dsp_command01(0xd9);
    sb_dsp_command01(0xd5);

    flags = splhigh();

    audio_devs[dev]->dmachan1 = dma8;

    dsp_cleanup();
    dsp_busy = 0;


    splx(flags);
}

static void
sb16_dsp_output_block(int dev, u_long buf, int count, int intrflag, int dma_restart)
{
    u_long   flags, cnt;


    cnt = count;
    if (dsp_16bit)
	cnt >>= 1;
    cnt--;

    if (audio_devs[dev]->flags & DMA_AUTOMODE && intrflag && cnt==dsp_count) {
	irq_mode = IMODE_OUTPUT;
	intr_active = 1;
	return;		/* Auto mode on. No need to react */
    }
    flags = splhigh();

    if (dma_restart) {

	sb16_dsp_halt(dev);
	DMAbuf_start_dma(dev, buf, count, 1);
    }


    sb_dsp_command(0x41);
    sb_dsp_command((u_char) ((dsp_current_speed >> 8) & 0xff));
    sb_dsp_command((u_char) (dsp_current_speed & 0xff));
    sb_dsp_command((u_char) (dsp_16bit ? 0xb6 : 0xc6));
    dsp_count = cnt;
    sb_dsp_command((u_char) ((dsp_stereo ? 0x20 : 0) +
				(dsp_16bit ? 0x10 : 0)));
    sb_dsp_command((u_char) (cnt & 0xff));
    sb_dsp_command((u_char) (cnt >> 8));

    irq_mode = IMODE_OUTPUT;
    intr_active = 1;
    splx(flags);
}

static void
sb16_dsp_start_input(int dev, u_long buf, int count, int intrflag, int dma_restart)
{
    u_long   flags, cnt;

    cnt = count;
    if (dsp_16bit)
	cnt >>= 1;
    cnt--;

    if (audio_devs[dev]->flags & DMA_AUTOMODE && intrflag && cnt == dsp_count) {
	irq_mode = IMODE_INPUT;
	intr_active = 1;
	return;		/* Auto mode on. No need to react */
    }
    flags = splhigh();

    if (dma_restart) {
	sb_reset_dsp();
	DMAbuf_start_dma(dev, buf, count, 0);
    }
    sb_dsp_command(0x42);
    sb_dsp_command((u_char) ((dsp_current_speed >> 8) & 0xff));
    sb_dsp_command((u_char) (dsp_current_speed & 0xff));
    sb_dsp_command((u_char) (dsp_16bit ? 0xbe : 0xce));
    dsp_count = cnt;
    sb_dsp_command((u_char) ((dsp_stereo ? 0x20 : 0) +
				    (dsp_16bit ? 0x10 : 0)));
    sb_dsp_command01((u_char) (cnt & 0xff));
    sb_dsp_command((u_char) (cnt >> 8));

    irq_mode = IMODE_INPUT;
    intr_active = 1;
    splx(flags);
}

static int
sb16_dsp_prepare_for_input(int dev, int bsize, int bcount)
{
    int fudge;
    struct dma_buffparms *dmap =  audio_devs[dev]->dmap_in;

    audio_devs[my_dev]->dmachan2 = dsp_16bit ? dma16 : dma8;


    fudge =  audio_devs[my_dev]->dmachan2 ;

    if (dmap->dma_chan != fudge ) {
      isa_dma_release( dmap->dma_chan);
      isa_dma_acquire(fudge);
      dmap->dma_chan = fudge;
    }

    dsp_count = 0;
    dsp_cleanup();
    if (dsp_16bit) 
	sb_dsp_command(0xd5);	/* Halt DMA until trigger() is called */
    else
 	sb_dsp_command(0xd0);	/* Halt DMA until trigger() is called */

    trigger_bits = 0;
    return 0;
}

static int
sb16_dsp_prepare_for_output(int dev, int bsize, int bcount)
{
    int fudge = dsp_16bit ? dma16 : dma8;
    struct dma_buffparms *dmap =  audio_devs[dev]->dmap_out;

    if (dmap->dma_chan != fudge ) {
      isa_dma_release( dmap->dma_chan);
      isa_dma_acquire(fudge);
      dmap->dma_chan = fudge;
    }

    audio_devs[my_dev]->dmachan1 = fudge;

    dsp_count = 0;
    dsp_cleanup();
    if (dsp_16bit) 
	sb_dsp_command(0xd5);	/* Halt DMA until trigger() is called */
    else
 	sb_dsp_command(0xd0);	/* Halt DMA until trigger() is called */

    trigger_bits = 0;
    return 0;
}

static void
sb16_dsp_trigger(int dev, int bits)
{
    if (bits != 0)
	bits = 1;

    if (bits == trigger_bits)	/* No change */
	return;

    trigger_bits = bits;

    if (!bits)
	sb_dsp_command(0xd0);	/* Halt DMA */
    else if (bits & irq_mode) {
	if (dsp_16bit)
	    sb_dsp_command(0xd6);	/* Continue 16bit DMA */
	else
	    sb_dsp_command(0xd4);	/* Continue 8bit DMA */
    }
}

static void
dsp_cleanup(void)
{
    irq_mode = IMODE_NONE;
    intr_active = 0;
}

static void
sb16_dsp_reset(int dev)
{
    u_long   flags;

    flags = splhigh();

    sb_reset_dsp();
    dsp_cleanup();

    splx(flags);
}

static void
sb16_dsp_halt(int dev)
{

    if (dsp_16bit) {
	sb_dsp_command01(0xd9);
	sb_dsp_command01(0xd5);
    } else {
	sb_dsp_command01(0xda);
	sb_dsp_command01(0xd0);
    }


}

static void
set_irq_hw(int level)
{
    int             ival;

    switch (level) {
#ifdef PC98
    case 5:
	ival = 8;
	break;
    case 3:
	ival = 1;
	break;
    case 10:
	ival = 2;
	break;  
#else
    case 5:
	ival = 2;
	break;
    case 7:
	ival = 4;
	break;
    case 9:
	ival = 1;
	break;
    case 10:
	ival = 8;
	break;
#endif
    default:
	printf("SB16_IRQ_LEVEL %d does not exist\n", level);
	return;
    }
    sb_setmixer(IRQ_NR, ival);
}

void
sb16_dsp_init(struct address_info * hw_config)
{
    if (sbc_major < 4)
	return;		/* Not a SB16 */

    snprintf(sb16_dsp_operations.name, sizeof(sb16_dsp_operations.name),
	"SoundBlaster 16 %d.%d", sbc_major, sbc_minor);

    conf_printf(sb16_dsp_operations.name, hw_config);

    if (num_audiodevs < MAX_AUDIO_DEV) {
	audio_devs[my_dev = num_audiodevs++] = &sb16_dsp_operations;
	audio_devs[my_dev]->dmachan1 = dma8;
	audio_devs[my_dev]->buffsize = DSP_BUFFSIZE;

    } else
	printf("SB: Too many DSP devices available\n");
    sb16_dsp_ok = 1;
    return;
}

int
sb16_dsp_detect(struct address_info * hw_config)
{
    struct address_info *sb_config;

    if (sb16_dsp_ok)
	return 1;	/* Can't drive two cards */

    if (!(sb_config = sound_getconf(SNDCARD_SB))) {
	printf("SB16 Error: Plain SB not configured\n");
	return 0;
    }
    /*
     * sb_setmixer(OPSW,0xf); if(sb_getmixer(OPSW)!=0xf) return 0;
     */

    if (!sb_reset_dsp())
	return 0;

    if (sbc_major < 4)	/* Set by the plain SB driver */
	return 0;	/* Not a SB16 */

#ifdef PC98
    hw_config->dma = sb_config->dma;
#else 
    if (hw_config->dma < 4)
	if (hw_config->dma != sb_config->dma) {
	    printf("SB16 Error: Invalid DMA channel %d/%d\n",
		   sb_config->dma, hw_config->dma);
	    return 0;
	}
#endif
    dma16 = hw_config->dma;
    dma8 = sb_config->dma;
    /*    hw_config->irq = 0;  sb_config->irq; 
    hw_config->io_base = sb_config->io_base;
    */
    set_irq_hw(sb_config->irq);

#ifdef PC98
    sb_setmixer (DMA_NR, hw_config->dma == 0 ? 1 : 2);
#else
    sb_setmixer(DMA_NR, (1 << hw_config->dma) | (1 << sb_config->dma));
#endif

    DEB(printf("SoundBlaster 16: IRQ %d DMA %d OK\n",
		sb_config->irq, hw_config->dma));

    /*
     * dsp_showmessage(0xe3,99);
     */
    sb16_dsp_ok = 1;
    return 1;
}

void
sb16_dsp_interrupt(int unused)
{
	int             data;

	data = inb(DSP_DATA_AVL16);	/* Interrupt acknowledge */

	if (intr_active)
		switch (irq_mode) {
		case IMODE_OUTPUT:
			DMAbuf_outputintr(my_dev, 1);
			break;

		case IMODE_INPUT:
			DMAbuf_inputintr(my_dev);
			break;

		default:
			printf("SoundBlaster: Unexpected interrupt\n");
		}
}
#endif
