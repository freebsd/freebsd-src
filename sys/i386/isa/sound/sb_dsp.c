/*
 * sound/sb_dsp.c
 * 
 * The low level driver for the SoundBlaster DSP chip (SB1.0 to 2.1, SB Pro).
 * 
 * Copyright by Hannu Savolainen 1994
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
 * Modified: Hunyue Yau      Jan 6 1994 Added code to support Sound Galaxy NX
 * Pro
 * 
 * JRA Gibson      April 1995 Code added for MV ProSonic/Jazz 16 in 16 bit mode
 */

#include <i386/isa/sound/sound_config.h>

#if (NSB > 0)

#ifdef SM_WAVE
#define JAZZ16
#endif

#include  <i386/isa/sound/sbcard.h>
#include <i386/isa/sound/sb_mixer.h>
#include <machine/clock.h>

#undef SB_TEST_IRQ

/*
 * XXX note -- only one sb-like device is supported until these
 * variables are put in a struct sb_unit[] array
 */



int             sbc_base = 0;
int             sbc_irq = 0;
static int      open_mode = 0;	/* Read, write or both */
int             Jazz16_detected = 0;
int             sb_no_recording = 0;
static int      dsp_count = 0;
static int      trigger_bits;

/*
 * The DSP channel can be used either for input or output. Variable
 * 'sb_irq_mode' will be set when the program calls read or write first time
 * after open. Current version doesn't support mode changes without closing
 * and reopening the device. Support for this feature may be implemented in a
 * future version of this driver.
 */

int             sb_dsp_ok = 0;	/* Set to 1 after successful init */
static int      midi_disabled = 0;
int             sb_dsp_highspeed = 0;
int             sbc_major = 1, sbc_minor = 0;	/* DSP version   */
static int      dsp_stereo = 0;
static int      dsp_current_speed = DSP_DEFAULT_SPEED;
static int      sb16 = 0;

int             sb_midi_mode = NORMAL_MIDI;
int             sb_midi_busy = 0;	/* 1 if the process has output to *  *
					 * MIDI   */
int             sb_dsp_busy = 0;

volatile int    sb_irq_mode = IMODE_NONE; /* or IMODE_INPUT or IMODE_OUTPUT */

static int      dma8 = 1;

#ifdef JAZZ16 /* 16 bit support for JAZZ16 */

static int      dsp_16bit = 0;
static int      dma16 = 5;

static int      dsp_set_bits(int arg);
static int      initialize_ProSonic16(void);
#endif		/* end of 16 bit support for JAZZ16 */

int             sb_duplex_midi = 0;
static int      my_dev = 0;

volatile int    sb_intr_active = 0;

static int      dsp_speed(int);
static int      dsp_set_stereo(int mode);
static void     sb_dsp_reset(int dev);
sound_os_info  *sb_osp = NULL;

#if defined(CONFIG_MIDI) || defined(CONFIG_AUDIO)

/*
 * Common code for the midi and pcm functions
 */

int
sb_dsp_command(u_char val)
{
    int             i;
    u_long   limit;

    limit = get_time() + hz / 10;	/* The timeout is 0.1 secods */

    /*
     * Note! the i<500000 is an emergency exit. The sb_dsp_command() is
     * sometimes called while interrupts are disabled. This means that
     * the timer is disabled also. However the timeout situation is a
     * abnormal condition. Normally the DSP should be ready to accept
     * commands after just couple of loops.
     */

    for (i = 0; i < 500000 && get_time() < limit; i++) {
	if ((inb(DSP_STATUS) & 0x80) == 0) {
	    outb(DSP_COMMAND, val);
	    return 1;
	}
    }

    printf("SoundBlaster: DSP Command(0x%02x) timeout. IRQ conflict ?\n", val);
    return 0;
}

