
/* 
 * sound/gus_wave.c
 * 
 * Driver for the Gravis UltraSound wave table synth.
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

#include "sound_config.h"
#ifdef linux
#include <linux/ultrasound.h>
#elif __FreeBSD__
#include <machine/ultrasound.h>
#else
#include "ultrasound.h"
#endif
#include "gus_hw.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_GUS)

#define MAX_SAMPLE	128
#define MAX_PATCH	256

struct voice_info
  {
    unsigned long   orig_freq;
    unsigned long   current_freq;
    unsigned long   mode;
    int             bender;
    int             bender_range;
    int             panning;
    int             midi_volume;
    unsigned int    initial_volume;
    unsigned int    current_volume;
    int             loop_irq_mode, loop_irq_parm;
#define LMODE_FINISH		1
#define LMODE_PCM		2
#define LMODE_PCM_STOP		3
    int             volume_irq_mode, volume_irq_parm;
#define VMODE_HALT		1
#define VMODE_ENVELOPE		2
#define VMODE_START_NOTE	3

    int             env_phase;
    unsigned char   env_rate[6];
    unsigned char   env_offset[6];

    /* 
     * Volume computation parameters for gus_adagio_vol()
     */
    int             main_vol, expression_vol, patch_vol;

    /* Variables for "Ultraclick" removal */
    int             dev_pending, note_pending, volume_pending, sample_pending;
    char            kill_pending;
    long            offset_pending;

  };

extern int      gus_base;
extern int      gus_irq, gus_dma;
extern char    *snd_raw_buf[MAX_DSP_DEV][DSP_BUFFCOUNT];
extern unsigned long snd_raw_buf_phys[MAX_DSP_DEV][DSP_BUFFCOUNT];
extern int      snd_raw_count[MAX_DSP_DEV];
static long     gus_mem_size = 0;
static long     free_mem_ptr = 0;
static int      gus_busy = 0;
static int      nr_voices = 0;
static int      gus_devnum = 0;
static int      volume_base, volume_scale, volume_method;
static int      gus_line_vol = 100, gus_mic_vol = 0;
static int      gus_recmask = SOUND_MASK_MIC;
static int      recording_active = 0;

#define VOL_METHOD_ADAGIO	1
int             gus_wave_volume = 60;
int             gus_pcm_volume = 80;
static unsigned char mix_image = 0x00;

/* 
 * Current version of this driver doesn't allow synth and PCM functions
 * at the same time. The active_device specifies the active driver
 */
static int      active_device = 0;

#define GUS_DEV_WAVE		1	/* 
					   * * * Wave table synth   */
#define GUS_DEV_PCM_DONE	2	/* 
					   * * * PCM device, transfer done   */
#define GUS_DEV_PCM_CONTINUE	3	/* 
					   * * * PCM device, transfer the
					   * second * * * chn   */

static int      gus_sampling_speed;
static int      gus_sampling_channels;
static int      gus_sampling_bits;

DEFINE_WAIT_QUEUE (dram_sleeper, dram_sleep_flag);

/* 
 * Variables and buffers for PCM output
 */
#define MAX_PCM_BUFFERS		(32*MAX_REALTIME_FACTOR)	/* 
								   * * * Don't
								   * * * change 
								   * 
								 */

static int      pcm_bsize,	/* 
				 * Current blocksize 
				 */
                pcm_nblk,	/* 
				 * Current # of blocks 
				 */
                pcm_banksize;	/* 


				 * *  * * # bytes allocated for channels   */
static int      pcm_datasize[MAX_PCM_BUFFERS];	/* 


						 * *  * * Actual # of bytes
						 * in blk  *  */
static volatile int pcm_head, pcm_tail, pcm_qlen;	/* 


							 * *  * * DRAM queue
							 *  */
static volatile int pcm_active;
static int      pcm_opened = 0;
static int      pcm_current_dev;
static int      pcm_current_block;
static unsigned long pcm_current_buf;
static int      pcm_current_count;
static int      pcm_current_intrflag;

struct voice_info voices[32];

static int      freq_div_table[] =
{
  44100,			/* 
				 * 14 
				 */
  41160,			/* 
				 * 15 
				 */
  38587,			/* 
				 * 16 
				 */
  36317,			/* 
				 * 17 
				 */
  34300,			/* 
				 * 18 
				 */
  32494,			/* 
				 * 19 
				 */
  30870,			/* 
				 * 20 
				 */
  29400,			/* 
				 * 21 
				 */
  28063,			/* 
				 * 22 
				 */
  26843,			/* 
				 * 23 
				 */
  25725,			/* 
				 * 24 
				 */
  24696,			/* 
				 * 25 
				 */
  23746,			/* 
				 * 26 
				 */
  22866,			/* 
				 * 27 
				 */
  22050,			/* 
				 * 28 
				 */
  21289,			/* 
				 * 29 
				 */
  20580,			/* 
				 * 30 
				 */
  19916,			/* 
				 * 31 
				 */
  19293				/* 
				 * 32 
				 */
};

static struct patch_info *samples;
static long     sample_ptrs[MAX_SAMPLE + 1];
static int      sample_map[32];
static int      free_sample;


static int      patch_table[MAX_PATCH];
static int      patch_map[32];

static struct synth_info gus_info =
{"Gravis UltraSound", 0, SYNTH_TYPE_SAMPLE, SAMPLE_TYPE_GUS, 0, 16, 0, MAX_PATCH};

static void     gus_poke (long addr, unsigned char data);
static void     compute_and_set_volume (int voice, int volume, int ramp_time);
extern unsigned short gus_adagio_vol (int vel, int mainv, int xpn, int voicev);
static void     compute_volume (int voice, int volume);
static void     do_volume_irq (int voice);
static void     set_input_volumes (void);

#define	INSTANT_RAMP		-1	/* 
					   * * * Dont use ramping   */
#define FAST_RAMP		0	/* 
					   * * * Fastest possible ramp   */

static void
reset_sample_memory (void)
{
  int             i;

  for (i = 0; i <= MAX_SAMPLE; i++)
    sample_ptrs[i] = -1;
  for (i = 0; i < 32; i++)
    sample_map[i] = -1;
  for (i = 0; i < 32; i++)
    patch_map[i] = -1;

  gus_poke (0, 0);		/* 
				 * Put silence here 
				 */
  gus_poke (1, 0);

  free_mem_ptr = 2;
  free_sample = 0;

  for (i = 0; i < MAX_PATCH; i++)
    patch_table[i] = -1;
}

void
gus_delay (void)
{
  int             i;

  for (i = 0; i < 7; i++)
    INB (u_DRAMIO);
}

static void
gus_poke (long addr, unsigned char data)
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  OUTB (0x43, u_Command);
  OUTB (addr & 0xff, u_DataLo);
  OUTB ((addr >> 8) & 0xff, u_DataHi);

  OUTB (0x44, u_Command);
  OUTB ((addr >> 16) & 0xff, u_DataHi);
  OUTB (data, u_DRAMIO);
  RESTORE_INTR (flags);
}

static unsigned char
gus_peek (long addr)
{
  unsigned long   flags;
  unsigned char   tmp;

  DISABLE_INTR (flags);
  OUTB (0x43, u_Command);
  OUTB (addr & 0xff, u_DataLo);
  OUTB ((addr >> 8) & 0xff, u_DataHi);

  OUTB (0x44, u_Command);
  OUTB ((addr >> 16) & 0xff, u_DataHi);
  tmp = INB (u_DRAMIO);
  RESTORE_INTR (flags);

  return tmp;
}

void
gus_write8 (int reg, unsigned int data)
{
  unsigned long   flags;

  DISABLE_INTR (flags);

  OUTB (reg, u_Command);
  OUTB ((unsigned char) (data & 0xff), u_DataHi);

  RESTORE_INTR (flags);
}

unsigned char
gus_read8 (int reg)
{
  unsigned long   flags;
  unsigned char   val;

  DISABLE_INTR (flags);
  OUTB (reg | 0x80, u_Command);
  val = INB (u_DataHi);
  RESTORE_INTR (flags);

  return val;
}

unsigned char
gus_look8 (int reg)
{
  unsigned long   flags;
  unsigned char   val;

  DISABLE_INTR (flags);
  OUTB (reg, u_Command);
  val = INB (u_DataHi);
  RESTORE_INTR (flags);

  return val;
}

void
gus_write16 (int reg, unsigned int data)
{
  unsigned long   flags;

  DISABLE_INTR (flags);

  OUTB (reg, u_Command);

  OUTB ((unsigned char) (data & 0xff), u_DataLo);
  OUTB ((unsigned char) ((data >> 8) & 0xff), u_DataHi);

  RESTORE_INTR (flags);
}

unsigned short
gus_read16 (int reg)
{
  unsigned long   flags;
  unsigned char   hi, lo;

  DISABLE_INTR (flags);

  OUTB (reg | 0x80, u_Command);

  lo = INB (u_DataLo);
  hi = INB (u_DataHi);

  RESTORE_INTR (flags);

  return ((hi << 8) & 0xff00) | lo;
}

void
gus_write_addr (int reg, unsigned long address, int is16bit)
{
  unsigned long   hold_address;

  if (is16bit)
    {
      /* 
       * Special processing required for 16 bit patches
       */

      hold_address = address;
      address = address >> 1;
      address &= 0x0001ffffL;
      address |= (hold_address & 0x000c0000L);
    }

  gus_write16 (reg, (unsigned short) ((address >> 7) & 0xffff));
  gus_write16 (reg + 1, (unsigned short) ((address << 9) & 0xffff));
}

static void
gus_select_voice (int voice)
{
  if (voice < 0 || voice > 31)
    return;

  OUTB (voice, u_Voice);
}

static void
gus_select_max_voices (int nvoices)
{
  if (nvoices < 14)
    nvoices = 14;
  if (nvoices > 32)
    nvoices = 32;

  nr_voices = nvoices;

  gus_write8 (0x0e, (nvoices - 1) | 0xc0);
}

static void
gus_voice_on (unsigned int mode)
{
  gus_write8 (0x00, (unsigned char) (mode & 0xfc));
  gus_delay ();
  gus_write8 (0x00, (unsigned char) (mode & 0xfc));
}

static void
gus_voice_off (void)
{
  gus_write8 (0x00, gus_read8 (0x00) | 0x03);
}

static void
gus_voice_mode (unsigned int m)
{
  unsigned char   mode = (unsigned char) (m & 0xff);

  gus_write8 (0x00, (gus_read8 (0x00) & 0x03) | (mode & 0xfc));		/* 
									 * Don't 
									 * start 
									 * or 
									 * stop
									 * *
									 * voice 
									 */
  gus_delay ();
  gus_write8 (0x00, (gus_read8 (0x00) & 0x03) | (mode & 0xfc));
}

