/*
 * sound/pas2_card.c
 * 
 * Detection routine for the Pro Audio Spectrum cards.
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

#if defined(CONFIG_PAS)
#define _PAS2_CARD_C_

#define DEFINE_TRANSLATIONS
#include <i386/isa/sound/pas_hw.h>

/*
 * The Address Translation code is used to convert I/O register addresses to
 * be relative to the given base -register
 */

int             translat_code;
static int      pas_intr_mask = 0;
static int      pas_irq = 0;

sound_os_info  *pas_osp;

char            pas_model;
static char    *pas_model_names[] =
{"", "Pro AudioSpectrum+", "CDPC", "Pro AudioSpectrum 16", "Pro AudioSpectrum 16D"};

/*
 * pas_read() and pas_write() are equivalents of inb and outb
 */
/*
 * These routines perform the I/O address translation required
 */
/*
 * to support other than the default base address
 */
extern void     mix_write(u_char data, int ioaddr);

u_char
pas_read(int ioaddr)
{
	return inb(ioaddr ^ translat_code);
}

void
pas_write(u_char data, int ioaddr)
{
	outb(ioaddr ^ translat_code, data);
}

void
pas2_msg(char *foo)
{
	printf("    PAS2: %s.\n", foo);
}

/******************* Begin of the Interrupt Handler ********************/

void
pasintr(int irq)
{
	int             status;

	status = pas_read(INTERRUPT_STATUS);
	pas_write(status, INTERRUPT_STATUS);	/* Clear interrupt */

	if (status & I_S_PCM_SAMPLE_BUFFER_IRQ) {
#ifdef CONFIG_AUDIO
		pas_pcm_interrupt(status, 1);
#endif
		status &= ~I_S_PCM_SAMPLE_BUFFER_IRQ;
	}
	if (status & I_S_MIDI_IRQ) {
#ifdef CONFIG_MIDI
		pas_midi_interrupt();
#endif
		status &= ~I_S_MIDI_IRQ;
	}
}

int
pas_set_intr(int mask)
{
	if (!mask)
		return 0;

	pas_intr_mask |= mask;

	pas_write(pas_intr_mask, INTERRUPT_MASK);
	return 0;
}

int
pas_remove_intr(int mask)
{
	if (!mask)
		return 0;

	pas_intr_mask &= ~mask;
	pas_write(pas_intr_mask, INTERRUPT_MASK);

	return 0;
}

/******************* End of the Interrupt handler **********************/

/******************* Begin of the Initialization Code ******************/