void
sbintr(int irq)
{
    int             status;

#ifdef CONFIG_SBPRO
    if (sb16) {
	u_char   src = sb_getmixer(IRQ_STAT); /* Interrupt source register */
#ifdef CONFIG_SB16
	if (src & 3)
	    sb16_dsp_interrupt(irq);
#ifdef CONFIG_MIDI
	if (src & 4)
	    sb16midiintr(irq);	/* SB MPU401 interrupt */
#endif	/* CONFIG_MIDI */
#endif	/* CONFIG_SB16 */
	if (!(src & 1))
	    return;	/* Not a DSP interupt */
    }
#endif	/* CONFIG_SBPRO */

    status = inb(DSP_DATA_AVAIL);	/* Clear interrupt */

    if (sb_intr_active)
	switch (sb_irq_mode) {
	case IMODE_OUTPUT:
	    sb_intr_active = 0;
	    DMAbuf_outputintr(my_dev, 1);
	    break;

	case IMODE_INPUT:
	    sb_intr_active = 0;
	    DMAbuf_inputintr(my_dev);
	    /*
	     * A complete buffer has been input. Let's start new one
	     */
	    break;

	case IMODE_INIT:
	    sb_intr_active = 0;
	    break;

	case IMODE_MIDI:
#ifdef CONFIG_MIDI
	    sb_midi_interrupt(irq);
#endif
	    break;

	default:
	    printf("SoundBlaster: Unexpected interrupt\n");
	}
}


int
sb_reset_dsp(void)
{
    int             loopc;

    outb(DSP_RESET, 1);
    DELAY(10);
    outb(DSP_RESET, 0);
    DELAY(30);


    for (loopc = 0; loopc < 100 && !(inb(DSP_DATA_AVAIL) & 0x80); loopc++)
	DELAY(10);

    if (inb(DSP_READ) != 0xAA) {
        printf("sb_reset_dsp failed\n");
	return 0;	/* Sorry */
    }

    return 1;
}

#endif

#ifdef CONFIG_AUDIO

void
dsp_speaker(char state)
{
    if (state)
	sb_dsp_command(DSP_CMD_SPKON);
    else
	sb_dsp_command(DSP_CMD_SPKOFF);
}

static int
dsp_speed(int speed)
{
    u_char   tconst;
    u_long   flags;
    int             max_speed = 44100;

    if (speed < 4000)
	    speed = 4000;

    /*
     * Older SB models don't support higher speeds than 22050.
     */

    if (sbc_major < 2 || (sbc_major == 2 && sbc_minor == 0))
	max_speed = 22050;

    /*
     * SB models earlier than SB Pro have low limit for the input speed.
     */
    if (open_mode != OPEN_WRITE)	/* Recording is possible */
	if (sbc_major < 3) /* Limited input speed with these cards */
	    if (sbc_major == 2 && sbc_minor > 0)
		max_speed = 15000;
	    else
		max_speed = 13000;

    if (speed > max_speed)
	speed = max_speed;	/* Invalid speed */

    /*
     * Logitech SoundMan Games and Jazz16 cards can support 44.1kHz
     * stereo
     */
#if !defined (SM_GAMES)
    /*
     * Max. stereo speed is 22050
     */
    if (dsp_stereo && speed > 22050 && Jazz16_detected == 0)
	speed = 22050;
#endif

    if ((speed > 22050) && sb_midi_busy) {
	printf("SB Warning: High speed DSP not possible simultaneously with MIDI output\n");
	speed = 22050;
    }
    if (dsp_stereo)
	speed *= 2;

    /*
     * Now the speed should be valid
     */

    if (speed > 22050) {	/* High speed mode */
	int             tmp;

	tconst = (u_char) ((65536 - ((256000000 + speed / 2) / speed)) >> 8);
	sb_dsp_highspeed = 1;

	flags = splhigh();
	if (sb_dsp_command(DSP_CMD_TCONST))
	    sb_dsp_command(tconst);
	splx(flags);

	tmp = 65536 - (tconst << 8);
	speed = (256000000 + tmp / 2) / tmp;
    } else {
	int             tmp;

	sb_dsp_highspeed = 0;
	tconst = (256 - ((1000000 + speed / 2) / speed)) & 0xff;

	flags = splhigh();
	if (sb_dsp_command(DSP_CMD_TCONST))	/* Set time constant */
	    sb_dsp_command(tconst);
	splx(flags);

	tmp = 256 - tconst;
	speed = (1000000 + tmp / 2) / tmp;
    }

    if (dsp_stereo)
	speed /= 2;

    dsp_current_speed = speed;
    return speed;
}

