/*
 * sound/mad16_sb_midi.c
 * 
 * The low level driver for MAD16 SoundBlaster-DS-chip-based MIDI.
 * 
 * Copyright by Hannu Savolainen 1993, Aaron Ucko 1995
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
 */

#include <i386/isa/sound/sound_config.h>

#if defined(CONFIG_MAD16) && defined(CONFIG_MIDI)

#define sbc_base mad16_sb_base
#include <i386/isa/sound/sb_card.h>

static int      input_opened = 0;
static int      my_dev;
static int      mad16_sb_base = 0x220;
static int      mad16_sb_irq = 0;
static int      mad16_sb_dsp_ok = 0;
static sound_os_info *midi_osp;

int             mad16_sb_midi_mode = NORMAL_MIDI;
int             mad16_sb_midi_busy = 0;

int             mad16_sb_duplex_midi = 0;
volatile int    mad16_sb_intr_active = 0;

void            (*midi_input_intr) (int dev, unsigned char data);

static void     mad16_sb_midi_init(int model);

static int
mad16_sb_dsp_command(unsigned char val)
{
	int             i;
	unsigned long   limit;

	limit = get_time() + hz / 10;	/* The timeout is 0.1 secods */

	/*
	 * Note! the i<500000 is an emergency exit. The
	 * mad16_sb_dsp_command() is sometimes called while interrupts are
	 * disabled. This means that the timer is disabled also. However the
	 * timeout situation is a abnormal condition. Normally the DSP should
	 * be ready to accept commands after just couple of loops.
	 */

	for (i = 0; i < 500000 && get_time() < limit; i++) {
		if ((inb(DSP_STATUS) & 0x80) == 0) {
			outb(DSP_COMMAND, val);
			return 1;
		}
	}

	printf("MAD16 (SBP mode): DSP Command(%x) Timeout.\n", val);
	printf("IRQ conflict???\n");
	return 0;
}

void
mad16_sbintr(int irq)
{
	int             status;

	unsigned long   flags;
	unsigned char   data;

	status = inb(DSP_DATA_AVAIL);	/* Clear interrupt */

	flags = splhigh();

	data = inb(DSP_READ);
	if (input_opened)
		midi_input_intr(my_dev, data);

	splx(flags);
}

static int
mad16_sb_reset_dsp(void)
{
	int             loopc;

	outb(DSP_RESET, 1);
	DELAY(10);
	outb(DSP_RESET, 0);
	DELAY(30);

	for (loopc = 0; loopc < 100 && !(inb(DSP_DATA_AVAIL) & 0x80); loopc++)
		DELAY(10);
		/* Wait for data available status */

	if (inb(DSP_READ) != 0xAA)
		return 0;	/* Sorry */

	return 1;
}

int
mad16_sb_dsp_detect(struct address_info * hw_config)
{
	mad16_sb_base = hw_config->io_base;
	mad16_sb_irq = hw_config->irq;
	midi_osp = hw_config->osp;

	if (mad16_sb_dsp_ok)
		return 0;	/* Already initialized */
	if (!mad16_sb_reset_dsp())
		return 0;

	return 1;		/* Detected */
}

void
mad16_sb_dsp_init(struct address_info * hw_config)
/*
 * this function now just verifies the reported version and calls
 * mad16_sb_midi_init -- everything else is done elsewhere
 */
{

	midi_osp = hw_config->osp;
	if (snd_set_irq_handler(mad16_sb_irq, mad16_sbintr, midi_osp) < 0) {
		printf("MAD16 SB MIDI: IRQ not free\n");
		return;
	}

	conf_printf("MAD16 MIDI (SB mode)", hw_config);
	mad16_sb_midi_init(2);

	mad16_sb_dsp_ok = 1;
	return;
}

static int
mad16_sb_midi_open(int dev, int mode,
		   void (*input) (int dev, unsigned char data),
		   void (*output) (int dev)
)
{

	if (!mad16_sb_dsp_ok) {
		printf("MAD16_SB Error: MIDI hardware not installed\n");
		return -(ENXIO);
	}
	if (mad16_sb_midi_busy)
		return -(EBUSY);

	if (mode != OPEN_WRITE && !mad16_sb_duplex_midi) {
		if (num_midis == 1)
			printf("MAD16 (SBP mode): Midi input not currently supported\n");
		return -(EPERM);
	}
	mad16_sb_midi_mode = NORMAL_MIDI;
	if (mode != OPEN_WRITE) {
		if (mad16_sb_intr_active)
			return -(EBUSY);
		mad16_sb_midi_mode = UART_MIDI;
	}
	if (mad16_sb_midi_mode == UART_MIDI) {
		mad16_sb_reset_dsp();

		if (!mad16_sb_dsp_command(0x35))
			return -(EIO);	/* Enter the UART mode */
		mad16_sb_intr_active = 1;

		input_opened = 1;
		midi_input_intr = input;
	}
	mad16_sb_midi_busy = 1;

	return 0;
}

static void
mad16_sb_midi_close(int dev)
{
	if (mad16_sb_midi_mode == UART_MIDI) {
		mad16_sb_reset_dsp();	/* The only way to kill the UART mode */
	}
	mad16_sb_intr_active = 0;
	mad16_sb_midi_busy = 0;
	input_opened = 0;
}

static int
mad16_sb_midi_out(int dev, unsigned char midi_byte)
{
	unsigned long   flags;

	if (mad16_sb_midi_mode == NORMAL_MIDI) {
		flags = splhigh();
		if (mad16_sb_dsp_command(0x38))
			mad16_sb_dsp_command(midi_byte);
		else
			printf("MAD16_SB Error: Unable to send a MIDI byte\n");
		splx(flags);
	} else
		mad16_sb_dsp_command(midi_byte);	/* UART write */

	return 1;
}

static int
mad16_sb_midi_start_read(int dev)
{
	if (mad16_sb_midi_mode != UART_MIDI) {
		printf("MAD16 (SBP mode): MIDI input not implemented.\n");
		return -(EPERM);
	}
	return 0;
}

static int
mad16_sb_midi_end_read(int dev)
{
	if (mad16_sb_midi_mode == UART_MIDI) {
		mad16_sb_reset_dsp();
		mad16_sb_intr_active = 0;
	}
	return 0;
}

static int
mad16_sb_midi_ioctl(int dev, unsigned cmd, ioctl_arg arg)
{
	return -(EPERM);
}

#define MIDI_SYNTH_NAME	"pseudo-SoundBlaster Midi"
#define MIDI_SYNTH_CAPS	0
#include <i386/isa/sound/midi_synth.h>

static struct midi_operations mad16_sb_midi_operations =
{
	{"MAD16 (SBP mode)", 0, 0, SNDCARD_MAD16},
	&std_midi_synth,
	{0},
	mad16_sb_midi_open,
	mad16_sb_midi_close,
	mad16_sb_midi_ioctl,
	mad16_sb_midi_out,
	mad16_sb_midi_start_read,
	mad16_sb_midi_end_read,
	NULL,			/* Kick */
	NULL,			/* command */
	NULL,			/* buffer_status */
	NULL
};

static void
mad16_sb_midi_init(int model)
{
	if (num_midis >= MAX_MIDI_DEV) {
		printf("Sound: Too many midi devices detected\n");
		return;
	}
	std_midi_synth.midi_dev = num_midis;
	my_dev = num_midis;
	midi_devs[num_midis++] = &mad16_sb_midi_operations;
}

#endif