static void
gus_voice_freq (unsigned long freq)
{
  unsigned long   divisor = freq_div_table[nr_voices - 14];
  unsigned short  fc;

  fc = (unsigned short) (((freq << 9) + (divisor >> 1)) / divisor);
  fc = fc << 1;

  gus_write16 (0x01, fc);
}

static void
gus_voice_volume (unsigned int vol)
{
  gus_write8 (0x0d, 0x03);	/* 
				 * Stop ramp before setting volume 
				 */
  gus_write16 (0x09, (unsigned short) (vol << 4));
}

static void
gus_voice_balance (unsigned int balance)
{
  gus_write8 (0x0c, (unsigned char) (balance & 0xff));
}

static void
gus_ramp_range (unsigned int low, unsigned int high)
{
  gus_write8 (0x07, (unsigned char) ((low >> 4) & 0xff));
  gus_write8 (0x08, (unsigned char) ((high >> 4) & 0xff));
}

static void
gus_ramp_rate (unsigned int scale, unsigned int rate)
{
  gus_write8 (0x06, (unsigned char) (((scale & 0x03) << 6) | (rate & 0x3f)));
}

static void
gus_rampon (unsigned int m)
{
  unsigned char   mode = (unsigned char) (m & 0xff);

  gus_write8 (0x0d, mode & 0xfc);
  gus_delay ();
  gus_write8 (0x0d, mode & 0xfc);
}

static void
gus_ramp_mode (unsigned int m)
{
  unsigned char   mode = (unsigned char) (m & 0xff);

  gus_write8 (0x0d, (gus_read8 (0x0d) & 0x03) | (mode & 0xfc));		/* 
									 * Don't 
									 * start 
									 * or 
									 * stop
									 * *
									 * ramping 
									 */
  gus_delay ();
  gus_write8 (0x0d, (gus_read8 (0x0d) & 0x03) | (mode & 0xfc));
}

static void
gus_rampoff (void)
{
  gus_write8 (0x0d, 0x03);
}

static void
gus_set_voice_pos (int voice, long position)
{
  int             sample_no;

  if ((sample_no = sample_map[voice]) != -1)
    if (position < samples[sample_no].len)
      if (voices[voice].volume_irq_mode == VMODE_START_NOTE)
	voices[voice].offset_pending = position;
      else
	gus_write_addr (0x0a, sample_ptrs[sample_no] + position,
			samples[sample_no].mode & WAVE_16_BITS);
}

static void
gus_voice_init (int voice)
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  gus_select_voice (voice);
  gus_voice_volume (0);
  gus_write_addr (0x0a, 0, 0);	/* 
				 * Set current position to 0 
				 */
  gus_write8 (0x00, 0x03);	/* 
				 * Voice off 
				 */
  gus_write8 (0x0d, 0x03);	/* 
				 * Ramping off 
				 */
  RESTORE_INTR (flags);

}

static void
gus_voice_init2 (int voice)
{
  voices[voice].panning = 0;
  voices[voice].mode = 0;
  voices[voice].orig_freq = 20000;
  voices[voice].current_freq = 20000;
  voices[voice].bender = 0;
  voices[voice].bender_range = 200;
  voices[voice].initial_volume = 0;
  voices[voice].current_volume = 0;
  voices[voice].loop_irq_mode = 0;
  voices[voice].loop_irq_parm = 0;
  voices[voice].volume_irq_mode = 0;
  voices[voice].volume_irq_parm = 0;
  voices[voice].env_phase = 0;
  voices[voice].main_vol = 127;
  voices[voice].patch_vol = 127;
  voices[voice].expression_vol = 127;
  voices[voice].sample_pending = -1;
}

static void
step_envelope (int voice)
{
  unsigned        vol, prev_vol, phase;
  unsigned char   rate;

  if (voices[voice].mode & WAVE_SUSTAIN_ON && voices[voice].env_phase == 2)
    {
      gus_rampoff ();
      return;			/* 
				 * Sustain 
				 */
    }

  if (voices[voice].env_phase >= 5)
    {
      /* 
       * Shoot the voice off
       */

      gus_voice_init (voice);
      return;
    }

  prev_vol = voices[voice].current_volume;
  gus_voice_volume (prev_vol);
  phase = ++voices[voice].env_phase;

  compute_volume (voice, voices[voice].midi_volume);

  vol = voices[voice].initial_volume * voices[voice].env_offset[phase] / 255;
  rate = voices[voice].env_rate[phase];
  gus_write8 (0x06, rate);	/* 
				 * Ramping rate 
				 */

  voices[voice].volume_irq_mode = VMODE_ENVELOPE;

  if (((vol - prev_vol) / 64) == 0)	/* 
					 * No significant volume change 
					 */
    {
      step_envelope (voice);	/* 
				 * Continue with the next phase 
				 */
      return;
    }

  if (vol > prev_vol)
    {
      if (vol >= (4096 - 64))
	vol = 4096 - 65;
      gus_ramp_range (0, vol);
      gus_rampon (0x20);	/* 
				 * Increasing, irq 
				 */
    }
  else
    {
      if (vol <= 64)
	vol = 65;
      gus_ramp_range (vol, 4030);
      gus_rampon (0x60);	/* 
				 * Decreasing, irq 
				 */
    }
  voices[voice].current_volume = vol;
}

static void
init_envelope (int voice)
{
  voices[voice].env_phase = -1;
  voices[voice].current_volume = 64;

  step_envelope (voice);
}

static void
start_release (int voice)
{
  if (gus_read8 (0x00) & 0x03)
    return;			/* 
				 * Voice already stopped 
				 */

  voices[voice].env_phase = 2;	/* 
				 * Will be incremented by step_envelope 
				 */

  voices[voice].current_volume =
    voices[voice].initial_volume =
    gus_read16 (0x09) >> 4;	/* 
				 * Get current volume 
				 */

  voices[voice].mode &= ~WAVE_SUSTAIN_ON;
  gus_rampoff ();
  step_envelope (voice);
}

static void
gus_voice_fade (int voice)
{
  int             instr_no = sample_map[voice], is16bits;

  if (instr_no < 0 || instr_no > MAX_SAMPLE)
    {
      gus_write8 (0x00, 0x03);	/* 
				 * Hard stop 
				 */
      return;
    }

  is16bits = (samples[instr_no].mode & WAVE_16_BITS) ? 1 : 0;	/* 
								 * 8 or 16
								 * bit
								 * samples 
								 */

  if (voices[voice].mode & WAVE_ENVELOPES)
    {
      start_release (voice);
      return;
    }

  /* 
   * Ramp the volume down but not too quickly.
   */
  if ((gus_read16 (0x09) >> 4) < 100)	/* 
					 * Get current volume 
					 */
    {
      gus_voice_off ();
      gus_rampoff ();
      gus_voice_init (voice);
      return;
    }

  gus_ramp_range (65, 4030);
  gus_ramp_rate (2, 4);
  gus_rampon (0x40 | 0x20);	/* 
				 * Down, once, irq 
				 */
  voices[voice].volume_irq_mode = VMODE_HALT;
}

static void
gus_reset (void)
{
  int             i;

  gus_select_max_voices (24);
  volume_base = 3071;
  volume_scale = 4;
  volume_method = VOL_METHOD_ADAGIO;

  for (i = 0; i < 32; i++)
    {
      gus_voice_init (i);	/* 
				 * Turn voice off 
				 */
      gus_voice_init2 (i);
    }

  INB (u_Status);		/* 
				 * Touch the status register 
				 */

  gus_look8 (0x41);		/* 
				 * Clear any pending DMA IRQs 
				 */
  gus_look8 (0x49);		/* 
				 * Clear any pending sample IRQs 
				 */
  gus_read8 (0x0f);		/* 
				 * Clear pending IRQs 
				 */

}

static void
gus_initialize (void)
{
  unsigned long   flags;
  unsigned char   dma_image, irq_image, tmp;

  static unsigned char gus_irq_map[16] =
  {0, 0, 1, 3, 0, 2, 0, 4, 0, 0, 0, 5, 6, 0, 0, 7};

  static unsigned char gus_dma_map[8] =
  {0, 1, 0, 2, 0, 3, 4, 5};

  DISABLE_INTR (flags);

  gus_write8 (0x4c, 0);		/* 
				 * Reset GF1 
				 */
  gus_delay ();
  gus_delay ();

  gus_write8 (0x4c, 1);		/* 
				 * Release Reset 
				 */
  gus_delay ();
  gus_delay ();

  /* 
   * Clear all interrupts
   */

  gus_write8 (0x41, 0);		/* 
				 * DMA control 
				 */
  gus_write8 (0x45, 0);		/* 
				 * Timer control 
				 */
  gus_write8 (0x49, 0);		/* 
				 * Sample control 
				 */

  gus_select_max_voices (24);

  INB (u_Status);		/* 
				 * Touch the status register 
				 */

  gus_look8 (0x41);		/* 
				 * Clear any pending DMA IRQs 
				 */
  gus_look8 (0x49);		/* 
				 * Clear any pending sample IRQs 
				 */
  gus_read8 (0x0f);		/* 
				 * Clear pending IRQs 
				 */

  gus_reset ();			/* 
				 * Resets all voices 
				 */

  gus_look8 (0x41);		/* 
				 * Clear any pending DMA IRQs 
				 */
  gus_look8 (0x49);		/* 
				 * Clear any pending sample IRQs 
				 */
  gus_read8 (0x0f);		/* 
				 * Clear pending IRQs 
				 */

  gus_write8 (0x4c, 7);		/* 
				 * Master reset | DAC enable | IRQ enable 
				 */

  /* 
   * Set up for Digital ASIC
   */

  OUTB (0x05, gus_base + 0x0f);

  mix_image |= 0x02;		/* 
				 * Disable line out 
				 */
  OUTB (mix_image, u_Mixer);

  OUTB (0x00, u_IRQDMAControl);

  OUTB (0x00, gus_base + 0x0f);

  /* 
   * Now set up the DMA and IRQ interface
   * 
   * The GUS supports two IRQs and two DMAs.
   * 
   * Just one DMA channel is used. This prevents simultaneous ADC and DAC.
   * Adding this support requires significant changes to the dmabuf.c, dsp.c
   * and audio.c also.
   */

  irq_image = 0;
  tmp = gus_irq_map[gus_irq];
  if (!tmp)
    printk ("Warning! GUS IRQ not selected\n");
  irq_image |= tmp;
  irq_image |= 0x40;		/* 
				 * Combine IRQ1 (GF1) and IRQ2 (Midi) 
				 */

  dma_image = 0x40;		/* 
				 * Combine DMA1 (DRAM) and IRQ2 (ADC) 
				 */
  tmp = gus_dma_map[gus_dma];
  if (!tmp)
    printk ("Warning! GUS DMA not selected\n");
  dma_image |= tmp;

  /* 
   * For some reason the IRQ and DMA addresses must be written twice
   */

  /* 
   * Doing it first time 
   */

  OUTB (mix_image, u_Mixer);	/* 
				 * Select DMA control 
				 */
  OUTB (dma_image | 0x80, u_IRQDMAControl);	/* 
						 * Set DMA address 
						 */

  OUTB (mix_image | 0x40, u_Mixer);	/* 
					 * Select IRQ control 
					 */
  OUTB (irq_image, u_IRQDMAControl);	/* 
					 * Set IRQ address 
					 */

  /* 
   * Doing it second time 
   */

  OUTB (mix_image, u_Mixer);	/* 
				 * Select DMA control 
				 */
  OUTB (dma_image, u_IRQDMAControl);	/* 
					 * Set DMA address 
					 */

  OUTB (mix_image | 0x40, u_Mixer);	/* 
					 * Select IRQ control 
					 */
  OUTB (irq_image, u_IRQDMAControl);	/* 
					 * Set IRQ address 
					 */

  gus_select_voice (0);		/* 
				 * This disables writes to IRQ/DMA reg 
				 */

  mix_image &= ~0x02;		/* 
				 * Enable line out 
				 */
  mix_image |= 0x08;		/* 
				 * Enable IRQ 
				 */
  OUTB (mix_image, u_Mixer);	/* 
				 * Turn mixer channels on 
				 * Note! Mic in is left off.
				 */

  gus_select_voice (0);		/* 
				 * This disables writes to IRQ/DMA reg 
				 */

  gusintr (0);			/* 
				 * Serve pending interrupts 
				 */
  RESTORE_INTR (flags);
}