int
config_pas_hw(struct address_info * hw_config)
{
	char            ok = 1;
	u_int        int_ptrs;	/* scsi/sound interrupt pointers */

	pas_irq = hw_config->irq;

	pas_write(0x00, INTERRUPT_MASK);

	pas_write(0x36, SAMPLE_COUNTER_CONTROL);	/* Local timer control *
							 * register */

	pas_write(0x36, SAMPLE_RATE_TIMER);	/* Sample rate timer (16 bit) */
	pas_write(0, SAMPLE_RATE_TIMER);

	pas_write(0x74, SAMPLE_COUNTER_CONTROL);	/* Local timer control *
							 * register */

	pas_write(0x74, SAMPLE_BUFFER_COUNTER);	/* Sample count register (16 *
						 * bit) */
	pas_write(0, SAMPLE_BUFFER_COUNTER);

	pas_write(F_F_PCM_BUFFER_COUNTER | F_F_PCM_RATE_COUNTER | F_F_MIXER_UNMUTE | 1, FILTER_FREQUENCY);
	pas_write(P_C_PCM_DMA_ENABLE | P_C_PCM_MONO | P_C_PCM_DAC_MODE | P_C_MIXER_CROSS_L_TO_L | P_C_MIXER_CROSS_R_TO_R, PCM_CONTROL);
	pas_write(S_M_PCM_RESET | S_M_FM_RESET | S_M_SB_RESET | S_M_MIXER_RESET /* | S_M_OPL3_DUAL_MONO */ , SERIAL_MIXER);

	pas_write(I_C_1_BOOT_RESET_ENABLE
#ifdef PAS_JOYSTICK_ENABLE
		  | I_C_1_JOYSTICK_ENABLE
#endif
		  ,IO_CONFIGURATION_1);

	if (pas_irq < 0 || pas_irq > 15) {
		printf("PAS2: Invalid IRQ %d", pas_irq);
		ok = 0;
	} else {
		int_ptrs = pas_read(IO_CONFIGURATION_3);
		int_ptrs |= I_C_3_PCM_IRQ_translate[pas_irq] & 0xf;
		pas_write(int_ptrs, IO_CONFIGURATION_3);
		if (!I_C_3_PCM_IRQ_translate[pas_irq]) {
		    printf("PAS2: Invalid IRQ %d", pas_irq);
		    ok = 0;
		} else {
		    if (snd_set_irq_handler(pas_irq, pasintr, hw_config->osp) < 0)
			ok = 0;
		}
	}

	if (hw_config->dma < 0 || hw_config->dma > 7) {
		printf("PAS2: Invalid DMA selection %d", hw_config->dma);
		ok = 0;
	} else {
		pas_write(I_C_2_PCM_DMA_translate[hw_config->dma], IO_CONFIGURATION_2);
		if (!I_C_2_PCM_DMA_translate[hw_config->dma]) {
			printf("PAS2: Invalid DMA selection %d", hw_config->dma);
			ok = 0;
		} else {
			if (0) {
				printf("pas2_card.c: Can't allocate DMA channel\n");
				ok = 0;
			}
		}
	}

	/*
	 * This fixes the timing problems of the PAS due to the Symphony
	 * chipset as per Media Vision.  Only define this if your PAS doesn't
	 * work correctly.
	 */
#ifdef SYMPHONY_PAS
	outb(0xa8, 0x05);
	outb(0xa9, 0x60);
#endif

#ifdef BROKEN_BUS_CLOCK
	pas_write(S_C_1_PCS_ENABLE | S_C_1_PCS_STEREO | S_C_1_PCS_REALSOUND | S_C_1_FM_EMULATE_CLOCK, SYSTEM_CONFIGURATION_1);
#else
	/*
	 * pas_write(S_C_1_PCS_ENABLE, SYSTEM_CONFIGURATION_1);
	 */
	pas_write(S_C_1_PCS_ENABLE | S_C_1_PCS_STEREO | S_C_1_PCS_REALSOUND, SYSTEM_CONFIGURATION_1);
#endif
	pas_write(0x18, SYSTEM_CONFIGURATION_3);	/* ??? */

	pas_write(F_F_MIXER_UNMUTE | 0x01, FILTER_FREQUENCY);	/* Sets mute off and *
								 * selects filter rate *
								 * of 17.897 kHz */
	pas_write(8, PRESCALE_DIVIDER);

	mix_write(P_M_MV508_ADDRESS | 5, PARALLEL_MIXER);
	mix_write(5, PARALLEL_MIXER);

#if defined(CONFIG_SB_EMULATION) && defined(CONFIG_SB)

	{
		struct address_info *sb_config;

		if ((sb_config = sound_getconf(SNDCARD_SB))) {
			u_char   irq_dma;

			/*
			 * Turn on Sound Blaster compatibility
			 */
			/*
			 * bit 1 = SB emulation
			 */
			/*
			 * bit 0 = MPU401 emulation (CDPC only :-( )
			 */
			pas_write(0x02, COMPATIBILITY_ENABLE);

			/*
			 * "Emulation address"
			 */
			pas_write((sb_config->io_base >> 4) & 0x0f, EMULATION_ADDRESS);

			if (!E_C_SB_DMA_translate[sb_config->dma])
				printf("\n\nPAS16 Warning: Invalid SB DMA %d\n\n",
				       sb_config->dma);

			if (!E_C_SB_IRQ_translate[sb_config->irq])
				printf("\n\nPAS16 Warning: Invalid SB IRQ %d\n\n",
				       sb_config->irq);

			irq_dma = E_C_SB_DMA_translate[sb_config->dma] |
				E_C_SB_IRQ_translate[sb_config->irq];

			pas_write(irq_dma, EMULATION_CONFIGURATION);
		}
	}
#else
	pas_write(0x00, COMPATIBILITY_ENABLE);
#endif

	if (!ok)
		pas2_msg("Driver not enabled");

	return ok;
}

int
detect_pas_hw(struct address_info * hw_config)
{
	u_char   board_id, foo;

	/*
	 * WARNING: Setting an option like W:1 or so that disables warm boot
	 * reset of the card will screw up this detect code something fierce.
	 * Adding code to handle this means possibly interfering with other
	 * cards on the bus if you have something on base port 0x388. SO be
	 * forewarned.
	 */

	outb(MASTER_DECODE, 0xBC);	/* Talk to first board */
	outb(MASTER_DECODE, hw_config->io_base >> 2);	/* Set base address */
	translat_code = PAS_DEFAULT_BASE ^ hw_config->io_base;
	pas_write(1, WAIT_STATE);	/* One wait-state */

	board_id = pas_read(INTERRUPT_MASK);

	if (board_id == 0xff)
		return 0;

	/*
	 * We probably have a PAS-series board, now check for a PAS2-series
	 * board by trying to change the board revision bits. PAS2-series
	 * hardware won't let you do this - the bits are read-only.
	 */

	foo = board_id ^ 0xe0;

	pas_write(foo, INTERRUPT_MASK);
	foo = inb(INTERRUPT_MASK);
	pas_write(board_id, INTERRUPT_MASK);

	if (board_id != foo)	/* Not a PAS2 */
		return 0;

	pas_model = pas_read(CHIP_REV);

	return pas_model;
}

void
attach_pas_card(struct address_info * hw_config)
{
	pas_irq = hw_config->irq;
	pas_osp = hw_config->osp;

	if (detect_pas_hw(hw_config)) {

		if ((pas_model = pas_read(CHIP_REV))) {
			char            temp[100];

			sprintf(temp,
			      "%s rev %d", pas_model_names[(int) pas_model],
				pas_read(BOARD_REV_ID));
			conf_printf(temp, hw_config);
		}
		if (config_pas_hw(hw_config)) {

#ifdef CONFIG_AUDIO
			pas_pcm_init(hw_config);
#endif

#if defined(CONFIG_SB_EMULATION) && defined(CONFIG_SB)

			sb_dsp_disable_midi();	/* The SB emulation don't
						 * support * midi */
#endif


#ifdef CONFIG_MIDI
			pas_midi_init();
#endif
			pas_init_mixer();
		}
	}
}

int
probe_pas(struct address_info * hw_config)
{
	pas_osp = hw_config->osp;
	return detect_pas_hw(hw_config);
}

#endif