static int
dsp_set_stereo(int mode)
{
    dsp_stereo = 0;

#ifndef CONFIG_SBPRO
    return 0;
#else
    if (sbc_major < 3 || sb16)
	return 0;	/* Sorry no stereo */

    if (mode && sb_midi_busy) {
	printf("SB Warning: Stereo DSP not possible simultaneously with MIDI output\n");
	return 0;
    }
    dsp_stereo = !!mode;
    return dsp_stereo;
#endif
}

static void
sb_dsp_output_block(int dev, u_long buf, int count,
		    int intrflag, int restart_dma)
{
    u_long   flags;

    if (!sb_irq_mode)
	dsp_speaker(ON);

    DMAbuf_start_dma(dev, buf, count, 1);

    sb_irq_mode = 0;

    if (audio_devs[dev]->dmachan1 > 3)
	count >>= 1;
    count--;
    dsp_count = count;

    sb_irq_mode = IMODE_OUTPUT;
    if (sb_dsp_highspeed) {
	flags = splhigh();
	if (sb_dsp_command(DSP_CMD_HSSIZE)) {	/* High speed size */
	    sb_dsp_command((u_char) (dsp_count & 0xff));
	    sb_dsp_command((u_char) ((dsp_count >> 8) & 0xff));
	    sb_dsp_command(DSP_CMD_HSDAC);	/* High speed 8 bit DAC */
	} else
	    printf("SB Error: Unable to start (high speed) DAC\n");
	splx(flags);
    } else {
	flags = splhigh();
	if (sb_dsp_command(DSP_CMD_DAC8)) {	/* 8-bit DAC (DMA) */
	    sb_dsp_command((u_char) (dsp_count & 0xff));
	    sb_dsp_command((u_char) ((dsp_count >> 8) & 0xff));
	} else
	    printf("SB Error: Unable to start DAC\n");
	splx(flags);
    }
    sb_intr_active = 1;
}

static void
sb_dsp_start_input(int dev, u_long buf, int count, int intrflag,
		   int restart_dma)
{
    u_long   flags;

    if (sb_no_recording) {
	printf("SB Error: This device doesn't support recording\n");
	return;
    }
    /*
     * Start a DMA input to the buffer pointed by dmaqtail
     */

    if (!sb_irq_mode)
	dsp_speaker(OFF);

    DMAbuf_start_dma(dev, buf, count, 0);
    sb_irq_mode = 0;

    if (audio_devs[dev]->dmachan1 > 3)
	count >>= 1;
    count--;
    dsp_count = count;

    sb_irq_mode = IMODE_INPUT;
    if (sb_dsp_highspeed) {
	flags = splhigh();
	if (sb_dsp_command(DSP_CMD_HSSIZE)) {	/* High speed size */
	    sb_dsp_command((u_char) (dsp_count & 0xff));
	    sb_dsp_command((u_char) ((dsp_count >> 8) & 0xff));
	    sb_dsp_command(DSP_CMD_HSADC);	/* High speed 8 bit ADC */
	} else
	    printf("SB Error: Unable to start (high speed) ADC\n");
	splx(flags);
    } else {
	flags = splhigh();
	if (sb_dsp_command(DSP_CMD_ADC8)) {	/* 8-bit ADC (DMA) */
	    sb_dsp_command((u_char) (dsp_count & 0xff));
	    sb_dsp_command((u_char) ((dsp_count >> 8) & 0xff));
	} else
	    printf("SB Error: Unable to start ADC\n");
	splx(flags);
    }

    sb_intr_active = 1;
}

static void
sb_dsp_trigger(int dev, int bits)
{
    if (bits == trigger_bits)
	return;

    if (!bits)
	sb_dsp_command(0xd0);	/* Halt DMA */
    else if (bits & sb_irq_mode)
	sb_dsp_command(0xd4);	/* Continue DMA */

    trigger_bits = bits;
}

static void
dsp_cleanup(void)
{
    sb_intr_active = 0;
}

