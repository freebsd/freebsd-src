/*
 * sound/gus2_midi.c
 * 
 * The low level driver for the GUS Midi Interface.
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

#if defined(CONFIG_GUS) && defined(CONFIG_MIDI)
#include <i386/isa/sound/gus_hw.h>

static int      midi_busy = 0, input_opened = 0;
static int      my_dev;
static int      output_used = 0;
static volatile unsigned char gus_midi_control;

static void     (*midi_input_intr) (int dev, unsigned char data);

static unsigned char tmp_queue[256];
static volatile int qlen;
static volatile unsigned char qhead, qtail;
extern int      gus_base, gus_irq, gus_dma;
extern sound_os_info *gus_osp;

#define GUS_MIDI_STATUS()	inb( u_MidiStatus)

static int
gus_midi_open(int dev, int mode,
	      void (*input) (int dev, unsigned char data),
	      void (*output) (int dev)
)
{

	if (midi_busy) {
		printf("GUS: Midi busy\n");
		return -(EBUSY);
	}
	outb(u_MidiControl, MIDI_RESET);
	gus_delay();

	gus_midi_control = 0;
	input_opened = 0;

	if (mode == OPEN_READ || mode == OPEN_READWRITE) {
		gus_midi_control |= MIDI_ENABLE_RCV;
		input_opened = 1;
	}
	if (mode == OPEN_WRITE || mode == OPEN_READWRITE) {
		gus_midi_control |= MIDI_ENABLE_XMIT;
	}
	outb(u_MidiControl, gus_midi_control);	/* Enable */

	midi_busy = 1;
	qlen = qhead = qtail = output_used = 0;
	midi_input_intr = input;

	return 0;
}

static int
dump_to_midi(unsigned char midi_byte)
{
	unsigned long   flags;
	int             ok = 0;

	output_used = 1;

	flags = splhigh();

	if (GUS_MIDI_STATUS() & MIDI_XMIT_EMPTY) {
		ok = 1;
		outb(u_MidiData, midi_byte);
	} else {
		/*
		 * Enable Midi xmit interrupts (again)
		 */
		gus_midi_control |= MIDI_ENABLE_XMIT;
		outb(u_MidiControl, gus_midi_control);
	}

	splx(flags);
	return ok;
}

static void
gus_midi_close(int dev)
{
	/*
	 * Reset FIFO pointers, disable intrs
	 */

	outb(u_MidiControl, MIDI_RESET);
	midi_busy = 0;
}

static int
gus_midi_out(int dev, unsigned char midi_byte)
{

	unsigned long   flags;

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
gus_midi_start_read(int dev)
{
	return 0;
}

static int
gus_midi_end_read(int dev)
{
	return 0;
}

static int
gus_midi_ioctl(int dev, unsigned cmd, ioctl_arg arg)
{
	return -(EINVAL);
}

static void
gus_midi_kick(int dev)
{
}

static int
gus_midi_buffer_status(int dev)
{
	unsigned long   flags;

	if (!output_used)
		return 0;

	flags = splhigh();

	if (qlen && dump_to_midi(tmp_queue[qhead])) {
		qlen--;
		qhead++;
	}
	splx(flags);

	return (qlen > 0) | !(GUS_MIDI_STATUS() & MIDI_XMIT_EMPTY);
}

#define MIDI_SYNTH_NAME	"Gravis Ultrasound Midi"
#define MIDI_SYNTH_CAPS	SYNTH_CAP_INPUT
#include <i386/isa/sound/midi_synth.h>

static struct midi_operations gus_midi_operations =
{
	{"Gravis UltraSound Midi", 0, 0, SNDCARD_GUS},
	&std_midi_synth,
	{0},
	gus_midi_open,
	gus_midi_close,
	gus_midi_ioctl,
	gus_midi_out,
	gus_midi_start_read,
	gus_midi_end_read,
	gus_midi_kick,
	NULL,			/* command */
	gus_midi_buffer_status,
	NULL
};

void
gus_midi_init()
{
	if (num_midis >= MAX_MIDI_DEV) {
		printf("Sound: Too many midi devices detected\n");
		return;
	}
	outb(u_MidiControl, MIDI_RESET);

	std_midi_synth.midi_dev = my_dev = num_midis;
	midi_devs[num_midis++] = &gus_midi_operations;
	return;
}

void
gus_midi_interrupt(int dummy)
{
	unsigned char   stat, data;
	unsigned long   flags;

	flags = splhigh();

	stat = GUS_MIDI_STATUS();

	if (stat & MIDI_RCV_FULL) {
		data = inb(u_MidiData);
		if (input_opened)
			midi_input_intr(my_dev, data);
	}
	if (stat & MIDI_XMIT_EMPTY) {
		while (qlen && dump_to_midi(tmp_queue[qhead])) {
			qlen--;
			qhead++;
		}

		if (!qlen) {
			/*
			 * Disable Midi output interrupts, since no data in
			 * the buffer
			 */
			gus_midi_control &= ~MIDI_ENABLE_XMIT;
			outb(u_MidiControl, gus_midi_control);
		}
	}
	splx(flags);
}

#endif