int
gus_wave_detect (int baseaddr)
{
  unsigned long   i;
  unsigned long   loc;

  gus_base = baseaddr;

  gus_write8 (0x4c, 0);		/* Reset GF1 */
  gus_delay ();
  gus_delay ();

  gus_write8 (0x4c, 1);		/* Release Reset */
  gus_delay ();
  gus_delay ();

  /* See if there is first block there.... */
  gus_poke (0L, 0xaa);
  if (gus_peek (0L) != 0xaa)
    return (0);

  /* Now zero it out so that I can check for mirroring .. */
  gus_poke (0L, 0x00);
  for (i = 1L; i < 1024L; i++)
    {
      int             n, failed;

      /* check for mirroring ... */
      if (gus_peek (0L) != 0)
	break;
      loc = i << 10;

      for (n = loc - 1, failed = 0; n <= loc; n++)
	{
	  gus_poke (loc, 0xaa);
	  if (gus_peek (loc) != 0xaa)
	    failed = 1;

	  gus_poke (loc, 0x55);
	  if (gus_peek (loc) != 0x55)
	    failed = 1;
	}

      if (failed)
	break;
    }
  gus_mem_size = i << 10;
  return 1;
}

static int
guswave_ioctl (int dev,
	       unsigned int cmd, unsigned int arg)
{

  switch (cmd)
    {
    case SNDCTL_SYNTH_INFO:
      gus_info.nr_voices = nr_voices;
      IOCTL_TO_USER ((char *) arg, 0, &gus_info, sizeof (gus_info));
      return 0;
      break;

    case SNDCTL_SEQ_RESETSAMPLES:
      reset_sample_memory ();
      return 0;
      break;

    case SNDCTL_SEQ_PERCMODE:
      return 0;
      break;

    case SNDCTL_SYNTH_MEMAVL:
      return gus_mem_size - free_mem_ptr - 32;

    default:
      return RET_ERROR (EINVAL);
    }
}

static int
guswave_set_instr (int dev, int voice, int instr_no)
{
  int             sample_no;

  if (instr_no < 0 || instr_no > MAX_PATCH)
    return RET_ERROR (EINVAL);

  if (voice < 0 || voice > 31)
    return RET_ERROR (EINVAL);

  if (voices[voice].volume_irq_mode == VMODE_START_NOTE)
    {
      voices[voice].sample_pending = instr_no;
      return 0;
    }

  sample_no = patch_table[instr_no];
  patch_map[voice] = -1;

  if (sample_no < 0)
    {
      printk ("GUS: Undefined patch %d for voice %d\n", instr_no, voice);
      return RET_ERROR (EINVAL);	/* 
					 * Patch not defined 
					 */
    }

  if (sample_ptrs[sample_no] == -1)	/* 
					 * Sample not loaded 
					 */
    {
      printk ("GUS: Sample #%d not loaded for patch %d (voice %d)\n", sample_no, instr_no, voice);
      return RET_ERROR (EINVAL);
    }

  sample_map[voice] = sample_no;
  patch_map[voice] = instr_no;
  return 0;
}

static int
#ifdef FUTURE_VERSION
guswave_kill_note (int dev, int voice, int note, int velocity)
#else
guswave_kill_note (int dev, int voice, int velocity)
#endif
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  if (voices[voice].volume_irq_mode == VMODE_START_NOTE)
    voices[voice].kill_pending = 1;
  else
    {
      gus_select_voice (voice);
      gus_voice_fade (voice);
    }
  RESTORE_INTR (flags);

  return 0;
}

static void
guswave_aftertouch (int dev, int voice, int pressure)
{
  short           lo_limit, hi_limit;
  unsigned long   flags;

  return;			/* 
				 * Currently disabled 
				 */

  if (voice < 0 || voice > 31)
    return;

  if (voices[voice].mode & WAVE_ENVELOPES && voices[voice].env_phase != 2)
    return;			/* 
				 * Don't mix with envelopes 
				 */

  if (pressure < 32)
    {
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_rampoff ();
      compute_and_set_volume (voice, 255, 0);	/* 
						 * Back to original volume 
						 */
      RESTORE_INTR (flags);
      return;
    }

  hi_limit = voices[voice].current_volume;
  lo_limit = hi_limit * 99 / 100;
  if (lo_limit < 65)
    lo_limit = 65;

  DISABLE_INTR (flags);
  gus_select_voice (voice);
  if (hi_limit > (4095 - 65))
    {
      hi_limit = 4095 - 65;
      gus_voice_volume (hi_limit);
    }
  gus_ramp_range (lo_limit, hi_limit);
  gus_ramp_rate (3, 8);
  gus_rampon (0x58);		/* 
				 * Bidirectional, Down, Loop 
				 */
  RESTORE_INTR (flags);
}

static void
guswave_panning (int dev, int voice, int value)
{
  if (voice >= 0 || voice < 32)
    voices[voice].panning = value;
}

static void
compute_volume (int voice, int volume)
{
  if (volume < 128)
    voices[voice].midi_volume = volume;

  switch (volume_method)
    {
    case VOL_METHOD_ADAGIO:
      voices[voice].initial_volume =
	gus_adagio_vol (voices[voice].midi_volume, voices[voice].main_vol,
			voices[voice].expression_vol,
			voices[voice].patch_vol);
      break;

    default:
      voices[voice].initial_volume = volume_base +
	(voices[voice].midi_volume * volume_scale);
    }

  if (voices[voice].initial_volume > 4030)
    voices[voice].initial_volume = 4030;
}

static void
compute_and_set_volume (int voice, int volume, int ramp_time)
{
  int             current, target, rate;
  unsigned long   flags;

  DISABLE_INTR (flags);
/* 
 * CAUTION! Interrupts disabled. Enable them before returning
 */

  gus_select_voice (voice);

  compute_volume (voice, volume);
  voices[voice].current_volume = voices[voice].initial_volume;

  current = gus_read16 (0x09) >> 4;
  target = voices[voice].initial_volume;

  if (ramp_time == INSTANT_RAMP)
    {
      gus_rampoff ();
      gus_voice_volume (target);
      RESTORE_INTR (flags);
      return;
    }

  if (ramp_time == FAST_RAMP)
    rate = 63;
  else
    rate = 16;
  gus_ramp_rate (0, rate);

  if ((target - current) / 64 == 0)	/* 
					 * Too close 
					 */
    {
      gus_rampoff ();
      gus_voice_volume (target);
      RESTORE_INTR (flags);
      return;
    }

  if (target > current)
    {
      if (target > (4095 - 65))
	target = 4095 - 65;
      gus_ramp_range (current, target);
      gus_rampon (0x00);	/* 
				 * Ramp up, once, no irq 
				 */
    }
  else
    {
      if (target < 65)
	target = 65;

      gus_ramp_range (target, current);
      gus_rampon (0x40);	/* 
				 * Ramp down, once, no irq 
				 */
    }
  RESTORE_INTR (flags);
}

static void
dynamic_volume_change (int voice)
{
  unsigned char   status;
  unsigned long   flags;

  DISABLE_INTR (flags);
  gus_select_voice (voice);
  status = gus_read8 (0x00);	/* 
				 * Voice status 
				 */
  RESTORE_INTR (flags);

  if (status & 0x03)
    return;			/* 
				 * Voice not started 
				 */

  if (!(voices[voice].mode & WAVE_ENVELOPES))
    {
      compute_and_set_volume (voice, voices[voice].midi_volume, 1);
      return;
    }

  /* 
   * Voice is running and has envelopes.
   */

  DISABLE_INTR (flags);
  gus_select_voice (voice);
  status = gus_read8 (0x0d);	/* 
				 * Ramping status 
				 */
  RESTORE_INTR (flags);

  if (status & 0x03)		/* 
				 * Sustain phase? 
				 */
    {
      compute_and_set_volume (voice, voices[voice].midi_volume, 1);
      return;
    }

  if (voices[voice].env_phase < 0)
    return;

  compute_volume (voice, voices[voice].midi_volume);

#if 0				/* 
				   * * * Is this really required   */
  voices[voice].current_volume =
    gus_read16 (0x09) >> 4;	/* 
				 * Get current volume 
				 */

  voices[voice].env_phase--;
  step_envelope (voice);
#endif
}

static void
guswave_controller (int dev, int voice, int ctrl_num, int value)
{
  unsigned long   flags;
  unsigned long   freq;

  if (voice < 0 || voice > 31)
    return;

  switch (ctrl_num)
    {
    case CTRL_PITCH_BENDER:
      voices[voice].bender = value;

      if (voices[voice].volume_irq_mode != VMODE_START_NOTE)
	{
	  freq = compute_finetune (voices[voice].orig_freq, value, voices[voice].bender_range);
	  voices[voice].current_freq = freq;

	  DISABLE_INTR (flags);
	  gus_select_voice (voice);
	  gus_voice_freq (freq);
	  RESTORE_INTR (flags);
	}
      break;

    case CTRL_PITCH_BENDER_RANGE:
      voices[voice].bender_range = value;
      break;
#ifdef FUTURE_VERSION
    case CTL_EXPRESSION:
      value /= 128;
#endif
    case CTRL_EXPRESSION:
      volume_method = VOL_METHOD_ADAGIO;
      voices[voice].expression_vol = value;
      if (voices[voice].volume_irq_mode != VMODE_START_NOTE)
	dynamic_volume_change (voice);
      break;

#ifdef FUTURE_VERSION
    case CTL_PAN:
      voices[voice].panning = (value * 2) - 128;
      break;

    case CTL_MAIN_VOLUME:
      value = (value * 100) / 16383;
#endif

    case CTRL_MAIN_VOLUME:
      volume_method = VOL_METHOD_ADAGIO;
      voices[voice].main_vol = value;
      if (voices[voice].volume_irq_mode != VMODE_START_NOTE)
	dynamic_volume_change (voice);
      break;

    default:			/* 
				 * Ignore 
				 */
      break;
    }
}

