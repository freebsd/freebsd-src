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
 */

#define DEB(x)
#define DEB1(x)
/*
 * #define DEB_DMARES
 */
#include <i386/isa/sound/sound_config.h>
#include <i386/isa/sound/sb.h>
#include <i386/isa/sound/sb_mixer.h>

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SB16) && !defined(EXCLUDE_SB) && !defined(EXCLUDE_AUDIO) && !defined(EXCLUDE_SBPRO)

extern int      sbc_base;
extern int      sbc_major;
extern int      sbc_minor;

static int      sb16_dsp_ok = 0;	/*


					   * *  * * Set to 1 after successful *
					   * * initialization   */
static int      dsp_16bit = 0;
static int      dsp_stereo = 0;
static int      dsp_current_speed = 8000;	/*


						 * *  * * DSP_DEFAULT_SPEED;  */
static int      dsp_busy = 0;
static int      dma16, dma8;
static unsigned long dsp_count = 0;

static int      irq_mode = IMODE_NONE;	/*


					 * *  * * IMODE_INPUT, IMODE_OUTPUT
					 * or * * IMODE_NONE   */
static int      my_dev = 0;

static volatile int intr_active = 0;

static int      sb16_dsp_open (int dev, int mode);
static void     sb16_dsp_close (int dev);
static void     sb16_dsp_output_block (int dev, unsigned long buf, int count, int intrflag, int dma_restart);
static void     sb16_dsp_start_input (int dev, unsigned long buf, int count, int intrflag, int dma_restart);
static int      sb16_dsp_ioctl (int dev, unsigned int cmd, unsigned int arg, int local);
static int      sb16_dsp_prepare_for_input (int dev, int bsize, int bcount);
static int      sb16_dsp_prepare_for_output (int dev, int bsize, int bcount);
static void     sb16_dsp_reset (int dev);
static void     sb16_dsp_halt (int dev);
static int      dsp_set_speed (int);
static int      dsp_set_stereo (int);
static void     dsp_cleanup (void);

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
  NULL
};

static int
sb_dsp_command01 (unsigned char val)
{
  int             i = 1 << 16;

  while (--i & (!INB (DSP_STATUS) & 0x80));
  if (!i)
    printk ("SB16 sb_dsp_command01 Timeout\n");
  return sb_dsp_command (val);
}

static int
dsp_set_speed (int mode)
{
  DEB (printk ("dsp_set_speed(%d)\n", mode));
  if (mode)
    {
      if (mode < 5000)
	mode = 5000;
      if (mode > 44100)
	mode = 44100;
      dsp_current_speed = mode;
    }
  return mode;
}

static int
dsp_set_stereo (int mode)
{
  DEB (printk ("dsp_set_stereo(%d)\n", mode));

  dsp_stereo = mode;

  return mode;
}