static int
sb_dsp_prepare_for_input(int dev, int bsize, int bcount)
{
    int fudge = -1;
    struct dma_buffparms *dmap =  audio_devs[dev]->dmap_in;

    dsp_cleanup();
    dsp_speaker(OFF);

    if (sbc_major == 3) {	/* SB Pro */
#ifdef JAZZ16
	/*
	 * Select correct dma channel for 16/8 bit acccess
	 */
	audio_devs[my_dev]->dmachan1 = dsp_16bit ? dma16 : dma8;
	if (dsp_stereo)
	    sb_dsp_command(dsp_16bit ? 0xac : 0xa8);
	else
	    sb_dsp_command(dsp_16bit ? 0xa4 : 0xa0);
#else
	/*
	 * 8 bit only cards use this
	 */
	if (dsp_stereo)
	    sb_dsp_command(0xa8);
	else
	    sb_dsp_command(0xa0);
#endif
	dsp_speed(dsp_current_speed);	/* Speed must be recalculated
					 * if #channels * changes */
    }

    fudge = audio_devs[my_dev]->dmachan1;
    if (dmap->dma_chan != fudge ) {
      isa_dma_release( dmap->dma_chan);
      isa_dma_acquire(fudge);
      dmap->dma_chan = fudge;
    }

    trigger_bits = 0;
    sb_dsp_command(DSP_CMD_DMAHALT);	/* Halt DMA */
    return 0;
}

static int
sb_dsp_prepare_for_output(int dev, int bsize, int bcount)
{

    int fudge;
    struct dma_buffparms *dmap =  audio_devs[dev]->dmap_out;

    dsp_cleanup();
    dsp_speaker(ON);

#ifdef CONFIG_SBPRO
    if (sbc_major == 3) {	/* SB Pro */
#ifdef JAZZ16
	/*
	 * 16 bit specific instructions
	 */
	audio_devs[my_dev]->dmachan1 = dsp_16bit ? dma16 : dma8;

	if (Jazz16_detected != 2)	/* SM Wave */
	    sb_mixer_set_stereo(dsp_stereo);
	if (dsp_stereo)
	    sb_dsp_command(dsp_16bit ? 0xac : 0xa8);
	else
	    sb_dsp_command(dsp_16bit ? 0xa4 : 0xa0);
#else
	sb_mixer_set_stereo(dsp_stereo);
#endif
	dsp_speed(dsp_current_speed);	/* Speed must be recalculated
					 * if #channels * changes */
    }
#endif
    fudge = audio_devs[my_dev]->dmachan1;

    if (dmap->dma_chan != fudge ) {
      isa_dma_release( dmap->dma_chan);
      isa_dma_acquire(fudge);
      dmap->dma_chan = fudge;
    }

    trigger_bits = 0;
    sb_dsp_command(DSP_CMD_DMAHALT);	/* Halt DMA */
    return 0;
}

static void
sb_dsp_halt_xfer(int dev)
{
}

static int
sb_dsp_open(int dev, int mode)
{
    int             retval;

    if (!sb_dsp_ok) {
	printf("SB Error: SoundBlaster board not installed\n");
	return -(ENXIO);
    }
    if (sb_no_recording && mode & OPEN_READ) {
	printf("SB Warning: Recording not supported by this device\n");
    }
    if (sb_intr_active || (sb_midi_busy && sb_midi_mode == UART_MIDI)) {
	printf("SB: PCM not possible during MIDI input\n");
	return -(EBUSY);
    }
    /*
     * Allocate 8 bit dma
     */
#ifdef JAZZ16
    audio_devs[my_dev]->dmachan1 = dma8;
    /*
     * Allocate 16 bit dma
     */
    if (Jazz16_detected != 0)
	if (dma16 != dma8) {
	    if (0) {
		return -(EBUSY);
	    }
	}
#endif

    sb_irq_mode = IMODE_NONE;

    sb_dsp_busy = 1;
    open_mode = mode;


    return 0;
}

