/*
 * sound/386bsd/soundcard.c
 * 
 * Soundcard driver for FreeBSD.
 * 
 * Copyright by Hannu Savolainen 1993
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * soundcard.c,v 1.20 1994/10/01 02:17:14 swallace Exp
 */

#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#include "dev_table.h"

u_int	snd1_imask;
u_int	snd2_imask;
u_int	snd3_imask;
u_int	snd4_imask;
u_int	snd5_imask;
u_int	snd6_imask;
u_int	snd7_imask;
u_int	snd8_imask;
u_int	snd9_imask;

#define FIX_RETURN(ret) {if ((ret)<0) return -(ret); else return 0;}

static int      timer_running = 0;

static int      soundcards_installed = 0;	/* Number of installed
						 * soundcards */
static int      soundcard_configured = 0;

static struct fileinfo files[SND_NDEVS];

int             sndprobe (struct isa_device *dev);
int             sndattach (struct isa_device *dev);
int             sndopen (dev_t dev, int flags);
int             sndclose (dev_t dev, int flags);
int             sndioctl (dev_t dev, int cmd, caddr_t arg, int mode);
int             sndread (int dev, struct uio *uio);
int             sndwrite (int dev, struct uio *uio);
int             sndselect (int dev, int rw);
static void	sound_mem_init(void);

unsigned
long
get_time(void)
{
struct timeval timecopy;
int x;

   x = splclock();
   timecopy = time;
   splx(x);
   return timecopy.tv_usec/(1000000/HZ) +
	  (unsigned long)timecopy.tv_sec*HZ;
}
 

int
sndread (int dev, struct uio *buf)
{
  int             count = buf->uio_resid;

  dev = minor (dev);

  FIX_RETURN (sound_read_sw (dev, &files[dev], buf, count));
}

int
sndwrite (int dev, struct uio *buf)
{
  int             count = buf->uio_resid;

  dev = minor (dev);

  FIX_RETURN (sound_write_sw (dev, &files[dev], buf, count));
}

int
sndopen (dev_t dev, int flags)
{
  int             retval;

  dev = minor (dev);

  if (!soundcard_configured && dev)
    {
      printk ("SoundCard Error: The soundcard system has not been configured\n");
      FIX_RETURN (-ENODEV);
    }

  files[dev].mode = 0;

  if (flags & FREAD && flags & FWRITE)
    files[dev].mode = OPEN_READWRITE;
  else if (flags & FREAD)
    files[dev].mode = OPEN_READ;
  else if (flags & FWRITE)
    files[dev].mode = OPEN_WRITE;

  FIX_RETURN(sound_open_sw (dev, &files[dev]));
}

int
sndclose (dev_t dev, int flags)
{

  dev = minor (dev);

  sound_release_sw(dev, &files[dev]);
  FIX_RETURN (0);
}

int
sndioctl (dev_t dev, int cmd, caddr_t arg, int mode)
{
  dev = minor (dev);

  FIX_RETURN (sound_ioctl_sw (dev, &files[dev], cmd, (unsigned int) arg));
}

int
sndselect (int dev, int rw)
{
  dev = minor (dev);

  DEB (printk ("sound_ioctl(dev=%d, cmd=0x%x, arg=0x%x)\n", dev, cmd, arg));

  FIX_RETURN (0);
}

static unsigned short
ipri_to_irq (unsigned short ipri)
{
  /*
   * Converts the ipri (bitmask) to the corresponding irq number
   */
  int             irq;

  for (irq = 0; irq < 16; irq++)
    if (ipri == (1 << irq))
      return irq;

  return -1;			/* Invalid argument */
}

int
sndprobe (struct isa_device *dev)
{
  struct address_info hw_config;

  hw_config.io_base = dev->id_iobase;
  hw_config.irq = ipri_to_irq (dev->id_irq);
  hw_config.dma = dev->id_drq;
  
  return sndtable_probe (dev->id_unit, &hw_config);
}