static int
guswave_start_note2 (int dev, int voice, int note_num, int volume)
{
  int             sample, best_sample, best_delta, delta_freq;
  int             is16bits, samplep, patch, pan;
  unsigned long   note_freq, base_note, freq, flags;
  unsigned char   mode = 0;

  if (voice < 0 || voice > 31)
    {
      printk ("GUS: Invalid voice\n");
      return RET_ERROR (EINVAL);
    }

  if (note_num == 255)
    {
      if (voices[voice].mode & WAVE_ENVELOPES)
	{
	  voices[voice].midi_volume = volume;
	  dynamic_volume_change (voice);
	  return 0;
	}

      compute_and_set_volume (voice, volume, 1);
      return 0;
    }

  if ((patch = patch_map[voice]) == -1)
    {
      return RET_ERROR (EINVAL);
    }

  if ((samplep = patch_table[patch]) == -1)
    {
      return RET_ERROR (EINVAL);
    }

  note_freq = note_to_freq (note_num);

  /* 
   * Find a sample within a patch so that the note_freq is between low_note
   * and high_note.
   */
  sample = -1;

  best_sample = samplep;
  best_delta = 1000000;
  while (samplep >= 0 && sample == -1)
    {
      delta_freq = note_freq - samples[samplep].base_note;
      if (delta_freq < 0)
	delta_freq = -delta_freq;
      if (delta_freq < best_delta)
	{
	  best_sample = samplep;
	  best_delta = delta_freq;
	}
      if (samples[samplep].low_note <= note_freq && note_freq <= samples[samplep].high_note)
	sample = samplep;
      else
	samplep = samples[samplep].key;		/* 
						 * Follow link 
						 */
    }
  if (sample == -1)
    sample = best_sample;

  if (sample == -1)
    {
      printk ("GUS: Patch %d not defined for note %d\n", patch, note_num);
      return 0;			/* 
				 * Should play default patch ??? 
				 */
    }

  is16bits = (samples[sample].mode & WAVE_16_BITS) ? 1 : 0;	/* 
								 * 8 or 16
								 * bit
								 * samples 
								 */
  voices[voice].mode = samples[sample].mode;
  voices[voice].patch_vol = samples[sample].volume;

  if (voices[voice].mode & WAVE_ENVELOPES)
    {
      int             i;

      for (i = 0; i < 6; i++)
	{
	  voices[voice].env_rate[i] = samples[sample].env_rate[i];
	  voices[voice].env_offset[i] = samples[sample].env_offset[i];
	}
    }

  sample_map[voice] = sample;

  base_note = samples[sample].base_note / 100;	/* 
						 * To avoid overflows 
						 */
  note_freq /= 100;

  freq = samples[sample].base_freq * note_freq / base_note;

  voices[voice].orig_freq = freq;

  /* 
   * Since the pitch bender may have been set before playing the note, we
   * have to calculate the bending now.
   */

  freq = compute_finetune (voices[voice].orig_freq, voices[voice].bender, voices[voice].bender_range);
  voices[voice].current_freq = freq;

  pan = (samples[sample].panning + voices[voice].panning) / 32;
  pan += 7;
  if (pan < 0)
    pan = 0;
  if (pan > 15)
    pan = 15;

  if (samples[sample].mode & WAVE_16_BITS)
    {
      mode |= 0x04;		/* 
				 * 16 bits 
				 */
      if ((sample_ptrs[sample] >> 18) !=
	  ((sample_ptrs[sample] + samples[sample].len) >> 18))
	printk ("GUS: Sample address error\n");
    }

  /*************************************************************************
   *    CAUTION!        Interrupts disabled. Don't return before enabling
   *************************************************************************/

  DISABLE_INTR (flags);
  gus_select_voice (voice);
  gus_voice_off ();		/* 
				 * It may still be running 
				 */
  gus_rampoff ();
  if (voices[voice].mode & WAVE_ENVELOPES)
    {
      compute_volume (voice, volume);
      init_envelope (voice);
    }
  else
    compute_and_set_volume (voice, volume, 0);

  if (samples[sample].mode & WAVE_LOOP_BACK)
    gus_write_addr (0x0a, sample_ptrs[sample] + samples[sample].len -
		    voices[voice].offset_pending, is16bits);	/* Sample
								 * start=end */
  else
    gus_write_addr (0x0a, sample_ptrs[sample] + voices[voice].offset_pending,
		    is16bits);	/* Sample start=begin */

  if (samples[sample].mode & WAVE_LOOPING)
    {
      mode |= 0x08;		/* 
				 * Looping on 
				 */

      if (samples[sample].mode & WAVE_BIDIR_LOOP)
	mode |= 0x10;		/* 
				 * Bidirectional looping on 
				 */

      if (samples[sample].mode & WAVE_LOOP_BACK)
	{
	  gus_write_addr (0x0a,
			  sample_ptrs[sample] + samples[sample].loop_end -
			  voices[voice].offset_pending, is16bits);
	  mode |= 0x40;
	}

      gus_write_addr (0x02, sample_ptrs[sample] + samples[sample].loop_start, is16bits);	/* 
												 * Loop 
												 * start 
												 * location 
												 */
      gus_write_addr (0x04, sample_ptrs[sample] + samples[sample].loop_end, is16bits);	/* 
											 * Loop 
											 * end 
											 * location 
											 */
    }
  else
    {
      mode |= 0x20;		/* 
				 * Loop irq at the end 
				 */
      voices[voice].loop_irq_mode = LMODE_FINISH;	/* 
							 * Ramp it down at
							 * the * end 
							 */
      voices[voice].loop_irq_parm = 1;
      gus_write_addr (0x02, sample_ptrs[sample], is16bits);	/* 
								 * Loop start 
								 * location 
								 */
      gus_write_addr (0x04, sample_ptrs[sample] + samples[sample].len - 1, is16bits);	/* 
											 * Loop 
											 * end 
											 * location 
											 */
    }
  gus_voice_freq (freq);
  gus_voice_balance (pan);
  gus_voice_on (mode);
  RESTORE_INTR (flags);

  return 0;
}

/* 
 * * New guswave_start_note by Andrew J. Robinson attempts to minimize
 * clicking  * when the note playing on the voice is changed.  It uses volume 
 * ramping. */

static int
guswave_start_note (int dev, int voice, int note_num, int volume)
{
  long int        flags;
  int             mode;
  int             ret_val = 0;

  DISABLE_INTR (flags);
  if (note_num == 255)
    {
      if (voices[voice].volume_irq_mode == VMODE_START_NOTE)
	voices[voice].volume_pending = volume;
      else
	ret_val = guswave_start_note2 (dev, voice, note_num, volume);
    }
  else
    {
      gus_select_voice (voice);
      mode = gus_read8 (0x00);
      if (mode & 0x20)
	gus_write8 (0x00, mode & 0xdf);		/* No interrupt! */

      voices[voice].offset_pending = 0;
      voices[voice].kill_pending = 0;
      voices[voice].volume_irq_mode = 0;
      voices[voice].loop_irq_mode = 0;

      if (voices[voice].sample_pending >= 0)
	{
	  guswave_set_instr (voices[voice].dev_pending, voice,
			     voices[voice].sample_pending);
	  voices[voice].sample_pending = -1;
	}

      if ((mode & 0x01) || ((gus_read16 (0x09) >> 4) < 2065))
	{
	  ret_val = guswave_start_note2 (dev, voice, note_num, volume);
	}
      else
	{
	  voices[voice].dev_pending = dev;
	  voices[voice].note_pending = note_num;
	  voices[voice].volume_pending = volume;
	  voices[voice].volume_irq_mode = VMODE_START_NOTE;

	  gus_rampoff ();
	  gus_ramp_range (2000, 4065);
	  gus_ramp_rate (0, 63);	/* Fastest possible rate */
	  gus_rampon (0x20 | 0x40);	/* Ramp down, once, irq */
	}
    }
  RESTORE_INTR (flags);
  return ret_val;
}

static void
guswave_reset (int dev)
{
  int             i;

  for (i = 0; i < 32; i++)
    {
      gus_voice_init (i);
      gus_voice_init2 (i);
    }
}

static int
guswave_open (int dev, int mode)
{
  int             err;

  if (gus_busy)
    return RET_ERROR (EBUSY);

  gus_initialize ();

  if ((err = DMAbuf_open_dma (gus_devnum)))
    return err;

  RESET_WAIT_QUEUE (dram_sleeper, dram_sleep_flag);
  gus_busy = 1;
  active_device = GUS_DEV_WAVE;

  gus_reset ();

  return 0;
}

static void
guswave_close (int dev)
{
  gus_busy = 0;
  active_device = 0;
  gus_reset ();

  DMAbuf_close_dma (gus_devnum);
}