static void
sb_dsp_close(int dev)
{
#ifdef JAZZ16
    /*
     * Release 16 bit dma channel
     */
    if (Jazz16_detected) {
	audio_devs[my_dev]->dmachan1 = dma8;

    }
#endif

    dsp_cleanup();
    dsp_speaker(OFF);
    sb_dsp_busy = 0;
    sb_dsp_highspeed = 0;
    open_mode = 0;

}

#ifdef JAZZ16
/*
 * Function dsp_set_bits() only required for 16 bit cards
 */
static int
dsp_set_bits(int arg)
{
    if (arg)
	if (Jazz16_detected == 0)
	    dsp_16bit = 0;
	else
	    switch (arg) {
	    case 8:
		dsp_16bit = 0;
		break;
	    case 16:
		dsp_16bit = 1;
		break;
	    default:
		dsp_16bit = 0;
	    }
    return dsp_16bit ? 16 : 8;
}

#endif				/* ifdef JAZZ16 */

static int
sb_dsp_ioctl(int dev, u_int cmd, ioctl_arg arg, int local)
{
    switch (cmd) {
    case SOUND_PCM_WRITE_RATE:
	if (local)
	    return dsp_speed((int) arg);
	return *(int *) arg = dsp_speed((*(int *) arg));
	break;

    case SOUND_PCM_READ_RATE:
	if (local)
	    return dsp_current_speed;
	return *(int *) arg = dsp_current_speed;
	break;

    case SOUND_PCM_WRITE_CHANNELS:
	if (local)
	    return dsp_set_stereo((int) arg - 1) + 1;
	return *(int *) arg = dsp_set_stereo((*(int *) arg) - 1) + 1;
	break;

    case SOUND_PCM_READ_CHANNELS:
	if (local)
	    return dsp_stereo + 1;
	return *(int *) arg = dsp_stereo + 1;
	break;

    case SNDCTL_DSP_STEREO:
	if (local)
	    return dsp_set_stereo((int) arg);
	return *(int *) arg = dsp_set_stereo((*(int *) arg));
	break;

#ifdef JAZZ16
    /*
     * Word size specific cases here.
     * SNDCTL_DSP_SETFMT=SOUND_PCM_WRITE_BITS
     */
    case SNDCTL_DSP_SETFMT:
	if (local)
	    return dsp_set_bits((int) arg);
	return *(int *) arg = dsp_set_bits((*(int *) arg));
	break;

    case SOUND_PCM_READ_BITS:
	if (local)
	    return dsp_16bit ? 16 : 8;
	return *(int *) arg = dsp_16bit ? 16 : 8;
	break;
#else
    case SOUND_PCM_WRITE_BITS:
    case SOUND_PCM_READ_BITS:
	if (local)
	    return 8;
	return *(int *) (int) arg = 8;	/* Only 8 bits/sample supported */
	break;
#endif				/* ifdef JAZZ16  */

    case SOUND_PCM_WRITE_FILTER:
    case SOUND_PCM_READ_FILTER:
	return -(EINVAL);
	break;

    default:;
    }

    return -(EINVAL);
}

static void
sb_dsp_reset(int dev)
{
    u_long   flags;

    flags = splhigh();

    sb_reset_dsp();
    dsp_speed(dsp_current_speed);
    dsp_cleanup();

    splx(flags);
}

#endif


#ifdef JAZZ16

/*
 * Initialization of a Media Vision ProSonic 16 Soundcard. The function
 * initializes a ProSonic 16 like PROS.EXE does for DOS. It sets the base
 * address, the DMA-channels, interrupts and enables the joystickport.
 * 
 * Also used by Jazz 16 (same card, different name)
 * 
 * written 1994 by Rainer Vranken E-Mail:
 * rvranken@polaris.informatik.uni-essen.de
 */

u_int
get_sb_byte(void)
{
    int             i;

    for (i = 1000; i; i--)
	if (inb(DSP_DATA_AVAIL) & 0x80) {
	    return inb(DSP_READ);
	}
    return 0xffff;
}

#ifdef SM_WAVE
/*
 * Logitech Soundman Wave detection and initialization by Hannu Savolainen.
 * 
 * There is a microcontroller (8031) in the SM Wave card for MIDI emulation.
 * it's located at address MPU_BASE+4.  MPU_BASE+7 is a SM Wave specific
 * control register for MC reset, SCSI, OPL4 and DSP (future expansion)
 * address decoding. Otherwise the SM Wave is just a ordinary MV Jazz16 based
 * soundcard.
 */

