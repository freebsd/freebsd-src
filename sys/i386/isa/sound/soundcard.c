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
 */

#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#include "dev_table.h"

int	__timeout_val = 0;
int   __process_aborting = 0;

u_int	snd1mask;
u_int	snd2mask;
u_int	snd3mask;
u_int	snd4mask;
u_int	snd5mask;

struct sbc_device
{
  int             usecount;
};

#define FIX_RETURN(ret) {if ((ret)<0) return -(ret); else return 0;}

static struct sbc_device sbc_devices[SND_NDEVS];
static int      timer_running = 0;

static int      in_use = 0;	/* Total # of open device files (excluding
				 * minor 0) */

static int      soundcards_installed = 0;	/* Number of installed
						 * soundcards */
static int      soundcard_configured = 0;
extern char    *snd_raw_buf[MAX_DSP_DEV][DSP_BUFFCOUNT];
extern unsigned long snd_raw_buf_phys[MAX_DSP_DEV][DSP_BUFFCOUNT];
extern int      snd_raw_count[MAX_DSP_DEV];

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

int
get_time()
{
extern struct timeval time;
 
   return(time.tv_usec + (time.tv_sec*1000000));
}
 

int
sndread (int dev, struct uio *buf)
{
  int             count = buf->uio_resid;

  dev = minor (dev);

  DEB (printk ("sound_read(dev=%d, count=%d)\n", dev, count));

  switch (dev & 0x0f) /* It really has to be 0x0f */
    {
    case SND_DEV_AUDIO:
      FIX_RETURN (audio_read (dev, &files[dev], buf, count));
      break;

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
      FIX_RETURN (dsp_read (dev, &files[dev], buf, count));
      break;

    case SND_DEV_SEQ:
      FIX_RETURN (sequencer_read (dev, &files[dev], buf, count));
      break;

#ifndef EXCLUDE_CHIP_MIDI
    case CMIDI_DEV_PRO: 
      FIX_RETURN (CMIDI_read (dev, &files[dev], buf, count));
  
      break;
#endif


#ifndef EXCLUDE_MPU401
    case SND_DEV_MIDIN:
      FIX_RETURN (MIDIbuf_read (dev, &files[dev], buf, count));
#endif

    default:
      ;
    }

  FIX_RETURN (-EPERM);
}

int
sndwrite (int dev, struct uio *buf)
{
  int             count = buf->uio_resid;

  DEB (printk ("sound_write(dev=%d, count=%d)\n", dev, count));

  dev = minor (dev);

  switch (dev & 0x0f) /* It really has to be 0x0f */ 
    {

    case SND_DEV_SEQ:
      FIX_RETURN (sequencer_write (dev, &files[dev], buf, count));
      break;

    case SND_DEV_AUDIO:
      FIX_RETURN (audio_write (dev, &files[dev], buf, count));
      break;

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
      FIX_RETURN (dsp_write (dev, &files[dev], buf, count));
      break;

#ifndef EXCLUDE_CHIP_MIDI
    case CMIDI_DEV_PRO: 
      FIX_RETURN (CMIDI_write (dev, &files[dev], buf, count));
      break;
#endif
      
    default:
      FIX_RETURN (-EPERM);
    }

  FIX_RETURN (count);
}

