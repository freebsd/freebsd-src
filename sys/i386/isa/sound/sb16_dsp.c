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
   #define DEB_DMARES
 */
#include "sound_config.h"
#include "sb.h"
#include "sb_mixer.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SB16) && !defined(EXCLUDE_SB) && !defined(EXCLUDE_AUDIO) && !defined(EXCLUDE_SBPRO)

extern int      sbc_base;

static int      sb16_dsp_ok = 0;	/* Set to 1 after successful initialization */
static int      dsp_16bit = 0;
static int      dsp_stereo = 0;
static int      dsp_current_speed = 8000;	/*DSP_DEFAULT_SPEED; */
static int      dsp_busy = 0;
static int      dma16, dma8;
static unsigned long dsp_count = 0;

static int      irq_mode = IMODE_NONE;	/* IMODE_INPUT, IMODE_OUTPUT or

					   IMODE_NONE */
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
int             sb_reset_dsp (void);

static struct audio_operations sb16_dsp_operations =
{
  "SoundBlaster 16",
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
wait_data_avail (int t)
{
  int             loopc = 5000000;

  t += GET_TIME ();
  do
    {
      if (INB (DSP_DATA_AVAIL) & 0x80)
	return 1;
    }
  while (--loopc && GET_TIME () < t);
  printk ("!data_avail l=%d\n", loopc);
  return 0;
}

static int 
read_dsp (int t)
{
  if (!wait_data_avail (t))
    return -1;
  else
    return INB (DSP_READ);
}

static int 
dsp_ini2 (void)
{
#if 0
  /* sb_setmixer(0x83, sb_getmixer(0x83) | 0x03);       */
  sb_dsp_command (0xe2);
  sb_dsp_command (0x76);	/* E0 ??? */
  sb_dsp_command (0xe2);
  sb_dsp_command (0x30);	/* A0 ??? */
  sb_dsp_command (0xe4);
  sb_dsp_command (0xaa);
  sb_dsp_command (0xe8);
  if (read_dsp (100) != 0xaa)
    printk ("Error dsp_ini2\n");
#endif
  return 0;
}
/*
   static char *dsp_getmessage(unsigned char command,int maxn)
   {
   static char buff[100];
   int n=0;

   sb_dsp_command(command);
   while(n<maxn && wait_data_avail(2)) {
   buff[++n]=INB(DSP_READ);
   if(!buff[n])
   break;
   }
   buff[0]=n;
   return buff;
   }

   static void dsp_showmessage(unsigned char command,int len)
   {
   int n;
   unsigned char *c;
   c=dsp_getmessage(command,len);
   printk("DSP C=%x l=%d,lr=%d b=",command,len,c[0]);
   for(n=1;n<=c[0];n++)
   if(c[n]>=' ' & c[n]<='z')
   printk("%c",c[n]);
   else
   printk("|%x|",c[n]);
   printk("\n");
   }
 */
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
	return RET_ERROR (EINVAL);
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

    case SNDCTL_DSP_SAMPLESIZE:
      if (local)
	return dsp_set_bits (arg);
      return IOCTL_OUT (arg, dsp_set_bits (IOCTL_IN (arg)));

    case SOUND_PCM_READ_BITS:
      if (local)
	return dsp_16bit ? 16 : 8;
      return IOCTL_OUT (arg, dsp_16bit ? 16 : 8);

    case SOUND_PCM_WRITE_FILTER:	/* NOT YET IMPLEMENTED */
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

  if (ALLOC_DMA_CHN (dma8))
    {
      printk ("SB16: Unable to grab DMA%d\n", dma8);
      sb_free_irq ();
      return RET_ERROR (EBUSY);
    }

  if (dma16 != dma8)
    if (ALLOC_DMA_CHN (dma16))
      {
	printk ("SB16: Unable to grab DMA%d\n", dma16);
	sb_free_irq ();
	RELEASE_DMA_CHN (dma8);
	return RET_ERROR (EBUSY);
      }

  dsp_ini2 ();

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
      int             pos, chan = sound_dsp_dmachan[dev];

      DISABLE_INTR (flags);
      clear_dma_ff (chan);
      disable_dma (chan);
      pos = get_dma_residue (chan);
      enable_dma (chan);
      RESTORE_INTR (flags);
      printk ("dmapos=%d %x\n", pos, pos);
    }