static void
smw_putmem(int base, int addr, u_char val)
{
    u_long   flags;

    flags = splhigh();

    outb(base + 1, addr & 0xff);	/* Low address bits */
    outb(base + 2, addr >> 8);	/* High address bits */
    outb(base, val);	/* Data */

    splx(flags);
}

static u_char
smw_getmem(int base, int addr)
{
    u_long   flags;
    u_char   val;

    flags = splhigh();

    outb(base + 1, addr & 0xff);	/* Low address bits */
    outb(base + 2, addr >> 8);	/* High address bits */
    val = inb(base);	/* Data */

    splx(flags);
    return val;
}

#ifdef SMW_MIDI0001_INCLUDED
#include </sys/i386/isa/sound/smw-midi0001.h>
#else
u_char  *smw_ucode = NULL;
int             smw_ucodeLen = 0;

#endif

static int
initialize_smw(int mpu_base)
{

    int             mp_base = mpu_base + 4;	/* Microcontroller base */
    int             i;
    u_char   control;


    /*
     * Reset the microcontroller so that the RAM can be accessed
     */

    control = inb(mpu_base + 7);
    outb(mpu_base + 7, control | 3);	/* Set last two bits to 1 (?) */
    outb(mpu_base + 7, (control & 0xfe) | 2);	/* xxxxxxx0 resets the mc */
    DELAY(3000); /* Wait at least 1ms */
	
    outb(mpu_base + 7, control & 0xfc);	/* xxxxxx00 enables RAM */

    /*
     * Detect microcontroller by probing the 8k RAM area
     */
    smw_putmem(mp_base, 0, 0x00);
    smw_putmem(mp_base, 1, 0xff);
    DELAY(10);

    if (smw_getmem(mp_base, 0) != 0x00 || smw_getmem(mp_base, 1) != 0xff) {
	printf("\nSM Wave: No microcontroller RAM detected (%02x, %02x)\n",
	       smw_getmem(mp_base, 0), smw_getmem(mp_base, 1));
	return 0;	/* No RAM */
    }
    /*
     * There is RAM so assume it's really a SM Wave
     */

    if (smw_ucodeLen > 0) {
	if (smw_ucodeLen != 8192) {
	    printf("\nSM Wave: Invalid microcode (MIDI0001.BIN) length\n");
	    return 1;
	}
	/*
	 * Download microcode
	 */

	for (i = 0; i < 8192; i++)
	    smw_putmem(mp_base, i, smw_ucode[i]);

	/*
	 * Verify microcode
	 */

	for (i = 0; i < 8192; i++)
	    if (smw_getmem(mp_base, i) != smw_ucode[i]) {
		printf("SM Wave: Microcode verification failed\n");
		return 0;
	    }
    }
    control = 0;
#ifdef SMW_SCSI_IRQ
    /*
     * Set the SCSI interrupt (IRQ2/9, IRQ3 or IRQ10). The SCSI interrupt
     * is disabled by default.
     * 
     * Btw the Zilog 5380 SCSI controller is located at MPU base + 0x10.
     */
    {
	static u_char scsi_irq_bits[] =
	    {0, 0, 3, 1, 0, 0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0};

	control |= scsi_irq_bits[SMW_SCSI_IRQ] << 6;
    }
#endif

#ifdef SMW_OPL4_ENABLE
    /*
     * Make the OPL4 chip visible on the PC bus at 0x380.
     * 
     * There is no need to enable this feature since VoxWare doesn't support
     * OPL4 yet. Also there is no RAM in SM Wave so enabling OPL4 is
     * pretty useless.
     */
    control |= 0x10;	/* Uses IRQ12 if bit 0x20 == 0 */
    /* control |= 0x20;      Uncomment this if you want to use IRQ7 */
#endif

    outb(mpu_base + 7, control | 0x03);	/* xxxxxx11 restarts */
    return 1;
}

#endif