static int
guswave_load_patch (int dev, int format, snd_rw_buf * addr,
		    int offs, int count, int pmgr_flag)
{
  struct patch_info patch;
  int             instr;
  long            sizeof_patch;

  unsigned long   blk_size, blk_end, left, src_offs, target;

  sizeof_patch = (long) &patch.data[0] - (long) &patch;		/* 
								 * Size of
								 * the header
								 * * info 
								 */

  if (format != GUS_PATCH)
    {
      printk ("GUS Error: Invalid patch format (key) 0x%x\n", format);
      return RET_ERROR (EINVAL);
    }

  if (count < sizeof_patch)
    {
      printk ("GUS Error: Patch header too short\n");
      return RET_ERROR (EINVAL);
    }

  count -= sizeof_patch;

  if (free_sample >= MAX_SAMPLE)
    {
      printk ("GUS: Sample table full\n");
      return RET_ERROR (ENOSPC);
    }

  /* 
   * Copy the header from user space but ignore the first bytes which have
   * been transferred already.
   */

  COPY_FROM_USER (&((char *) &patch)[offs], addr, offs, sizeof_patch - offs);

  instr = patch.instr_no;

  if (instr < 0 || instr > MAX_PATCH)
    {
      printk ("GUS: Invalid patch number %d\n", instr);
      return RET_ERROR (EINVAL);
    }

  if (count < patch.len)
    {
      printk ("GUS Warning: Patch record too short (%d<%d)\n",
	      count, (int) patch.len);
      patch.len = count;
    }

  if (patch.len <= 0 || patch.len > gus_mem_size)
    {
      printk ("GUS: Invalid sample length %d\n", (int) patch.len);
      return RET_ERROR (EINVAL);
    }

  if (patch.mode & WAVE_LOOPING)
    {
      if (patch.loop_start < 0 || patch.loop_start >= patch.len)
	{
	  printk ("GUS: Invalid loop start\n");
	  return RET_ERROR (EINVAL);
	}

      if (patch.loop_end < patch.loop_start || patch.loop_end > patch.len)
	{
	  printk ("GUS: Invalid loop end\n");
	  return RET_ERROR (EINVAL);
	}
    }

  free_mem_ptr = (free_mem_ptr + 31) & ~31;	/* 
						 * Alignment 32 bytes 
						 */

#define GUS_BANK_SIZE (256*1024)

  if (patch.mode & WAVE_16_BITS)
    {
      /* 
       * 16 bit samples must fit one 256k bank.
       */
      if (patch.len >= GUS_BANK_SIZE)
	{
	  printk ("GUS: Sample (16 bit) too long %d\n", (int) patch.len);
	  return RET_ERROR (ENOSPC);
	}

      if ((free_mem_ptr / GUS_BANK_SIZE) !=
	  ((free_mem_ptr + patch.len) / GUS_BANK_SIZE))
	{
	  unsigned long   tmp_mem =	/* 
					 * Align to 256K*N 
					 */
	  ((free_mem_ptr / GUS_BANK_SIZE) + 1) * GUS_BANK_SIZE;

	  if ((tmp_mem + patch.len) > gus_mem_size)
	    return RET_ERROR (ENOSPC);

	  free_mem_ptr = tmp_mem;	/* 
					 * This leaves unusable memory 
					 */
	}
    }

  if ((free_mem_ptr + patch.len) > gus_mem_size)
    return RET_ERROR (ENOSPC);

  sample_ptrs[free_sample] = free_mem_ptr;

  /* 
   * Tremolo is not possible with envelopes 
   */

  if (patch.mode & WAVE_ENVELOPES)
    patch.mode &= ~WAVE_TREMOLO;

  memcpy ((char *) &samples[free_sample], &patch, sizeof_patch);

  /* 
   * Link this_one sample to the list of samples for patch 'instr'.
   */

  samples[free_sample].key = patch_table[instr];
  patch_table[instr] = free_sample;

  /* 
   * Use DMA to transfer the wave data to the DRAM
   */

  left = patch.len;
  src_offs = 0;
  target = free_mem_ptr;

  while (left)			/* 
				 * Not all moved 
				 */
    {
      blk_size = sound_buffsizes[gus_devnum];
      if (blk_size > left)
	blk_size = left;

      /* 
       * DMA cannot cross 256k bank boundaries. Check for that.
       */
      blk_end = target + blk_size;

      if ((target >> 18) != (blk_end >> 18))
	{			/* 
				 * Have to split the block 
				 */

	  blk_end &= ~(256 * 1024 - 1);
	  blk_size = blk_end - target;
	}

#if defined(GUS_NO_DMA) || defined(GUS_PATCH_NO_DMA)
      /* 
       * For some reason the DMA is not possible. We have to use PIO.
       */
      {
	long            i;
	unsigned char   data;

	for (i = 0; i < blk_size; i++)
	  {
	    GET_BYTE_FROM_USER (data, addr, sizeof_patch + i);
	    if (patch.mode & WAVE_UNSIGNED)

	      if (!(patch.mode & WAVE_16_BITS) || (i & 0x01))
		data ^= 0x80;	/* 
				 * Convert to signed 
				 */
	    gus_poke (target + i, data);
	  }
      }
#else /* 
         * * * GUS_NO_DMA   */
      {
	unsigned long   address, hold_address;
	unsigned char   dma_command;
	unsigned long   flags;

	/* 
	 * OK, move now. First in and then out.
	 */

	COPY_FROM_USER (snd_raw_buf[gus_devnum][0],
			addr, sizeof_patch + src_offs,
			blk_size);

	DISABLE_INTR (flags);	/******** INTERRUPTS DISABLED NOW ********/
	gus_write8 (0x41, 0);	/* 
				 * Disable GF1 DMA 
				 */
	DMAbuf_start_dma (gus_devnum, snd_raw_buf_phys[gus_devnum][0],
			  blk_size, DMA_MODE_WRITE);

	/* 
	 * Set the DRAM address for the wave data
	 */

	address = target;

	if (sound_dsp_dmachan[gus_devnum] > 3)
	  {
	    hold_address = address;
	    address = address >> 1;
	    address &= 0x0001ffffL;
	    address |= (hold_address & 0x000c0000L);
	  }

	gus_write16 (0x42, (address >> 4) & 0xffff);	/* 
							 * DRAM DMA address 
							 */

	/* 
	 * Start the DMA transfer
	 */

	dma_command = 0x21;	/* 
				 * IRQ enable, DMA start 
				 */
	if (patch.mode & WAVE_UNSIGNED)
	  dma_command |= 0x80;	/* 
				 * Invert MSB 
				 */
	if (patch.mode & WAVE_16_BITS)
	  dma_command |= 0x40;	/* 
				 * 16 bit _DATA_ 
				 */
	if (sound_dsp_dmachan[gus_devnum] > 3)
	  dma_command |= 0x04;	/* 
				 * 16 bit DMA channel 
				 */

	gus_write8 (0x41, dma_command);		/* 
						 * Let's go luteet (=bugs) 
						 */

	/* 
	 * Sleep here until the DRAM DMA done interrupt is served
	 */
	active_device = GUS_DEV_WAVE;

	DO_SLEEP (dram_sleeper, dram_sleep_flag, HZ);
	if (TIMED_OUT (dram_sleeper, dram_sleep_flag))
	  printk ("GUS: DMA Transfer timed out\n");
	RESTORE_INTR (flags);
      }
#endif /* 
          * * * GUS_NO_DMA   */

      /* 
       * Now the next part
       */

      left -= blk_size;
      src_offs += blk_size;
      target += blk_size;

      gus_write8 (0x41, 0);	/* 
				 * Stop DMA 
				 */
    }

  free_mem_ptr += patch.len;

  if (!pmgr_flag)
    pmgr_inform (dev, PM_E_PATCH_LOADED, instr, free_sample, 0, 0);
  free_sample++;
  return 0;
}

static void
guswave_hw_control (int dev, unsigned char *event)
{
  int             voice, cmd;
  unsigned short  p1, p2;
  unsigned long   plong, flags;

  cmd = event[2];
  voice = event[3];
  p1 = *(unsigned short *) &event[4];
  p2 = *(unsigned short *) &event[6];
  plong = *(unsigned long *) &event[4];

  if ((voices[voice].volume_irq_mode == VMODE_START_NOTE) &&
      (cmd != _GUS_VOICESAMPLE) && (cmd != _GUS_VOICE_POS))
    do_volume_irq (voice);

  switch (cmd)
    {

    case _GUS_NUMVOICES:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_select_max_voices (p1);
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICESAMPLE:
      guswave_set_instr (dev, voice, p1);
      break;

    case _GUS_VOICEON:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      p1 &= ~0x20;		/* 
				 * Disable intr 
				 */
      gus_voice_on (p1);
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICEOFF:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_voice_off ();
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICEFADE:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_voice_fade (voice);
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICEMODE:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      p1 &= ~0x20;		/* 
				 * Disable intr 
				 */
      gus_voice_mode (p1);
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICEBALA:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_voice_balance (p1);
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICEFREQ:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_voice_freq (plong);
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICEVOL:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_voice_volume (p1);
      RESTORE_INTR (flags);
      break;

    case _GUS_VOICEVOL2:	/* 
				 * Just update the voice value 
				 */
      voices[voice].initial_volume =
	voices[voice].current_volume = p1;
      break;

    case _GUS_RAMPRANGE:
      if (voices[voice].mode & WAVE_ENVELOPES)
	break;			/* 
				 * NO-NO 
				 */
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_ramp_range (p1, p2);
      RESTORE_INTR (flags);
      break;

    case _GUS_RAMPRATE:
      if (voices[voice].mode & WAVE_ENVELOPES)
	break;			/* 
				 * NO-NO 
				 */
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_ramp_rate (p1, p2);
      RESTORE_INTR (flags);
      break;

    case _GUS_RAMPMODE:
      if (voices[voice].mode & WAVE_ENVELOPES)
	break;			/* 
				 * NO-NO 
				 */
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      p1 &= ~0x20;		/* 
				 * Disable intr 
				 */
      gus_ramp_mode (p1);
      RESTORE_INTR (flags);
      break;

    case _GUS_RAMPON:
      if (voices[voice].mode & WAVE_ENVELOPES)
	break;			/* 
				 * NO-NO 
				 */
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      p1 &= ~0x20;		/* 
				 * Disable intr 
				 */
      gus_rampon (p1);
      RESTORE_INTR (flags);
      break;

    case _GUS_RAMPOFF:
      if (voices[voice].mode & WAVE_ENVELOPES)
	break;			/* 
				 * NO-NO 
				 */
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_rampoff ();
      RESTORE_INTR (flags);
      break;

    case _GUS_VOLUME_SCALE:
      volume_base = p1;
      volume_scale = p2;
      break;

    case _GUS_VOICE_POS:
      DISABLE_INTR (flags);
      gus_select_voice (voice);
      gus_set_voice_pos (voice, plong);
      RESTORE_INTR (flags);
      break;

    default:;
    }
}

static int
gus_sampling_set_speed (int speed)
{
  if (speed <= 0)
    return gus_sampling_speed;

  if (speed > 44100)
    speed = 44100;

  gus_sampling_speed = speed;
  return speed;
}

static int
gus_sampling_set_channels (int channels)
{
  if (!channels)
    return gus_sampling_channels;
  if (channels > 2)
    channels = 2;
  if (channels < 1)
    channels = 1;
  gus_sampling_channels = channels;
  return channels;
}

static int
gus_sampling_set_bits (int bits)
{
  if (!bits)
    return gus_sampling_bits;

  if (bits != 8 && bits != 16)
    bits = 8;

  gus_sampling_bits = bits;
  return bits;
}