int
sndattach (struct isa_device *dev)
{
  int             i;
  static int      midi_initialized = 0;
  static int      seq_initialized = 0;
  static int 	  generic_midi_initialized = 0; 
  unsigned long	  mem_start = 0xefffffffUL;
  struct address_info hw_config;

  hw_config.io_base = dev->id_iobase;
  hw_config.irq = ipri_to_irq (dev->id_irq);
  hw_config.dma = dev->id_drq;

  if (dev->id_unit)		/* Card init */
    if (!sndtable_init_card (dev->id_unit, &hw_config))
      {
	printf (" <Driver not configured>");
	return FALSE;
      }

  /*
   * Init the high level sound driver
   */

  if (!(soundcards_installed = sndtable_get_cardcount ()))
    {
      printf (" <No such hardware>");
      return FALSE;		/* No cards detected */
    }

  printf("\n");

#ifndef EXCLUDE_AUDIO
  if (num_audiodevs)	/* Audio devices present */
    {
      mem_start = DMAbuf_init (mem_start);
      mem_start = audio_init (mem_start);
      sound_mem_init ();
    }

  soundcard_configured = 1;
#endif

  if (num_midis && !midi_initialized)
    {
      midi_initialized = 1;
      mem_start = MIDIbuf_init (mem_start);
    }

  if ((num_midis + num_synths) && !seq_initialized)
    {
      seq_initialized = 1;
      mem_start = sequencer_init (mem_start);
    }

  return TRUE;
}

void
tenmicrosec (void)
{
  int             i;

  for (i = 0; i < 16; i++)
    inb (0x80);
}

void
request_sound_timer (int count)
{
  static int      current = 0;
  int             tmp = count;

  if (count < 0)
    timeout ((timeout_func_t)sequencer_timer, 0, -count);
  else
    {

      if (count < current)
	current = 0;		/* Timer restarted */

      count = count - current;

      current = tmp;

      if (!count)
	count = 1;

      timeout ((timeout_func_t)sequencer_timer, 0, count);
    }
  timer_running = 1;
}

void
sound_stop_timer (void)
{
  if (timer_running)
    untimeout ((timeout_func_t)sequencer_timer, 0);
  timer_running = 0;
}

#ifndef EXCLUDE_AUDIO
static void
sound_mem_init (void)
{
  int             i, dev;
  unsigned long   dma_pagesize;
  struct dma_buffparms *dmap;
  static unsigned long dsp_init_mask = 0;

  for (dev = 0; dev < num_audiodevs; dev++)	/* Enumerate devices */
    if (!(dsp_init_mask & (1 << dev)))	/* Not already done */
      if (audio_devs[dev]->buffcount > 0 && audio_devs[dev]->dmachan > 0)
	{
	  dsp_init_mask |= (1 << dev);
	  dmap = audio_devs[dev]->dmap;

	  if (audio_devs[dev]->flags & DMA_AUTOMODE)
	    audio_devs[dev]->buffcount = 1;

	  if (audio_devs[dev]->dmachan > 3 && audio_devs[dev]->buffsize > 65536)
	    dma_pagesize = 131072;	/* 128k */
	  else
	    dma_pagesize = 65536;

	  /* More sanity checks */

	  if (audio_devs[dev]->buffsize > dma_pagesize)
	    audio_devs[dev]->buffsize = dma_pagesize;
	  audio_devs[dev]->buffsize &= ~0xfff;	/* Truncate to n*4k */
	  if (audio_devs[dev]->buffsize < 4096)
	    audio_devs[dev]->buffsize = 4096;

	  /* Now allocate the buffers */

	  for (dmap->raw_count = 0; dmap->raw_count < audio_devs[dev]->buffcount; dmap->raw_count++)
	    {
	      char           *tmpbuf = (char *)vm_page_alloc_contig(audio_devs[dev]->buffsize, 0ul, 0xfffffful, dma_pagesize);

	      if (tmpbuf == NULL)
		{
		  printk ("snd: Unable to allocate %d bytes of buffer\n",
			  audio_devs[dev]->buffsize);
		  return;
		}

	      dmap->raw_buf[dmap->raw_count] = tmpbuf;
	      /*
	       * Use virtual address as the physical address, since
	       * isa_dmastart performs the phys address computation.
	       */
	      dmap->raw_buf_phys[dmap->raw_count] =
		(unsigned long) dmap->raw_buf[dmap->raw_count];
	    }
	}			/* for dev */

}

#endif

struct isa_driver snddriver =
{sndprobe, sndattach, "snd"};

int
snd_ioctl_return (int *addr, int value)
{
  if (value < 0)
    return value;		/* Error */
  suword (addr, value);
  return 0;
}

int
snd_set_irq_handler (int interrupt_level, void(*hndlr)(int))
{
  return 1;
}

void
snd_release_irq(int vect)
{
}

#endif
