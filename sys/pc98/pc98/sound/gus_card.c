/*
 * sound/gus_card.c
 *
 * Detection routine for the Gravis Ultrasound.
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

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_GUS)

#include <i386/isa/sound/gus_hw.h>

int             gus_base, gus_irq, gus_dma;
extern int      gus_wave_volume;
extern int      gus_pcm_volume;
extern int      have_gus_max;

long
attach_gus_card (long mem_start, struct address_info *hw_config)
{
  int             io_addr;

  snd_set_irq_handler (hw_config->irq, gusintr, "Gravis Ultrasound");

  if (gus_wave_detect (hw_config->io_base))	/*
						 * Try first the default
						 */
    {
      mem_start = gus_wave_init (mem_start, hw_config->irq, hw_config->dma,
				 hw_config->dma_read);
#ifndef EXCLUDE_MIDI
      mem_start = gus_midi_init (mem_start);
#endif
#ifndef EXCLUDE_SEQUENCER
      sound_timer_init (hw_config->io_base + 8);
#endif
      return mem_start;
    }

#ifndef EXCLUDE_GUS_IODETECT

  /*
   * Look at the possible base addresses (0x2X0, X=1, 2, 3, 4, 5, 6)
   */

  for (io_addr = 0x210; io_addr <= 0x260; io_addr += 0x10)
    if (io_addr != hw_config->io_base)	/*
					 * Already tested
					 */
      if (gus_wave_detect (io_addr))
	{
	  printk (" WARNING! GUS found at %x, config was %x ", io_addr, hw_config->io_base);
	  mem_start = gus_wave_init (mem_start, hw_config->irq, hw_config->dma,
				     hw_config->dma_read);
#ifndef EXCLUDE_MIDI
	  mem_start = gus_midi_init (mem_start);
#endif
#ifndef EXCLUDE_SEQUENCER
	  sound_timer_init (io_addr + 8);
#endif
	  return mem_start;
	}

#endif

  return mem_start;		/*
				 * Not detected
				 */
}

int
probe_gus (struct address_info *hw_config)
{
  int             io_addr;

  if (gus_wave_detect (hw_config->io_base))
    return 1;

#ifndef EXCLUDE_GUS_IODETECT

  /*
   * Look at the possible base addresses (0x2X0, X=1, 2, 3, 4, 5, 6)
   */

  for (io_addr = 0x210; io_addr <= 0x260; io_addr += 0x10)
    if (io_addr != hw_config->io_base)	/*
					 * Already tested
					 */
      if (gus_wave_detect (io_addr))
	return 1;

#endif

  return 0;
}

void
gusintr (INT_HANDLER_PARMS (irq, dummy))
{
  unsigned char   src;

#ifdef linux
  sti ();
#endif

#ifndef EXCLUDE_GUSMAX
  if (have_gus_max)
# if defined(__FreeBSD__)
    ad1848_interrupt (INT_HANDLER_CALL (gus_irq));
# else
    ad1848_interrupt (INT_HANDLER_CALL (irq));
# endif
#endif

  while (1)
    {
      if (!(src = INB (u_IrqStatus)))
	return;

      if (src & DMA_TC_IRQ)
	{
	  guswave_dma_irq ();
	}

      if (src & (MIDI_TX_IRQ | MIDI_RX_IRQ))
	{
#ifndef EXCLUDE_MIDI
	  gus_midi_interrupt (0);
#endif
	}

      if (src & (GF1_TIMER1_IRQ | GF1_TIMER2_IRQ))
	{
#ifndef EXCLUDE_SEQUENCER
	  sound_timer_interrupt ();
#else
	  gus_write8 (0x45, 0);	/* Stop timers */
#endif
	}

      if (src & (WAVETABLE_IRQ | ENVELOPE_IRQ))
	{
	  gus_voice_irq ();
	}
    }
}

#endif

/*
 * Some extra code for the 16 bit sampling option
 */
#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_GUS16)

int
probe_gus_db16 (struct address_info *hw_config)
{
  return ad1848_detect (hw_config->io_base);
}

long
attach_gus_db16 (long mem_start, struct address_info *hw_config)
{
  gus_pcm_volume = 100;
  gus_wave_volume = 90;

  ad1848_init ("GUS 16 bit sampling", hw_config->io_base,
	       hw_config->irq,
	       hw_config->dma,
	       hw_config->dma);
  return mem_start;
}

#endif