static int
dsp_set_bits (int arg)
{
  DEB (printk ("dsp_set_bits(%d)\n", arg));

  if (arg)
    switch (arg)
      {
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

static int
sb16_dsp_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  switch (cmd)
    {
    case SOUND_PCM_WRITE_RATE:
      if (local)
	return dsp_set_speed (arg);
      return IOCTL_OUT (arg, dsp_set_speed (IOCTL_IN (arg)));

    case SOUND_PCM_READ_RATE:
      if (local)
	return dsp_current_speed;
      return IOCTL_OUT (arg, dsp_current_speed);

    case SNDCTL_DSP_STEREO:
      if (local)
	return dsp_set_stereo (arg);
      return IOCTL_OUT (arg, dsp_set_stereo (IOCTL_IN (arg)));

    case SOUND_PCM_WRITE_CHANNELS:
      if (local)
	return dsp_set_stereo (arg - 1) + 1;
      return IOCTL_OUT (arg, dsp_set_stereo (IOCTL_IN (arg) - 1) + 1);

    case SOUND_PCM_READ_CHANNELS:
      if (local)
	return dsp_stereo + 1;
      return IOCTL_OUT (arg, dsp_stereo + 1);

    case SNDCTL_DSP_SETFMT:
      if (local)
	return dsp_set_bits (arg);
      return IOCTL_OUT (arg, dsp_set_bits (IOCTL_IN (arg)));

    case SOUND_PCM_READ_BITS:
      if (local)
	return dsp_16bit ? 16 : 8;
      return IOCTL_OUT (arg, dsp_16bit ? 16 : 8);

    case SOUND_PCM_WRITE_FILTER:	/*
					 * NOT YET IMPLEMENTED
					 */
      if (IOCTL_IN (arg) > 1)
	return IOCTL_OUT (arg, RET_ERROR (EINVAL));
    default:
      return RET_ERROR (EINVAL);
    }

  return RET_ERROR (EINVAL);
}

static int
sb16_dsp_open (int dev, int mode)
{
  int             retval;

  DEB (printk ("sb16_dsp_open()\n"));
  if (!sb16_dsp_ok)
    {
      printk ("SB16 Error: SoundBlaster board not installed\n");
      return RET_ERROR (ENXIO);
    }

  if (intr_active)
    return RET_ERROR (EBUSY);

  retval = sb_get_irq ();
  if (retval < 0)
    return retval;

  sb_reset_dsp ();

  if (ALLOC_DMA_CHN (dma8, "SB16 (8bit)"))
    {
      printk ("SB16: Unable to grab DMA%d\n", dma8);
      sb_free_irq ();
      return RET_ERROR (EBUSY);
    }

  if (dma16 != dma8)
    if (ALLOC_DMA_CHN (dma16, "SB16 (16bit)"))
      {
	printk ("SB16: Unable to grab DMA%d\n", dma16);
	sb_free_irq ();
	RELEASE_DMA_CHN (dma8);
	return RET_ERROR (EBUSY);
      }

  irq_mode = IMODE_NONE;
  dsp_busy = 1;

  return 0;
}

static void
sb16_dsp_close (int dev)
{
  unsigned long   flags;

  DEB (printk ("sb16_dsp_close()\n"));
  sb_dsp_command01 (0xd9);
  sb_dsp_command01 (0xd5);

  DISABLE_INTR (flags);
  RELEASE_DMA_CHN (dma8);

  if (dma16 != dma8)
    RELEASE_DMA_CHN (dma16);
  sb_free_irq ();
  dsp_cleanup ();
  dsp_busy = 0;
  RESTORE_INTR (flags);
}

static void
sb16_dsp_output_block (int dev, unsigned long buf, int count, int intrflag, int dma_restart)
{
  unsigned long   flags, cnt;

  cnt = count;
  if (dsp_16bit)
    cnt >>= 1;
  cnt--;

#ifdef DEB_DMARES
  printk ("output_block: %x %d %d\n", buf, count, intrflag);
  if (intrflag)
    {
      int             pos, chan = audio_devs[dev]->dmachan;

      DISABLE_INTR (flags);
      clear_dma_ff (chan);
      disable_dma (chan);
      pos = get_dma_residue (chan);
      enable_dma (chan);
      RESTORE_INTR (flags);
      printk ("dmapos=%d %x\n", pos, pos);
    }
#endif
  if (audio_devs[dev]->flags & DMA_AUTOMODE &&
      intrflag &&
      cnt == dsp_count)
    {
      irq_mode = IMODE_OUTPUT;
      intr_active = 1;
      return;			/*
				 * Auto mode on. No need to react
				 */
    }
  DISABLE_INTR (flags);

  if (dma_restart)
    {
      sb16_dsp_halt (dev);
      DMAbuf_start_dma (dev, buf, count, DMA_MODE_WRITE);
    }
  sb_dsp_command (0x41);
  sb_dsp_command ((unsigned char) ((dsp_current_speed >> 8) & 0xff));
  sb_dsp_command ((unsigned char) (dsp_current_speed & 0xff));
  sb_dsp_command ((unsigned char) (dsp_16bit ? 0xb6 : 0xc6));
  sb_dsp_command ((unsigned char) ((dsp_stereo ? 0x20 : 0) +
				   (dsp_16bit ? 0x10 : 0)));
  sb_dsp_command01 ((unsigned char) (cnt & 0xff));
  sb_dsp_command ((unsigned char) (cnt >> 8));

  dsp_count = cnt;
  irq_mode = IMODE_OUTPUT;
  intr_active = 1;
  RESTORE_INTR (flags);
}

static void
sb16_dsp_start_input (int dev, unsigned long buf, int count, int intrflag, int dma_restart)
{
  unsigned long   flags, cnt;

  cnt = count;
  if (dsp_16bit)
    cnt >>= 1;
  cnt--;

#ifdef DEB_DMARES
  printk ("start_input: %x %d %d\n", buf, count, intrflag);
  if (intrflag)
    {
      int             pos, chan = audio_devs[dev]->dmachan;

      DISABLE_INTR (flags);
      clear_dma_ff (chan);
      disable_dma (chan);
      pos = get_dma_residue (chan);
      enable_dma (chan);
      RESTORE_INTR (flags);
      printk ("dmapos=%d %x\n", pos, pos);
    }
#endif
  if (audio_devs[dev]->flags & DMA_AUTOMODE &&
      intrflag &&
      cnt == dsp_count)
    {
      irq_mode = IMODE_INPUT;
      intr_active = 1;
      return;			/*
				 * Auto mode on. No need to react
				 */
    }
  DISABLE_INTR (flags);

  if (dma_restart)
    {
      sb_reset_dsp ();
      DMAbuf_start_dma (dev, buf, count, DMA_MODE_READ);
    }

  sb_dsp_command (0x42);
  sb_dsp_command ((unsigned char) ((dsp_current_speed >> 8) & 0xff));
  sb_dsp_command ((unsigned char) (dsp_current_speed & 0xff));
  sb_dsp_command ((unsigned char) (dsp_16bit ? 0xbe : 0xce));
  sb_dsp_command ((unsigned char) ((dsp_stereo ? 0x20 : 0) +
				   (dsp_16bit ? 0x10 : 0)));
  sb_dsp_command01 ((unsigned char) (cnt & 0xff));
  sb_dsp_command ((unsigned char) (cnt >> 8));

  dsp_count = cnt;
  irq_mode = IMODE_INPUT;
  intr_active = 1;
  RESTORE_INTR (flags);
}

static int
sb16_dsp_prepare_for_input (int dev, int bsize, int bcount)
{
  audio_devs[my_dev]->dmachan = dsp_16bit ? dma16 : dma8;
  dsp_count = 0;
  dsp_cleanup ();
  return 0;
}

static int
sb16_dsp_prepare_for_output (int dev, int bsize, int bcount)
{
  audio_devs[my_dev]->dmachan = dsp_16bit ? dma16 : dma8;
  dsp_count = 0;
  dsp_cleanup ();
  return 0;
}

static void
dsp_cleanup (void)
{
  irq_mode = IMODE_NONE;
  intr_active = 0;
}

static void
sb16_dsp_reset (int dev)
{
  unsigned long   flags;

  DISABLE_INTR (flags);

  sb_reset_dsp ();
  dsp_cleanup ();

  RESTORE_INTR (flags);
}

static void
sb16_dsp_halt (int dev)
{
  if (dsp_16bit)
    {
      sb_dsp_command01 (0xd9);
      sb_dsp_command01 (0xd5);
    }
  else
    {
      sb_dsp_command01 (0xda);
      sb_dsp_command01 (0xd0);
    }
  DMAbuf_reset_dma (dev);
}

static void
set_irq_hw (int level)
{
  int             ival;

  switch (level)
    {
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
      printk ("SB16_IRQ_LEVEL %d does not exist\n", level);
      return;
    }
  sb_setmixer (IRQ_NR, ival);
}

long
sb16_dsp_init (long mem_start, struct address_info *hw_config)
{
  if (sbc_major < 4)
    return mem_start;		/* Not a SB16 */

  sprintf (sb16_dsp_operations.name, "SoundBlaster 16 %d.%d", sbc_major, sbc_minor);

#if defined(__FreeBSD__)
  printk ("sbxvo0: <%s>", sb16_dsp_operations.name);
#else
  printk (" <%s>", sb16_dsp_operations.name);
#endif

  if (num_audiodevs < MAX_AUDIO_DEV)
    {
      audio_devs[my_dev = num_audiodevs++] = &sb16_dsp_operations;
      audio_devs[my_dev]->dmachan = hw_config->dma;
      audio_devs[my_dev]->buffcount = 1;
      audio_devs[my_dev]->buffsize = DSP_BUFFSIZE;
    }
  else
    printk ("SB: Too many DSP devices available\n");
  sb16_dsp_ok = 1;
  return mem_start;
}

int
sb16_dsp_detect (struct address_info *hw_config)
{
  struct address_info *sb_config;

  if (sb16_dsp_ok)
    return 1;			/* Can't drive two cards */

  if (!(sb_config = sound_getconf (SNDCARD_SB)))
    {
      printk ("SB16 Error: Plain SB not configured\n");
      return 0;
    }

  /*
   * sb_setmixer(OPSW,0xf); if(sb_getmixer(OPSW)!=0xf) return 0;
   */

  if (!sb_reset_dsp ())
    return 0;

  if (sbc_major < 4)		/* Set by the plain SB driver */
    return 0;			/* Not a SB16 */

#ifdef PC98
  hw_config->dma = sb_config->dma;
#else
  if (hw_config->dma < 4)
    if (hw_config->dma != sb_config->dma)
      {
	printk ("SB16 Error: Invalid DMA channel %d/%d\n",
		sb_config->dma, hw_config->dma);
	return 0;
      }
#endif

  dma16 = hw_config->dma;
  dma8 = sb_config->dma;
  set_irq_hw (sb_config->irq);
#ifdef PC98
  sb_setmixer (DMA_NR, hw_config->dma == 0 ? 1 : 2);
#else
  sb_setmixer (DMA_NR, (1 << hw_config->dma) | (1 << sb_config->dma));
#endif

  DEB (printk ("SoundBlaster 16: IRQ %d DMA %d OK\n", sb_config->irq, hw_config->dma));

  /*
     * dsp_showmessage(0xe3,99);
   */
  sb16_dsp_ok = 1;
  return 1;
}

void
sb16_dsp_interrupt (int unused)
{
  int             data;

  data = INB (DSP_DATA_AVL16);	/*
				 * Interrupt acknowledge
				 */

  if (intr_active)
    switch (irq_mode)
      {
      case IMODE_OUTPUT:
	intr_active = 0;
	DMAbuf_outputintr (my_dev, 1);
	break;

      case IMODE_INPUT:
	intr_active = 0;
	DMAbuf_inputintr (my_dev);
	break;

      default:
	printk ("SoundBlaster: Unexpected interrupt\n");
      }
}

#endif