int
sndopen (dev_t dev, int flags)
{
  int             retval;

  dev = minor (dev);

 /* printf("SND: Minor number is now : %ld\n",dev); */

  DEB (printk ("sound_open(dev=%d) : usecount=%d\n", dev, sbc_devices[dev].usecount));

  if ((dev >= SND_NDEVS) || (dev < 0))
    {
      printk ("Invalid minor device %d\n", dev);
      FIX_RETURN (-ENODEV);
    }

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

  switch (dev & 0x0f) /* It has to be 0x0f. Trust me */ 
    {
    case SND_DEV_CTL:
      if (!soundcards_installed)
	if (soundcard_configured)
	  {
	    printk ("Soundcard not installed\n");
	    FIX_RETURN (-ENODEV);
	  }
      break;

    case SND_DEV_SEQ:
      if ((retval = sequencer_open (dev, &files[dev])) < 0)
	FIX_RETURN (retval);
      break;

/** UWM stuff **/

#ifndef EXCLUDE_CHIP_MIDI
    case CMIDI_DEV_PRO: 
     	FIX_RETURN ( CMIDI_open (dev, &files[dev]) ); 
  	break;	 
#endif


#ifndef EXCLUDE_MPU401
    case SND_DEV_MIDIN:
      if ((retval = MIDIbuf_open (dev, &files[dev])) < 0)
	FIX_RETURN (retval);
      break;
#endif

    case SND_DEV_AUDIO:
      if ((retval = audio_open (dev, &files[dev])) < 0)
	FIX_RETURN (retval);
      break;

    case SND_DEV_DSP:
      if ((retval = dsp_open (dev, &files[dev], 8)) < 0)
	FIX_RETURN (retval);
      break;

    case SND_DEV_DSP16:
      if ((retval = dsp_open (dev, &files[dev], 16)) < 0)
	FIX_RETURN (retval);
      break;

    default:
      printk ("Invalid minor device %d\n", dev);
      FIX_RETURN (-ENODEV);
    }

  sbc_devices[dev].usecount++;
  in_use++;

  FIX_RETURN (0);
}

int
sndclose (dev_t dev, int flags)
{

  dev = minor (dev);

  DEB (printk ("sound_release(dev=%d)\n", dev));

  switch (dev & 0x0f) /* Has to be 0x0f */
    {
    case SND_DEV_SEQ:
      sequencer_release (dev, &files[dev]);
      break;

#ifndef EXCLUDE_CHIP_MIDI
    case CMIDI_DEV_PRO: 
      CMIDI_close (dev, &files[dev]);
      break;
#endif
      
#ifndef EXCLUDE_MPU401
    case SND_DEV_MIDIN:
      MIDIbuf_release (dev, &files[dev]);
      break;
#endif

    case SND_DEV_AUDIO:
      audio_release (dev, &files[dev]);
      break;

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
      dsp_release (dev, &files[dev]);
      break;

    default:;
    }

  sbc_devices[dev].usecount--;
  in_use--;			/* If not control port */

  FIX_RETURN (0);
}

int
sndioctl (dev_t dev, int cmd, caddr_t arg, int mode)
{
  dev = minor (dev);

  DEB (printk ("sound_ioctl(dev=%d, cmd=0x%x, arg=0x%x)\n", dev, cmd, arg));

  switch (dev & 0x0f)
    {

    case SND_DEV_CTL:
      if (!num_mixers)
	FIX_RETURN (-ENODEV);

      if (dev >= num_mixers)
	FIX_RETURN (-ENODEV);

      FIX_RETURN (mixer_devs[dev]->ioctl (dev, cmd, (unsigned int) arg));
      break;

    case SND_DEV_SEQ:
      FIX_RETURN (sequencer_ioctl (dev, &files[dev], cmd, (unsigned int) arg));
      break;

    case SND_DEV_AUDIO:
      FIX_RETURN (audio_ioctl (dev, &files[dev], cmd, (unsigned int) arg));
      break;

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
      FIX_RETURN (dsp_ioctl (dev, &files[dev], cmd, (unsigned int) arg));
      break;

#ifndef EXCLUDE_MPU401
    case SND_DEV_MIDIN:
      FIX_RETURN (MIDIbuf_ioctl (dev, &files[dev], cmd, (unsigned int) arg));
      break;
#endif

    default:
      FIX_RETURN (-EPERM);
      break;
    }

  FIX_RETURN (-EPERM);
}

int
sndselect (int dev, int rw)
{
  dev = minor (dev);

  DEB (printk ("sound_ioctl(dev=%d, cmd=0x%x, arg=0x%x)\n", dev, cmd, arg));

  FIX_RETURN (0);
}

static short
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
  static int      dsp_initialized = 0;
  static int      midi_initialized = 0;
  static int      seq_initialized = 0;
  static int 	  generic_midi_initialized = 0; 
  unsigned long	  mem_start = 0xefffffff;
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
  soundcard_configured = 1;
  if (num_dspdevs)
    sound_mem_init ();
#endif

  if (num_dspdevs && !dsp_initialized)	/* Audio devices present */
    {
      dsp_initialized = 1;
      mem_start = DMAbuf_init (mem_start);
      mem_start = audio_init (mem_start);
      mem_start = dsp_init (mem_start);
    }

/** UWM stuff **/