static int
gus_sampling_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  switch (cmd)
    {
    case SOUND_PCM_WRITE_RATE:
      if (local)
	return gus_sampling_set_speed (arg);
      return IOCTL_OUT (arg, gus_sampling_set_speed (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_RATE:
      if (local)
	return gus_sampling_speed;
      return IOCTL_OUT (arg, gus_sampling_speed);
      break;

    case SNDCTL_DSP_STEREO:
      if (local)
	return gus_sampling_set_channels (arg + 1) - 1;
      return IOCTL_OUT (arg, gus_sampling_set_channels (IOCTL_IN (arg) + 1) - 1);
      break;

    case SOUND_PCM_WRITE_CHANNELS:
      if (local)
	return gus_sampling_set_channels (arg);
      return IOCTL_OUT (arg, gus_sampling_set_channels (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_CHANNELS:
      if (local)
	return gus_sampling_channels;
      return IOCTL_OUT (arg, gus_sampling_channels);
      break;

    case SNDCTL_DSP_SAMPLESIZE:
      if (local)
	return gus_sampling_set_bits (arg);
      return IOCTL_OUT (arg, gus_sampling_set_bits (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_BITS:
      if (local)
	return gus_sampling_bits;
      return IOCTL_OUT (arg, gus_sampling_bits);

    case SOUND_PCM_WRITE_FILTER:	/* 
					 * NOT YET IMPLEMENTED 
					 */
      return IOCTL_OUT (arg, RET_ERROR (EINVAL));
      break;

    case SOUND_PCM_READ_FILTER:
      return IOCTL_OUT (arg, RET_ERROR (EINVAL));
      break;

    default:
      return RET_ERROR (EINVAL);
    }
  return RET_ERROR (EINVAL);
}

static void
gus_sampling_reset (int dev)
{
}

static int
gus_sampling_open (int dev, int mode)
{
#ifdef GUS_NO_DMA
  printk ("GUS: DMA mode not enabled. Device not supported\n");
  return RET_ERROR (ENXIO);
#endif

  if (gus_busy)
    return RET_ERROR (EBUSY);

  gus_initialize ();

  gus_busy = 1;
  active_device = 0;

  gus_reset ();
  reset_sample_memory ();
  gus_select_max_voices (14);

  pcm_active = 0;
  pcm_opened = 1;
  if (mode & OPEN_READ)
    {
      recording_active = 1;
      set_input_volumes ();
    }

  return 0;
}

static void
gus_sampling_close (int dev)
{
  gus_reset ();
  gus_busy = 0;
  pcm_opened = 0;
  active_device = 0;

  if (recording_active)
    set_input_volumes ();

  recording_active = 0;
}

static void
gus_sampling_update_volume (void)
{
  unsigned long   flags;
  int             voice;

  DISABLE_INTR (flags);
  if (pcm_active && pcm_opened)
    for (voice = 0; voice < gus_sampling_channels; voice++)
      {
	gus_select_voice (voice);
	gus_rampoff ();
	gus_voice_volume (1530 + (25 * gus_pcm_volume));
	gus_ramp_range (65, 1530 + (25 * gus_pcm_volume));
      }
  RESTORE_INTR (flags);
}

static void
play_next_pcm_block (void)
{
  unsigned long   flags;
  int             speed = gus_sampling_speed;
  int             this_one, is16bits, chn;
  unsigned long   dram_loc;
  unsigned char   mode[2], ramp_mode[2];

  if (!pcm_qlen)
    return;

  this_one = pcm_head;

  for (chn = 0; chn < gus_sampling_channels; chn++)
    {
      mode[chn] = 0x00;
      ramp_mode[chn] = 0x03;	/* 
				 * Ramping and rollover off 
				 */

      if (chn == 0)
	{
	  mode[chn] |= 0x20;	/* 
				 * Loop irq 
				 */
	  voices[chn].loop_irq_mode = LMODE_PCM;
	}

      if (gus_sampling_bits != 8)
	{
	  is16bits = 1;
	  mode[chn] |= 0x04;	/* 
				 * 16 bit data 
				 */
	}
      else
	is16bits = 0;

      dram_loc = this_one * pcm_bsize;
      dram_loc += chn * pcm_banksize;

      if (this_one == (pcm_nblk - 1))	/* 
					 * Last of the DRAM buffers 
					 */
	{
	  mode[chn] |= 0x08;	/* 
				 * Enable loop 
				 */
	  ramp_mode[chn] = 0x03;	/* 
					 * Disable rollover 
					 */
	}
      else
	{
	  if (chn == 0)
	    ramp_mode[chn] = 0x04;	/* 
					 * Enable rollover bit 
					 */
	}

      DISABLE_INTR (flags);
      gus_select_voice (chn);
      gus_voice_freq (speed);

      if (gus_sampling_channels == 1)
	gus_voice_balance (7);	/* 
				 * mono 
				 */
      else if (chn == 0)
	gus_voice_balance (0);	/* 
				 * left 
				 */
      else
	gus_voice_balance (15);	/* 
				 * right 
				 */

      if (!pcm_active)		/* 
				 * Voice not started yet 
				 */
	{
	  /* 
	   * The playback was not started yet (or there has been a pause).
	   * Start the voice (again) and ask for a rollover irq at the end of
	   * this_one block. If this_one one is last of the buffers, use just
	   * the normal loop with irq.
	   */

	  gus_voice_off ();	/* 
				 * It could already be running 
				 */
	  gus_rampoff ();
	  gus_voice_volume (1530 + (25 * gus_pcm_volume));
	  gus_ramp_range (65, 1530 + (25 * gus_pcm_volume));

	  gus_write_addr (0x0a, dram_loc, is16bits);	/* 
							 * Starting position 
							 */
	  gus_write_addr (0x02, chn * pcm_banksize, is16bits);	/* 
								 * Loop start 
								 * location 
								 */

	  if (chn != 0)
	    gus_write_addr (0x04, pcm_banksize + (pcm_bsize * pcm_nblk),
			    is16bits);	/* 
					 * Loop end location 
					 */
	}

      if (chn == 0)
	gus_write_addr (0x04, dram_loc + pcm_datasize[this_one], is16bits);	/* 
										 * Loop 
										 * end 
										 * location 
										 */
      else
	mode[chn] |= 0x08;	/* 
				 * Enable loop 
				 */

      if (pcm_datasize[this_one] != pcm_bsize)
	{
	  /* 
	   * Incomplete block. Possibly the last one. 
	   */
	  if (chn == 0)
	    {
	      mode[chn] &= ~0x08;	/* 
					 * Disable loop 
					 */
	      mode[chn] |= 0x20;	/* 
					 * Enable loop IRQ 
					 */
	      voices[0].loop_irq_mode = LMODE_PCM_STOP;
	      ramp_mode[chn] = 0x03;	/* 
					 * No rollover bit 
					 */
	    }
	  else
	    {
	      gus_write_addr (0x04, dram_loc + pcm_datasize[this_one], is16bits);	/* 
											 * Loop 
											 * end 
											 * location 
											 */
	      mode[chn] &= ~0x08;	/* 
					 * Disable loop 
					 */
	    }
	}

      RESTORE_INTR (flags);
    }

  for (chn = 0; chn < gus_sampling_channels; chn++)
    {
      DISABLE_INTR (flags);
      gus_select_voice (chn);
      gus_write8 (0x0d, ramp_mode[chn]);
      gus_voice_on (mode[chn]);
      RESTORE_INTR (flags);
    }

  pcm_active = 1;
}

static void
gus_transfer_output_block (int dev, unsigned long buf,
			   int total_count, int intrflag, int chn)
{
  /* 
   * This routine transfers one block of audio data to the DRAM. In mono mode
   * it's called just once. When in stereo mode, this_one routine is called
   * once for both channels.
   * 
   * The left/mono channel data is transferred to the beginning of dram and the
   * right data to the area pointed by gus_page_size.
   */

  int             this_one, count;
  unsigned long   flags;
  unsigned char   dma_command;
  unsigned long   address, hold_address;

  DISABLE_INTR (flags);

  count = total_count / gus_sampling_channels;

  if (chn == 0)
    {
      if (pcm_qlen >= pcm_nblk)
	printk ("GUS Warning: PCM buffers out of sync\n");

      this_one = pcm_current_block = pcm_tail;
      pcm_qlen++;
      pcm_tail = (pcm_tail + 1) % pcm_nblk;
      pcm_datasize[this_one] = count;
    }
  else
    this_one = pcm_current_block;

  gus_write8 (0x41, 0);		/* 
				 * Disable GF1 DMA 
				 */
  DMAbuf_start_dma (dev, buf + (chn * count), count, DMA_MODE_WRITE);

  address = this_one * pcm_bsize;
  address += chn * pcm_banksize;

  if (sound_dsp_dmachan[dev] > 3)
    {
      hold_address = address;
      address = address >> 1;
      address &= 0x0001ffffL;
      address |= (hold_address & 0x000c0000L);
    }

  gus_write16 (0x42, (address >> 4) & 0xffff);	/* 
						 * DRAM DMA address 
						 */

  dma_command = 0x21;		/* 
				 * IRQ enable, DMA start 
				 */

  if (gus_sampling_bits != 8)
    dma_command |= 0x40;	/* 
				 * 16 bit _DATA_ 
				 */
  else
    dma_command |= 0x80;	/* 
				 * Invert MSB 
				 */

  if (sound_dsp_dmachan[dev] > 3)
    dma_command |= 0x04;	/* 
				 * 16 bit DMA channel 
				 */

  gus_write8 (0x41, dma_command);	/* 
					 * Kick on 
					 */

  if (chn == (gus_sampling_channels - 1))	/* 
						 * Last channel 
						 */
    {
      /* 
       * Last (right or mono) channel data 
       */
      active_device = GUS_DEV_PCM_DONE;
      if (!pcm_active && (pcm_qlen > 2 || count < pcm_bsize))
	{
	  play_next_pcm_block ();
	}
    }
  else				/* 
				   * * * Left channel data. The right channel
				   * is * * * transferred after DMA interrupt   */
    active_device = GUS_DEV_PCM_CONTINUE;

  RESTORE_INTR (flags);
}

static void
gus_sampling_output_block (int dev, unsigned long buf, int total_count,
			   int intrflag, int restart_dma)
{
  pcm_current_buf = buf;
  pcm_current_count = total_count;
  pcm_current_intrflag = intrflag;
  pcm_current_dev = dev;
  gus_transfer_output_block (dev, buf, total_count, intrflag, 0);
}

static void
gus_sampling_start_input (int dev, unsigned long buf, int count,
			  int intrflag, int restart_dma)
{
  unsigned long   flags;
  unsigned char   mode;

  DISABLE_INTR (flags);

  DMAbuf_start_dma (dev, buf, count, DMA_MODE_READ);

  mode = 0xa0;			/* 
				 * DMA IRQ enable, invert MSB 
				 */

  if (sound_dsp_dmachan[dev] > 3)
    mode |= 0x04;		/* 
				 * 16 bit DMA channel 
				 */
  if (gus_sampling_channels > 1)
    mode |= 0x02;		/* 
				 * Stereo 
				 */
  mode |= 0x01;			/* 
				 * DMA enable 
				 */

  gus_write8 (0x49, mode);

  RESTORE_INTR (flags);
}

static int
gus_sampling_prepare_for_input (int dev, int bsize, int bcount)
{
  unsigned int    rate;

  rate = (9878400 / (gus_sampling_speed + 2)) / 16;

  gus_write8 (0x48, rate & 0xff);	/* 
					 * Set sampling frequency 
					 */

  if (gus_sampling_bits != 8)
    {
      printk ("GUS Error: 16 bit recording not supported\n");
      return RET_ERROR (EINVAL);
    }

  return 0;
}

static int
gus_sampling_prepare_for_output (int dev, int bsize, int bcount)
{
  int             i;

  long            mem_ptr, mem_size;

  mem_ptr = 0;
  mem_size = gus_mem_size / gus_sampling_channels;

  if (mem_size > (256 * 1024))
    mem_size = 256 * 1024;

  pcm_bsize = bsize / gus_sampling_channels;
  pcm_head = pcm_tail = pcm_qlen = 0;

  pcm_nblk = MAX_PCM_BUFFERS;
  if ((pcm_bsize * pcm_nblk) > mem_size)
    pcm_nblk = mem_size / pcm_bsize;

  for (i = 0; i < pcm_nblk; i++)
    pcm_datasize[i] = 0;

  pcm_banksize = pcm_nblk * pcm_bsize;

  if (gus_sampling_bits != 8 && pcm_banksize == (256 * 1024))
    pcm_nblk--;

  return 0;
}

static int
gus_has_output_drained (int dev)
{
  return !pcm_qlen;
}

static void
gus_copy_from_user (int dev, char *localbuf, int localoffs,
		    snd_rw_buf * userbuf, int useroffs, int len)
{
  if (gus_sampling_channels == 1)
    {
      COPY_FROM_USER (&localbuf[localoffs], userbuf, useroffs, len);
    }
  else if (gus_sampling_bits == 8)
    {
      int             in_left = useroffs;
      int             in_right = useroffs + 1;
      char           *out_left, *out_right;
      int             i;

      len /= 2;
      localoffs /= 2;
      out_left = &localbuf[localoffs];
      out_right = out_left + pcm_bsize;

      for (i = 0; i < len; i++)
	{
	  GET_BYTE_FROM_USER (*out_left++, userbuf, in_left);
	  in_left += 2;
	  GET_BYTE_FROM_USER (*out_right++, userbuf, in_right);
	  in_right += 2;
	}
    }
  else
    {
      int             in_left = useroffs;
      int             in_right = useroffs + 1;
      short          *out_left, *out_right;
      int             i;

      len /= 4;
      localoffs /= 4;

      out_left = (short *) &localbuf[localoffs];
      out_right = out_left + (pcm_bsize / 2);

      for (i = 0; i < len; i++)
	{
	  GET_SHORT_FROM_USER (*out_left++, (short *) userbuf, in_left);
	  in_left += 2;
	  GET_SHORT_FROM_USER (*out_right++, (short *) userbuf, in_right);
	  in_right += 2;
	}
    }
}

static struct audio_operations gus_sampling_operations =
{
  "Gravis UltraSound",
  gus_sampling_open,
  gus_sampling_close,
  gus_sampling_output_block,
  gus_sampling_start_input,
  gus_sampling_ioctl,
  gus_sampling_prepare_for_input,
  gus_sampling_prepare_for_output,
  gus_sampling_reset,
  gus_sampling_reset,
  gus_has_output_drained,
  gus_copy_from_user
};

#ifdef FUTURE_VERSION
static void
guswave_bender (int dev, int voice, int value)
{
  int             freq;
  unsigned long   flags;

  voices[voice].bender = value - 8192;
  freq = compute_finetune (voices[voice].orig_freq, value, voices[voice].bender_range);
  voices[voice].current_freq = freq;

  DISABLE_INTR (flags);
  gus_select_voice (voice);
  gus_voice_freq (freq);
  RESTORE_INTR (flags);
}
#endif

static int
guswave_patchmgr (int dev, struct patmgr_info *rec)
{
  int             i, n;

  switch (rec->command)
    {
    case PM_GET_DEVTYPE:
      rec->parm1 = PMTYPE_WAVE;
      return 0;
      break;

    case PM_GET_NRPGM:
      rec->parm1 = MAX_PATCH;
      return 0;
      break;

    case PM_GET_PGMMAP:
      rec->parm1 = MAX_PATCH;

      for (i = 0; i < MAX_PATCH; i++)
	{
	  int             ptr = patch_table[i];

	  rec->data.data8[i] = 0;

	  while (ptr >= 0 && ptr < free_sample)
	    {
	      rec->data.data8[i]++;
	      ptr = samples[ptr].key;	/* 
					 * Follow link 
					 */
	    }
	}
      return 0;
      break;

    case PM_GET_PGM_PATCHES:
      {
	int             ptr = patch_table[rec->parm1];

	n = 0;

	while (ptr >= 0 && ptr < free_sample)
	  {
	    rec->data.data32[n++] = ptr;
	    ptr = samples[ptr].key;	/* 
					 * Follow link 
					 */
	  }
      }
      rec->parm1 = n;
      return 0;
      break;

    case PM_GET_PATCH:
      {
	int             ptr = rec->parm1;
	struct patch_info *pat;

	if (ptr < 0 || ptr >= free_sample)
	  return RET_ERROR (EINVAL);

	memcpy (rec->data.data8, (char *) &samples[ptr],
		sizeof (struct patch_info));

	pat = (struct patch_info *) rec->data.data8;

	pat->key = GUS_PATCH;	/* 
				 * Restore patch type 
				 */
	rec->parm1 = sample_ptrs[ptr];	/* 
					 * DRAM address 
					 */
	rec->parm2 = sizeof (struct patch_info);
      }
      return 0;
      break;

    case PM_SET_PATCH:
      {
	int             ptr = rec->parm1;
	struct patch_info *pat;

	if (ptr < 0 || ptr >= free_sample)
	  return RET_ERROR (EINVAL);

	pat = (struct patch_info *) rec->data.data8;

	if (pat->len > samples[ptr].len)	/* 
						 * Cannot expand sample 
						 */
	  return RET_ERROR (EINVAL);

	pat->key = samples[ptr].key;	/* 
					 * Ensure the link is correct 
					 */

	memcpy ((char *) &samples[ptr], rec->data.data8,
		sizeof (struct patch_info));

	pat->key = GUS_PATCH;
      }
      return 0;
      break;

    case PM_READ_PATCH:	/* 
				 * Returns a block of wave data from the DRAM 
				 */
      {
	int             sample = rec->parm1;
	int             n;
	long            offs = rec->parm2;
	int             l = rec->parm3;

	if (sample < 0 || sample >= free_sample)
	  return RET_ERROR (EINVAL);

	if (offs < 0 || offs >= samples[sample].len)
	  return RET_ERROR (EINVAL);	/* 
					 * Invalid offset 
					 */

	n = samples[sample].len - offs;		/* 
						 * Nr of bytes left 
						 */

	if (l > n)
	  l = n;

	if (l > sizeof (rec->data.data8))
	  l = sizeof (rec->data.data8);

	if (l <= 0)
	  return RET_ERROR (EINVAL);	/* 
					 * Was there a bug? 
					 */

	offs += sample_ptrs[sample];	/* 
					 * Begin offsess + offset to DRAM 
					 */

	for (n = 0; n < l; n++)
	  rec->data.data8[n] = gus_peek (offs++);
	rec->parm1 = n;		/* 
				 * Nr of bytes copied 
				 */
      }
      return 0;
      break;

    case PM_WRITE_PATCH:	/* 
				 * Writes a block of wave data to the DRAM 
				 */
      {
	int             sample = rec->parm1;
	int             n;
	long            offs = rec->parm2;
	int             l = rec->parm3;

	if (sample < 0 || sample >= free_sample)
	  return RET_ERROR (EINVAL);

	if (offs < 0 || offs >= samples[sample].len)
	  return RET_ERROR (EINVAL);	/* 
					 * Invalid offset 
					 */

	n = samples[sample].len - offs;		/* 
						 * Nr of bytes left 
						 */

	if (l > n)
	  l = n;

	if (l > sizeof (rec->data.data8))
	  l = sizeof (rec->data.data8);

	if (l <= 0)
	  return RET_ERROR (EINVAL);	/* 
					 * Was there a bug? 
					 */

	offs += sample_ptrs[sample];	/* 
					 * Begin offsess + offset to DRAM 
					 */

	for (n = 0; n < l; n++)
	  gus_poke (offs++, rec->data.data8[n]);
	rec->parm1 = n;		/* 
				 * Nr of bytes copied 
				 */
      }
      return 0;
      break;

    default:
      return RET_ERROR (EINVAL);
    }
}

static struct synth_operations guswave_operations =
{
  &gus_info,
#ifdef FUTURE_VERSION
  0,
#endif
  SYNTH_TYPE_SAMPLE,
  SAMPLE_TYPE_GUS,
  guswave_open,
  guswave_close,
  guswave_ioctl,
  guswave_kill_note,
  guswave_start_note,
  guswave_set_instr,
  guswave_reset,
  guswave_hw_control,
  guswave_load_patch,
  guswave_aftertouch,
  guswave_controller,
  guswave_panning,
  guswave_patchmgr,
#ifdef FUTURE_VERSION
  guswave_bender
#endif
};

static void
set_input_volumes (void)
{
  unsigned long   flags;
  unsigned char   mask = 0xff & ~0x06;	/* Just line out enabled */

  DISABLE_INTR (flags);

/*
 *    Enable channels having vol > 10%
 *      Note! bit 0x01 means line in DISABLED while 0x04 means
 *            mic in ENABLED.
 */
  if (gus_line_vol > 10)
    mask &= ~0x01;
  if (gus_mic_vol > 10)
    mask |= 0x04;

  if (recording_active)
    {
/*
 *    Disable channel, if not selected for recording
 */
      if (!(gus_recmask & SOUND_MASK_LINE))
	mask |= 0x01;
      if (!(gus_recmask & SOUND_MASK_MIC))
	mask &= ~0x04;
    }

  mix_image &= ~0x07;
  mix_image |= mask & 0x07;
  OUTB (mix_image, u_Mixer);

  RESTORE_INTR (flags);
}

static int
gus_mixer_ioctl (int dev, unsigned int cmd, unsigned int arg)
{
#define MIX_DEVS	(SOUND_MASK_MIC|SOUND_MASK_LINE| \
			 SOUND_MASK_SYNTH|SOUND_MASK_PCM)
  if (((cmd >> 8) & 0xff) == 'M')
    {
      if (cmd & IOC_IN)
	switch (cmd & 0xff)
	  {
	  case SOUND_MIXER_RECSRC:
	    gus_recmask = IOCTL_IN (arg) & MIX_DEVS;
	    if (!(gus_recmask & (SOUND_MASK_MIC | SOUND_MASK_LINE)))
	      gus_recmask = SOUND_MASK_MIC;
	    /* Note! Input volumes are updated during next open for recording */
	    return IOCTL_OUT (arg, gus_recmask);
	    break;

	  case SOUND_MIXER_MIC:
	    {
	      int             vol = IOCTL_IN (arg) & 0xff;

	      if (vol < 0)
		vol = 0;
	      if (vol > 100)
		vol = 100;
	      gus_mic_vol = vol;
	      set_input_volumes ();
	      return IOCTL_OUT (arg, vol | (vol << 8));
	    }
	    break;

	  case SOUND_MIXER_LINE:
	    {
	      int             vol = IOCTL_IN (arg) & 0xff;

	      if (vol < 0)
		vol = 0;
	      if (vol > 100)
		vol = 100;
	      gus_line_vol = vol;
	      set_input_volumes ();
	      return IOCTL_OUT (arg, vol | (vol << 8));
	    }
	    break;

	  case SOUND_MIXER_PCM:
	    gus_pcm_volume = IOCTL_IN (arg) & 0xff;
	    if (gus_pcm_volume < 0)
	      gus_pcm_volume = 0;
	    if (gus_pcm_volume > 100)
	      gus_pcm_volume = 100;
	    gus_sampling_update_volume ();
	    return IOCTL_OUT (arg, gus_pcm_volume | (gus_pcm_volume << 8));
	    break;

	  case SOUND_MIXER_SYNTH:
	    {
	      int             voice;

	      gus_wave_volume = IOCTL_IN (arg) & 0xff;

	      if (gus_wave_volume < 0)
		gus_wave_volume = 0;
	      if (gus_wave_volume > 100)
		gus_wave_volume = 100;

	      if (active_device == GUS_DEV_WAVE)
		for (voice = 0; voice < nr_voices; voice++)
		  dynamic_volume_change (voice);	/* 
							 * Apply the new
							 * volume 
							 */

	      return IOCTL_OUT (arg, gus_wave_volume | (gus_wave_volume << 8));
	    }
	    break;

	  default:
	    return RET_ERROR (EINVAL);
	  }
      else
	switch (cmd & 0xff)	/* 
				 * Return parameters 
				 */
	  {

	  case SOUND_MIXER_RECSRC:
	    return IOCTL_OUT (arg, gus_recmask);
	    break;

	  case SOUND_MIXER_DEVMASK:
	    return IOCTL_OUT (arg, MIX_DEVS);
	    break;

	  case SOUND_MIXER_STEREODEVS:
	    return IOCTL_OUT (arg, 0);
	    break;

	  case SOUND_MIXER_RECMASK:
	    return IOCTL_OUT (arg, SOUND_MASK_MIC | SOUND_MASK_LINE);
	    break;

	  case SOUND_MIXER_CAPS:
	    return IOCTL_OUT (arg, 0);
	    break;

	  case SOUND_MIXER_MIC:
	    return IOCTL_OUT (arg, gus_mic_vol | (gus_mic_vol << 8));
	    break;

	  case SOUND_MIXER_LINE:
	    return IOCTL_OUT (arg, gus_line_vol | (gus_line_vol << 8));
	    break;

	  case SOUND_MIXER_PCM:
	    return IOCTL_OUT (arg, gus_pcm_volume | (gus_pcm_volume << 8));
	    break;

	  case SOUND_MIXER_SYNTH:
	    return IOCTL_OUT (arg, gus_wave_volume | (gus_wave_volume << 8));
	    break;

	  default:
	    return RET_ERROR (EINVAL);
	  }
    }
  else
    return RET_ERROR (EINVAL);
}

static struct mixer_operations gus_mixer_operations =
{
  gus_mixer_ioctl
};

long
gus_wave_init (long mem_start, int irq, int dma)
{
  printk ("snd4: <Gravis UltraSound %dk>", (int) gus_mem_size / 1024);

  if (irq < 0 || irq > 15)
    {
      printk ("ERROR! Invalid IRQ#%d. GUS Disabled", irq);
      return mem_start;
    }

  if (dma < 0 || dma > 7)
    {
      printk ("ERROR! Invalid DMA#%d. GUS Disabled", dma);
      return mem_start;
    }

  gus_irq = irq;
  gus_dma = dma;

  if (num_synths >= MAX_SYNTH_DEV)
    printk ("GUS Error: Too many synthesizers\n");
  else
    synth_devs[num_synths++] = &guswave_operations;

  PERMANENT_MALLOC (struct patch_info *, samples,
	                   (MAX_SAMPLE + 1) * sizeof (*samples), mem_start);

  reset_sample_memory ();

  gus_initialize ();

  if (num_dspdevs < MAX_DSP_DEV)
    {
      dsp_devs[gus_devnum = num_dspdevs++] = &gus_sampling_operations;
      sound_dsp_dmachan[gus_devnum] = dma;
      sound_buffcounts[gus_devnum] = DSP_BUFFCOUNT;
      sound_buffsizes[gus_devnum] = DSP_BUFFSIZE;
      sound_dma_automode[gus_devnum] = 0;
    }
  else
    printk ("GUS: Too many PCM devices available\n");

  if (num_mixers < MAX_MIXER_DEV)	/* 
					 * Don't install if there is another
					 * mixer 
					 */
    mixer_devs[num_mixers++] = &gus_mixer_operations;

  return mem_start;
}

static void
do_loop_irq (int voice)
{
  unsigned char   tmp;
  int             mode, parm;
  unsigned long   flags;

  DISABLE_INTR (flags);
  gus_select_voice (voice);

  tmp = gus_read8 (0x00);
  tmp &= ~0x20;			/* 
				 * Disable wave IRQ for this_one voice 
				 */
  gus_write8 (0x00, tmp);

  mode = voices[voice].loop_irq_mode;
  voices[voice].loop_irq_mode = 0;
  parm = voices[voice].loop_irq_parm;

  switch (mode)
    {

    case LMODE_FINISH:		/* 
				 * Final loop finished, shoot volume down 
				 */

      if ((gus_read16 (0x09) >> 4) < 100)	/* 
						 * Get current volume 
						 */
	{
	  gus_voice_off ();
	  gus_rampoff ();
	  gus_voice_init (voice);
	  break;
	}
      gus_ramp_range (65, 4065);
      gus_ramp_rate (0, 63);	/* 
				 * Fastest possible rate 
				 */
      gus_rampon (0x20 | 0x40);	/* 
				 * Ramp down, once, irq 
				 */
      voices[voice].volume_irq_mode = VMODE_HALT;
      break;

    case LMODE_PCM_STOP:
      pcm_active = 0;		/* 
				 * Requires extensive processing 
				 */
    case LMODE_PCM:
      {
	int             orig_qlen = pcm_qlen;

	pcm_qlen--;
	pcm_head = (pcm_head + 1) % pcm_nblk;
	if (pcm_qlen)
	  {
	    play_next_pcm_block ();
	  }
	else
	  {			/* 
				 * Out of data. Just stop the voice 
				 */
	    gus_voice_off ();
	    gus_rampoff ();
	    pcm_active = 0;
	  }

	if (orig_qlen == pcm_nblk)
	  {
	    DMAbuf_outputintr (gus_devnum, 0);
	  }
      }
      break;

    default:;
    }
  RESTORE_INTR (flags);
}

static void
do_volume_irq (int voice)
{
  unsigned char   tmp;
  int             mode, parm;
  unsigned long   flags;

  DISABLE_INTR (flags);

  gus_select_voice (voice);

  tmp = gus_read8 (0x0d);
  tmp &= ~0x20;			/* 
				 * Disable volume ramp IRQ 
				 */
  gus_write8 (0x0d, tmp);

  mode = voices[voice].volume_irq_mode;
  voices[voice].volume_irq_mode = 0;
  parm = voices[voice].volume_irq_parm;

  switch (mode)
    {
    case VMODE_HALT:		/* 
				 * Decay phase finished 
				 */
      gus_voice_init (voice);
      break;

    case VMODE_ENVELOPE:
      gus_rampoff ();
      step_envelope (voice);
      break;

    case VMODE_START_NOTE:
      guswave_start_note2 (voices[voice].dev_pending, voice,
		  voices[voice].note_pending, voices[voice].volume_pending);
      if (voices[voice].kill_pending)
	guswave_kill_note (voices[voice].dev_pending, voice, 0);
      if (voices[voice].sample_pending >= 0)
	{
	  guswave_set_instr (voices[voice].dev_pending, voice,
			     voices[voice].sample_pending);
	  voices[voice].sample_pending = -1;
	}
      break;

    default:;
    }

  RESTORE_INTR (flags);
}

void
gus_voice_irq (void)
{
  unsigned long   wave_ignore = 0, volume_ignore = 0;
  unsigned long   voice_bit;

  unsigned char   src, voice;

  while (1)
    {
      src = gus_read8 (0x0f);	/* 
				 * Get source info 
				 */
      voice = src & 0x1f;
      src &= 0xc0;

      if (src == (0x80 | 0x40))
	return;			/* 
				 * No interrupt 
				 */

      voice_bit = 1 << voice;

      if (!(src & 0x80))	/* 
				 * Wave IRQ pending 
				 */
	if (!(wave_ignore & voice_bit) && voice < nr_voices)	/* 
								 * Not done
								 * yet 
								 */
	  {
	    wave_ignore |= voice_bit;
	    do_loop_irq (voice);
	  }

      if (!(src & 0x40))	/* 
				 * Volume IRQ pending 
				 */
	if (!(volume_ignore & voice_bit) && voice < nr_voices)	/* 
								 * Not done
								 * yet 
								 */
	  {
	    volume_ignore |= voice_bit;
	    do_volume_irq (voice);
	  }
    }
}

void
guswave_dma_irq (void)
{
  unsigned char   status;

  status = gus_look8 (0x41);	/* 
				 * Get DMA IRQ Status 
				 */
  if (status & 0x40)		/* 
				 * DMA Irq pending 
				 */
    switch (active_device)
      {
      case GUS_DEV_WAVE:
	if (SOMEONE_WAITING (dram_sleeper, dram_sleep_flag))
	  WAKE_UP (dram_sleeper, dram_sleep_flag);
	break;

      case GUS_DEV_PCM_CONTINUE:
	gus_transfer_output_block (pcm_current_dev, pcm_current_buf,
				   pcm_current_count,
				   pcm_current_intrflag, 1);
	break;

      case GUS_DEV_PCM_DONE:
	if (pcm_qlen < pcm_nblk)
	  {
	    DMAbuf_outputintr (gus_devnum, pcm_qlen == 0);
	  }
	break;

      default:;
      }

  status = gus_look8 (0x49);	/* 
				 * Get Sampling IRQ Status 
				 */
  if (status & 0x40)		/* 
				 * Sampling Irq pending 
				 */
    {
      DMAbuf_inputintr (gus_devnum);
    }

}

#endif
