/*
 * sound/pas2_midi.c
 * 
 * The low level driver for the PAS Midi Interface.
 * 
 * Copyright by Hannu Savolainen 1993
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

#if defined(CONFIG_PAS) && defined(CONFIG_MIDI)
#include <i386/isa/sound/pas_hw.h>

static int      midi_busy = 0, input_opened = 0;
static int      my_dev;
static volatile int ofifo_bytes = 0;

static u_char tmp_queue[256];
static volatile int qlen;
static volatile u_char qhead, qtail;

static void     (*midi_input_intr) (int dev, u_char data);

static int
pas_midi_open(int dev, int mode,
	      void (*input) (int dev, u_char data),
	      void (*output) (int dev)
)
{
	int             err;
	u_long   flags;
	u_char   ctrl;


	if (midi_busy) {
		printf("PAS2: Midi busy\n");
		return -(EBUSY);
	}
	/*
	 * Reset input and output FIFO pointers
	 */
	pas_write(M_C_RESET_INPUT_FIFO | M_C_RESET_OUTPUT_FIFO,
		  MIDI_CONTROL);

	flags = splhigh();

	if ((err = pas_set_intr(I_M_MIDI_IRQ_ENABLE)) < 0)
		return err;

	/*
	 * Enable input available and output FIFO empty interrupts
	 */

	ctrl = 0;
	input_opened = 0;
	midi_input_intr = input;

	if (mode == OPEN_READ || mode == OPEN_READWRITE) {
		ctrl |= M_C_ENA_INPUT_IRQ;	/* Enable input */
		input_opened = 1;
	}
	if (mode == OPEN_WRITE || mode == OPEN_READWRITE) {
		ctrl |= M_C_ENA_OUTPUT_IRQ |	/* Enable output */
			M_C_ENA_OUTPUT_HALF_IRQ;
	}
	pas_write(ctrl,
		  MIDI_CONTROL);

	/*
	 * Acknowledge any pending interrupts
	 */

	pas_write(0xff, MIDI_STATUS);
	ofifo_bytes = 0;

	splx(flags);

	midi_busy = 1;
	qlen = qhead = qtail = 0;
	return 0;
}

static void
pas_midi_close(int dev)
{

	/*
	 * Reset FIFO pointers, disable intrs
	 */
	pas_write(M_C_RESET_INPUT_FIFO | M_C_RESET_OUTPUT_FIFO, MIDI_CONTROL);

	pas_remove_intr(I_M_MIDI_IRQ_ENABLE);
	midi_busy = 0;
}

static int
dump_to_midi(u_char midi_byte)
{
	int             fifo_space, x;

	fifo_space = ((x = pas_read(MIDI_FIFO_STATUS)) >> 4) & 0x0f;

	if (fifo_space == 15 || (fifo_space < 2 && ofifo_bytes > 13)) {	/* Fifo full */
		return 0;	/* Upper layer will call again */
	}
	ofifo_bytes++;

	pas_write(midi_byte, MIDI_DATA);

	return 1;
}

static int
pas_midi_out(int dev, u_char midi_byte)
{

	u_long   flags;

	/*
	 * Drain the local queue first
	 */

	flags = splhigh();

	while (qlen && dump_to_midi(tmp_queue[qhead])) {
		qlen--;
		qhead++;
	}

	splx(flags);

	/*
	 * Output the byte if the local queue is empty.
	 */

	if (!qlen)
		if (dump_to_midi(midi_byte))
			return 1;	/* OK */

	/*
	 * Put to the local queue
	 */

	if (qlen >= 256)
		return 0;	/* Local queue full */

	flags = splhigh();

	tmp_queue[qtail] = midi_byte;
	qlen++;
	qtail++;

	splx(flags);

	return 1;
}

static int
pas_midi_start_read(int dev)
{
	return 0;
}

static int
pas_midi_end_read(int dev)
{
	return 0;
}

static int
pas_midi_ioctl(int dev, u_int cmd, ioctl_arg arg)
{
	return -(EINVAL);
}

static void
pas_midi_kick(int dev)
{
	ofifo_bytes = 0;
}

static int
pas_buffer_status(int dev)
{
	return qlen;
}

#define MIDI_SYNTH_NAME	"Pro Audio Spectrum Midi"
#define MIDI_SYNTH_CAPS	SYNTH_CAP_INPUT
#include <i386/isa/sound/midi_synth.h>

static struct midi_operations pas_midi_operations =
{
	{"Pro Audio Spectrum", 0, 0, SNDCARD_PAS},
	&std_midi_synth,
	{0},
	pas_midi_open,
	pas_midi_close,
	pas_midi_ioctl,
	pas_midi_out,
	pas_midi_start_read,
	pas_midi_end_read,
	pas_midi_kick,
	NULL,			/* command */
	pas_buffer_status,
	NULL
};

void
pas_midi_init()
{
	if (num_midis >= MAX_MIDI_DEV) {
		printf("Sound: Too many midi devices detected\n");
		return;
	}
	std_midi_synth.midi_dev = my_dev = num_midis;
	midi_devs[num_midis++] = &pas_midi_operations;
	return;
}

void
pas_midi_interrupt(void)
{
	u_char   stat;
	int             i, incount;
	u_long   flags;

	stat = pas_read(MIDI_STATUS);

	if (stat & M_S_INPUT_AVAIL) {	/* Input byte available */
		incount = pas_read(MIDI_FIFO_STATUS) & 0x0f;	/* Input FIFO count */
		if (!incount)
			incount = 16;

		for (i = 0; i < incount; i++)
			if (input_opened) {
				midi_input_intr(my_dev, pas_read(MIDI_DATA));
			} else
				pas_read(MIDI_DATA);	/* Flush */
	}
	if (stat & (M_S_OUTPUT_EMPTY | M_S_OUTPUT_HALF_EMPTY)) {
		if (!(stat & M_S_OUTPUT_EMPTY)) {
			ofifo_bytes = 8;
		} else {
			ofifo_bytes = 0;
		}

		flags = splhigh();

		while (qlen && dump_to_midi(tmp_queue[qhead])) {
			qlen--;
			qhead++;
		}

		splx(flags);
	}
	if (stat & M_S_OUTPUT_OVERRUN) {
		printf("MIDI output overrun %x,%x,%d \n", pas_read(MIDI_FIFO_STATUS), stat, ofifo_bytes);
		ofifo_bytes = 100;
	}
	pas_write(stat, MIDI_STATUS);	/* Acknowledge interrupts */
}

#endif