static int
initialize_ProSonic16(void)
{
    int             x;
    static u_char int_translat[16] =
	    {0, 0, 2, 3, 0, 1, 0, 4, 0, 2, 5, 0, 0, 0, 0, 6},
	dma_translat[8] =
	    {0, 1, 0, 2, 0, 3, 0, 4};

    struct address_info *mpu_config;

    int             mpu_base, mpu_irq;

    if ((mpu_config = sound_getconf(SNDCARD_MPU401))) {
	mpu_base = mpu_config->io_base;
	mpu_irq = mpu_config->irq;
    } else {
	mpu_base = mpu_irq = 0;
    }

    outb(0x201, 0xAF);	/* ProSonic/Jazz16 wakeup */
    DELAY(15000);	/* wait at least 10 milliseconds */
    outb(0x201, 0x50);
    outb(0x201, (sbc_base & 0x70) | ((mpu_base & 0x30) >> 4));

    if (sb_reset_dsp()) {	/* OK. We have at least a SB */

	/* Check the version number of ProSonic (I guess) */

	if (!sb_dsp_command(0xFA))
	    return 1;
	if (get_sb_byte() != 0x12)
	    return 1;

	if (sb_dsp_command(0xFB) &&	/* set DMA-channels and Interrupts */
	    sb_dsp_command((dma_translat[JAZZ_DMA16]<<4)|dma_translat[dma8]) &&
	    sb_dsp_command((int_translat[mpu_irq]<<4)|int_translat[sbc_irq])) {
	    Jazz16_detected = 1;
	    if (mpu_base == 0)
		printf("Jazz16: No MPU401 devices configured - MIDI port not initialized\n");

#ifdef SM_WAVE
		if (mpu_base != 0)
		    if (initialize_smw(mpu_base))
			Jazz16_detected = 2;
#endif
	    sb_dsp_disable_midi();
	}
	return 1;	/* There was at least a SB */
    }
    return 0;		/* No SB or ProSonic16 detected */
}

#endif				/* ifdef JAZZ16  */

int
sb_dsp_detect(struct address_info * hw_config)
{
    sbc_base = hw_config->io_base;
    sbc_irq = hw_config->irq;
    sb_osp = hw_config->osp;


    if (sb_dsp_ok)
	return 0;	/* Already initialized */
    dma8 = hw_config->dma;

#ifdef JAZZ16
    dma16 = JAZZ_DMA16;

    if (!initialize_ProSonic16())
	return 0;
#else
    if (!sb_reset_dsp())
	return 0;
#endif

    return 1;		/* Detected */
}

#ifdef CONFIG_AUDIO
static struct audio_operations sb_dsp_operations =
{
    "SoundBlaster",
    NOTHING_SPECIAL,
    AFMT_U8,		/* Just 8 bits. Poor old SB */
    NULL,
    sb_dsp_open,
    sb_dsp_close,
    sb_dsp_output_block,
    sb_dsp_start_input,
    sb_dsp_ioctl,
    sb_dsp_prepare_for_input,
    sb_dsp_prepare_for_output,
    sb_dsp_reset,
    sb_dsp_halt_xfer,
    NULL,			/* local_qlen */
    NULL,			/* copy_from_user */
    NULL,
    NULL,
    sb_dsp_trigger
};

#endif