#ifndef EXCLUDE_CHIP_MIDI

     if (!generic_midi_initialized)
     {
	 generic_midi_initialized = 1;
	 mem_start = CMIDI_init (mem_start);
     } 

#endif 

#ifndef EXCLUDE_MPU401
  if (num_midis && !midi_initialized)
    {
      midi_initialized = 1;
      mem_start = MIDIbuf_init (mem_start);
    }
#endif

  if ((num_midis + num_synths) && !seq_initialized)
    {
      seq_initialized = 1;
      mem_start = sequencer_init (mem_start);
    }

  for (i = 0; i < SND_NDEVS; i++)
    {
      sbc_devices[i].usecount = 0;
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

#ifdef EXCLUDE_GUS
void
gusintr (int unit)
{
  return (0);
}
#endif

void
request_sound_timer (int count)
{
  static int      current = 0;
  int             tmp = count;

  if (count < 0)
    timeout (sequencer_timer, 0, -count);
  else
    {

      if (count < current)
	current = 0;		/* Timer restarted */

      count = count - current;

      current = tmp;

      if (!count)
	count = 1;

      timeout (sequencer_timer, 0, count);
    }
  timer_running = 1;
}

void
sound_stop_timer (void)
{
  if (timer_running)
    untimeout (sequencer_timer, 0);
  timer_running = 0;
}

#ifndef EXCLUDE_AUDIO
static void
sound_mem_init (void)
{
  int             i, dev;
  unsigned long   dma_pagesize;
  static unsigned long dsp_init_mask = 0;

  for (dev = 0; dev < num_dspdevs; dev++)	/* Enumerate devices */
    if (!(dsp_init_mask & (1 << dev)))	/* Not already done */
      if (sound_buffcounts[dev] > 0 && sound_dsp_dmachan[dev] > 0)
	{
	  dsp_init_mask |= (1 << dev);

	  if (sound_dma_automode[dev])
	    {
	      sound_dma_automode[dev] = 0;	/* Not possible with FreeBSD */
	    }

	  if (sound_buffcounts[dev] == 1)
	    {
	      sound_buffcounts[dev] = 2;
	      sound_buffsizes[dev] /= 2;
	    }

	  if (sound_buffsizes[dev] > 65536)	/* Larger is not possible (yet) */
	    sound_buffsizes[dev] = 65536;

	  if (sound_dsp_dmachan[dev] > 3 && sound_buffsizes[dev] > 65536)
	    dma_pagesize = 131072;	/* 128k */
	  else
	    dma_pagesize = 65536;

	  /* More sanity checks */

	  if (sound_buffsizes[dev] > dma_pagesize)
	    sound_buffsizes[dev] = dma_pagesize;
	  sound_buffsizes[dev] &= 0xfffff000;	/* Truncate to n*4k */
	  if (sound_buffsizes[dev] < 4096)
	    sound_buffsizes[dev] = 4096;

	  /* Now allocate the buffers */

	  for (snd_raw_count[dev] = 0; snd_raw_count[dev] < sound_buffcounts[dev]; snd_raw_count[dev]++)
	    {
	      /*
	       * The DMA buffer allocation algorithm hogs memory. We allocate
	       * a memory area which is two times the requires size. This
	       * guarantees that it contains at least one valid DMA buffer.
	       * 
	       * This really needs some kind of finetuning.
	       */
	      char           *tmpbuf = malloc (2*sound_buffsizes[dev], M_DEVBUF, M_NOWAIT);
	      unsigned long   addr, rounded;

	      if (tmpbuf == NULL)
		{
		  printk ("snd: Unable to allocate %d bytes of buffer\n",
			  2 * sound_buffsizes[dev]);
		  return;
		}

	      addr = kvtop (tmpbuf);
	      /*
	       * Align the start address
	       */
	      rounded = (addr & ~(dma_pagesize - 1)) + dma_pagesize;

	      snd_raw_buf[dev][snd_raw_count[dev]] =
		&tmpbuf[rounded - addr];	/* Compute offset */
	      /*
	       * Use virtual address as the physical address, since
	       * isa_dmastart performs the phys address computation.
	       */
	      snd_raw_buf_phys[dev][snd_raw_count[dev]] =
		(unsigned long) snd_raw_buf[dev][snd_raw_count[dev]];
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

#endif