#endif
  if (sound_dma_automode[dev] &&
      intrflag &&
      cnt == dsp_count)
    {
      irq_mode = IMODE_OUTPUT;
      intr_active = 1;
      return;			/* Auto mode on. No need to react */
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
  /* sb_dsp_command (0);
     sb_dsp_command (0); */

  RESTORE_INTR (flags);
  dsp_count = cnt;
  irq_mode = IMODE_OUTPUT;
  intr_active = 1;
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
      int             pos, chan = sound_dsp_dmachan[dev];

      DISABLE_INTR (flags);
      clear_dma_ff (chan);
      disable_dma (chan);
      pos = get_dma_residue (chan);
      enable_dma (chan);
      RESTORE_INTR (flags);
      printk ("dmapos=%d %x\n", pos, pos);
    }
#endif
  if (sound_dma_automode[dev] &&
      intrflag &&
      cnt == dsp_count)
    {
      irq_mode = IMODE_INPUT;
      intr_active = 1;
      return;			/* Auto mode on. No need to react */
    }
  DISABLE_INTR (flags);

  if (dma_restart)
    {
      sb16_dsp_halt (dev);
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

  /* sb_dsp_command (0);
     sb_dsp_command (0); */
  RESTORE_INTR (flags);
  dsp_count = cnt;
  irq_mode = IMODE_INPUT;
  intr_active = 1;
}

static int
sb16_dsp_prepare_for_input (int dev, int bsize, int bcount)
{
  sound_dsp_dmachan[my_dev] = dsp_16bit ? dma16 : dma8;
  dsp_count = 0;
  dsp_cleanup ();
  return 0;
}

static int
sb16_dsp_prepare_for_output (int dev, int bsize, int bcount)
{
  sound_dsp_dmachan[my_dev] = dsp_16bit ? dma16 : dma8;
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
}

static void
set_irq_hw (int level)
{
  int             ival;

  switch (level)
    {
    case 5:
      ival = 2;
      break;
    case 7:
      ival = 4;
      break;
    case 10:
      ival = 8;
      break;
    default:
      printk ("SB16_IRQ_LEVEL %d does not exist\n", level);
      return;
    }
  sb_setmixer (IRQ_NR, ival);
}

long
sb16_dsp_init (long mem_start, struct address_info *hw_config)
{
  int             i, major, minor;

  major = minor = 0;
  sb_dsp_command (0xe1);	/* Get version */

  for (i = 1000; i; i--)
    {
      if (INB (DSP_DATA_AVAIL) & 0x80)
	{			/* wait for Data Ready */
	  if (major == 0)
	    major = INB (DSP_READ);
	  else
	    {
	      minor = INB (DSP_READ);
	      break;
	    }
	}
    }

#ifndef SCO
  sprintf (sb16_dsp_operations.name, "SoundBlaster 16 %d.%d", major, minor);
#endif

  printk ("snd6: <%s>", sb16_dsp_operations.name);

  if (num_dspdevs < MAX_DSP_DEV)
    {
      dsp_devs[my_dev = num_dspdevs++] = &sb16_dsp_operations;
      sound_dsp_dmachan[my_dev] = hw_config->dma;
      sound_buffcounts[my_dev] = 1;
      sound_buffsizes[my_dev] = DSP_BUFFSIZE;
      sound_dma_automode[my_dev] = 1;
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
    return 1;			/* Already initialized */

  if (!(sb_config = sound_getconf (SNDCARD_SB)))
    {
      printk ("SB16 Error: Plain SB not configured\n");
      return 0;
    }

  if (sbc_base != hw_config->io_base)
    printk ("Warning! SB16 I/O != SB I/O\n");

  /* sb_setmixer(OPSW,0xf);
     if(sb_getmixer(OPSW)!=0xf)
     return 0; */

  if (!sb_reset_dsp ())
    return 0;

  if (hw_config->irq != sb_config->irq)
    {
      printk ("SB16 Error: Invalid IRQ number %d/%d\n",
	      sb_config->irq, hw_config->irq);
      return 0;
    }

  if (hw_config->dma < 4)
    if (hw_config->dma != sb_config->dma)
      {
	printk ("SB16 Error: Invalid DMA channel %d/%d\n",
		sb_config->dma, hw_config->dma);
	return 0;
      }

  dma16 = hw_config->dma;
  dma8 = sb_config->dma;
  set_irq_hw (hw_config->irq);
  sb_setmixer (DMA_NR, (1 << hw_config->dma) | (1 << sb_config->dma));

  DEB (printk ("SoundBlaster 16: IRQ %d DMA %d OK\n", hw_config->irq, hw_config->dma));

/*
   dsp_showmessage(0xe3,99);
 */
  sb16_dsp_ok = 1;
  return 1;
}

void
sb16_dsp_interrupt (int unused)
{
  int             data;

  data = INB (DSP_DATA_AVL16);	/* Interrupt acknowledge */

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