void
sb_dsp_init(struct address_info * hw_config)
{
    int             i;
    char *fmt = NULL ;

#ifdef CONFIG_SBPRO
    int             mixer_type = 0;

#endif

    sb_osp = hw_config->osp;
    sbc_major = sbc_minor = 0;
    sb_dsp_command(DSP_CMD_GETVER);	/* Get version */

    for (i = 10000; i; i--) { /* perhaps wait longer on a fast machine ? */
	if (inb(DSP_DATA_AVAIL) & 0x80) { /* wait for Data Ready */
	    if (sbc_major == 0)
		sbc_major = inb(DSP_READ);
	    else {
		sbc_minor = inb(DSP_READ);
		break;
	    }
	} else
	    DELAY(20);
    }

    if (sbc_major == 0) {
	printf("\n\nFailed to get SB version (%x) - possible I/O conflict\n\n",
	       inb(DSP_DATA_AVAIL));
	sbc_major = 1;
    }
    if (sbc_major == 2 || sbc_major == 3)
	sb_duplex_midi = 1;

    if (sbc_major == 4)
	sb16 = 1;

    if (sbc_major == 3 && sbc_minor == 1) {
	int             ess_major = 0, ess_minor = 0;

	/*
	 * Try to detect ESS chips.
	 */

	sb_dsp_command(DSP_CMD_GETID);	/* Return identification bytes. */

	for (i = 1000; i; i--) {
	    if (inb(DSP_DATA_AVAIL) & 0x80) {	/* wait for Data Ready */
		if (ess_major == 0)
		    ess_major = inb(DSP_READ);
		else {
		    ess_minor = inb(DSP_READ);
		    break;
		}
	    }
	}

	if (ess_major == 0x48 && (ess_minor & 0xf0) == 0x80)
	    printf("Hmm... Could this be an ESS488 based card (rev %d)\n",
	       ess_minor & 0x0f);
	else if (ess_major == 0x68 && (ess_minor & 0xf0) == 0x80)
	    printf("Hmm... Could this be an ESS688 based card (rev %d)\n",
	       ess_minor & 0x0f);
    }
    if (snd_set_irq_handler(sbc_irq, sbintr, sb_osp) < 0)
	printf("sb_dsp: Can't allocate IRQ\n");;

#ifdef CONFIG_SBPRO
    if (sbc_major >= 3)
	mixer_type = sb_mixer_init(sbc_major);
#else
    if (sbc_major >= 3)
	printf("\nNOTE! SB Pro support required with your soundcard!\n");
#endif


#ifdef CONFIG_AUDIO
    if (sbc_major >= 3) {
	if (Jazz16_detected) {
	    if (Jazz16_detected == 2)
		fmt = "SoundMan Wave %d.%d";
	    else
		fmt = "MV Jazz16 %d.%d";
	    sb_dsp_operations.format_mask |= AFMT_S16_LE;	/* 16 bits */
	} else
#ifdef __SGNXPRO__
	if (mixer_type == 2)
	    fmt = "Sound Galaxy NX Pro %d.%d" ;
	else
#endif	/* __SGNXPRO__ */
	if (sbc_major == 4)
	    fmt = "SoundBlaster 16 %d.%d";
	else
	    fmt = "SoundBlaster Pro %d.%d";
    } else {
	fmt = "SoundBlaster %d.%d" ;
    }

    sprintf(sb_dsp_operations.name, fmt, sbc_major, sbc_minor);
    conf_printf(sb_dsp_operations.name, hw_config);

#if defined(CONFIG_SB16) && defined(CONFIG_SBPRO)
    if (!sb16)		/* There is a better driver for SB16 */
#endif	/* CONFIG_SB16 && CONFIG_SBPRO */
	if (num_audiodevs < MAX_AUDIO_DEV) {
	    audio_devs[my_dev = num_audiodevs++] = &sb_dsp_operations;
	    audio_devs[my_dev]->buffsize = DSP_BUFFSIZE;
	    dma8 = audio_devs[my_dev]->dmachan1 = hw_config->dma;
	    audio_devs[my_dev]->dmachan2 = -1;
#ifdef JAZZ16
	    /*
	     * Allocate 16 bit dma
	     */
	    if (Jazz16_detected != 0)
		if (dma16 != dma8) {
		    if (0) {
			printf("Jazz16: Can't allocate 16 bit DMA channel\n");
		    }
		}
#endif	/* JAZZ16 */
	} else
	    printf("SB: Too many DSP devices available\n");
#else
    conf_printf("SoundBlaster (configured without audio support)", hw_config);
#endif

#ifdef CONFIG_MIDI
    if (!midi_disabled && !sb16) {
	/*
	 * Midi don't work in the SB emulation mode of PAS,
	 * SB16 has better midi interface
	 */
	sb_midi_init(sbc_major);
    }
#endif	/* CONFIG_MIDI */
    sb_dsp_ok = 1;
}

void
sb_dsp_disable_midi(void)
{
    midi_disabled = 1;
}
#endif
