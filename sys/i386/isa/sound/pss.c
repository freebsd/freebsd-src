/* Marc.Hoffman@analog.com

   This is a pss driver.

   it is based on Greg.Yukna@analog.com @file{host} for DOG

   Unfortunately I can't distribute the ld file needed to
   make the pss card to emulate the SB stuff.

   I have provided a simple interface to the PSS unlike the
   DOG version.  to download a new algorithm just cat it to
   /dev/pss 14,9.

   You really need to rebuild this with the synth.ld file

   get the <synth>.ld from your dos directory maybe
   voyetra\dsp001.ld

   ld2inc < synth.ld > synth-ld.h
   (make config does the same).

   rebuild

   Okay if you blow things away no problem just

   main(){ioctl(open("/dev/pss"),SNDCTL_PSS_RESET)};

   and everything will be okay.

   At first I was going to worry about applications that were using
   the sound stuff and disallow the use of /dev/pss.  But for
   now I figured it doesn't matter.

   And if you change algos all the other applications running die off
   due to DMA problems.  Yeah just pull the plug and watch em die.

   If the registers get hosed
   main(){ioctl(open("/dev/pss"),SNDCTL_PSS_SETUP_REGISTERS)};

   Probably everything else can be done via mmap

   Oh if you want to develop code for the ADSP-21xx or Program the
   1848 just send me mail and I will hook you up.

               marc.hoffman@analog.com

   */
#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_PSS)

#ifndef PSS_MSS_BASE
#define PSS_MSS_BASE 0
#endif

#ifndef PSS_MPU_BASE
#define PSS_MPU_BASE 0
#endif

#ifndef PSS_MPU_IRQ
#define PSS_MPU_IRQ 0
#endif

#undef DEB
#define DEB(x) x

#include "pss.h"

static int      pss_ok = 0;
static int      sb_ok = 0;

static int      pss_base;
static int      pss_irq;
static int      pss_dma;

static int      gamePort = 0;

static int      sbInt;
static int      cdPol;
static int      cdAddr = 0;	/* 0x340;	*/
static int      cdInt = 10;

/* Define these by hand in local.h */
static int      wssAddr = PSS_MSS_BASE;
static int      midiAddr = PSS_MPU_BASE;
static int      midiInt = PSS_MPU_IRQ;

static int      SoundPortAddress;
static int      SoundPortData;
static int      speaker = 1;


static struct pss_speaker default_speaker =
{0, 0, 0, PSS_STEREO};

DEFINE_WAIT_QUEUE (pss_sleeper, pss_sleep_flag);

#include "synth-ld.h"

static int      pss_download_boot (unsigned char *block, int size);
static int      pss_reset_dsp (void);

static inline void
pss_outpw (unsigned short port, unsigned short value)
{
  __asm__         __volatile__ ("outw %w0, %w1"
				:	/* no outputs */
				:"a"            (value), "d" (port));
}

static inline unsigned int
pss_inpw (unsigned short port)
{
  unsigned int    _v;
  __asm__         __volatile__ ("inw %w1,%w0"
				:"=a"           (_v):"d" (port), "0" (0));

  return _v;
}

static void
PSS_write (int data)
{
  int             i, limit;

  limit = GET_TIME () + 10;	/* The timeout is 0.1 seconds */
  /*
   * Note! the i<5000000 is an emergency exit. The dsp_command() is sometimes
   * called while interrupts are disabled. This means that the timer is
   * disabled also. However the timeout situation is a abnormal condition.
   * Normally the DSP should be ready to accept commands after just couple of
   * loops.
   */

  for (i = 0; i < 5000000 && GET_TIME () < limit; i++)
    {
      if (pss_inpw (pss_base + PSS_STATUS) & PSS_WRITE_EMPTY)
	{
	  pss_outpw (pss_base + PSS_DATA, data);
	  return;
	}
    }
  printk ("PSS: DSP Command (%04x) Timeout.\n", data);
  printk ("IRQ conflict???\n");
}


static void
pss_setaddr (int addr, int configAddr)
{
  int             val;

  val = pss_inpw (configAddr);
  val &= ADDR_MASK;
  val |= (addr << 4);
  pss_outpw (configAddr, val);
}

/*_____ pss_checkint
         This function tests an interrupt number to see if
	 it is available. It takes the interrupt button
	 as it's argument and returns TRUE if the interrupt
	 is ok.
*/
static int
pss_checkint (int intNum)
{
  int             val;
  int             ret;
  int             i;

  /*_____ Set the interrupt bits */
  switch (intNum)
    {
    case 3:
      val = pss_inpw (pss_base + PSS_CONFIG);
      val &= INT_MASK;
      val |= INT_3_BITS;
      pss_outpw (pss_base + PSS_CONFIG, val);
      break;
    case 5:
      val = pss_inpw (pss_base + PSS_CONFIG);
      val &= INT_MASK;
      val |= INT_5_BITS;
      pss_outpw (pss_base + PSS_CONFIG, val);
      break;
    case 7:
      val = pss_inpw (pss_base + PSS_CONFIG);
      val &= INT_MASK;
      val |= INT_7_BITS;
      pss_outpw (pss_base + PSS_CONFIG, val);
      break;
    case 9:
      val = pss_inpw (pss_base + PSS_CONFIG);
      val &= INT_MASK;
      val |= INT_9_BITS;
      pss_outpw (pss_base + PSS_CONFIG, val);
      break;
    case 10:
      val = pss_inpw (pss_base + PSS_CONFIG);
      val &= INT_MASK;
      val |= INT_10_BITS;
      pss_outpw (pss_base + PSS_CONFIG, val);
      break;
    case 11:
      val = pss_inpw (pss_base + PSS_CONFIG);
      val &= INT_MASK;
      val |= INT_11_BITS;
      pss_outpw (pss_base + PSS_CONFIG, val);
      break;
    case 12:
      val = pss_inpw (pss_base + PSS_CONFIG);
      val &= INT_MASK;
      val |= INT_12_BITS;
      pss_outpw (pss_base + PSS_CONFIG, val);
      break;
    default:
      printk ("unknown interrupt selected. %d\n", intNum);
      return 0;
    }

  /*_____ Set the interrupt test bit */
  val = pss_inpw (pss_base + PSS_CONFIG);
  val |= INT_TEST_BIT;
  pss_outpw (pss_base + PSS_CONFIG, val);

  /*_____ Check if the interrupt is in use */
  /*_____ Do it a few times in case there is a delay */
  ret = 0;
  for (i = 0; i < 5; i++)
    {
      val = pss_inpw (pss_base + PSS_CONFIG);
      if (val & INT_TEST_PASS)
	{
	  ret = 1;
	  break;
	}
    }
  /*_____ Clear the Test bit and the interrupt bits */
  val = pss_inpw (pss_base + PSS_CONFIG);
  val &= INT_TEST_BIT_MASK;
  val &= INT_MASK;
  pss_outpw (pss_base + PSS_CONFIG, val);
  return (ret);
}

/*____ pss_setint
        This function sets the correct bits in the
	configuration register to
	enable the chosen interrupt.
*/
static void
pss_setint (int intNum, int configAddress)
{
  int             val;

  switch (intNum)
    {
    case 0:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      pss_outpw (configAddress, val);
      break;
    case 3:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      val |= INT_3_BITS;
      pss_outpw (configAddress, val);
      break;
    case 5:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      val |= INT_5_BITS;
      pss_outpw (configAddress, val);
      break;
    case 7:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      val |= INT_7_BITS;
      pss_outpw (configAddress, val);
      break;
    case 9:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      val |= INT_9_BITS;
      pss_outpw (configAddress, val);
      break;
    case 10:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      val |= INT_10_BITS;
      pss_outpw (configAddress, val);
      break;
    case 11:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      val |= INT_11_BITS;
      pss_outpw (configAddress, val);
      break;
    case 12:
      val = pss_inpw (configAddress);
      val &= INT_MASK;
      val |= INT_12_BITS;
      pss_outpw (configAddress, val);
      break;
    default:
      printk ("pss_setint unknown int\n");
    }
}


/*____ pss_setsbint
        This function sets the correct bits in the
	SoundBlaster configuration PSS register to
	enable the chosen interrupt.
	It takes a interrupt button as its argument.
*/
static void
pss_setsbint (int intNum)
{
  int             val;
  int             sbConfigAddress;

  sbConfigAddress = pss_base + SB_CONFIG;
  switch (intNum)
    {
    case 3:
      val = pss_inpw (sbConfigAddress);
      val &= INT_MASK;
      val |= INT_3_BITS;
      pss_outpw (sbConfigAddress, val);
      break;
    case 5:
      val = pss_inpw (sbConfigAddress);
      val &= INT_MASK;
      val |= INT_5_BITS;
      pss_outpw (sbConfigAddress, val);
      break;
    case 7:
      val = pss_inpw (sbConfigAddress);
      val &= INT_MASK;
      val |= INT_7_BITS;
      pss_outpw (sbConfigAddress, val);
      break;
    default:
      printk ("pss_setsbint: unknown_int\n");
    }
}

/*____ pss_setsbdma
        This function sets the correct bits in the
	SoundBlaster configuration PSS register to
	enable the chosen DMA channel.
	It takes a DMA button as its argument.
*/
static void
pss_setsbdma (int dmaNum)
{
  int             val;
  int             sbConfigAddress;

  sbConfigAddress = pss_base + SB_CONFIG;

  switch (dmaNum)
    {
    case 1:
      val = pss_inpw (sbConfigAddress);
      val &= DMA_MASK;
      val |= DMA_1_BITS;
      pss_outpw (sbConfigAddress, val);
      break;
    default:
      printk ("Personal Sound System ERROR! pss_setsbdma: unknown_dma\n");
    }
}

/*____ pss_setwssdma
        This function sets the correct bits in the
	WSS configuration PSS register to
	enable the chosen DMA channel.
	It takes a DMA button as its argument.
*/
static void
pss_setwssdma (int dmaNum)
{
  int             val;
  int             wssConfigAddress;

  wssConfigAddress = pss_base + PSS_WSS_CONFIG;

  switch (dmaNum)
    {
    case 0:
      val = pss_inpw (wssConfigAddress);
      val &= DMA_MASK;
      val |= DMA_0_BITS;
      pss_outpw (wssConfigAddress, val);
      break;
    case 1:
      val = pss_inpw (wssConfigAddress);
      val &= DMA_MASK;
      val |= DMA_1_BITS;
      pss_outpw (wssConfigAddress, val);
      break;
    case 3:
      val = pss_inpw (wssConfigAddress);
      val &= DMA_MASK;
      val |= DMA_3_BITS;
      pss_outpw (wssConfigAddress, val);
      break;
    default:
      printk ("Personal Sound System ERROR! pss_setwssdma: unknown_dma\n");
    }
}


/*_____ SetSpeakerOut
         This function sets the Volume, Bass, Treble and Mode of
	 the speaker out channel.
	 */
void
pss_setspeaker (struct pss_speaker *spk)
{
  PSS_write (SET_MASTER_COMMAND);
  if (spk->volume > PHILLIPS_VOL_MAX)
    spk->volume = PHILLIPS_VOL_MAX;
  if (spk->volume < PHILLIPS_VOL_MIN)
    spk->volume = PHILLIPS_VOL_MIN;

  PSS_write (MASTER_VOLUME_LEFT
	     | (PHILLIPS_VOL_CONSTANT + spk->volume / PHILLIPS_VOL_STEP));
  PSS_write (SET_MASTER_COMMAND);
  PSS_write (MASTER_VOLUME_RIGHT
	     | (PHILLIPS_VOL_CONSTANT + spk->volume / PHILLIPS_VOL_STEP));

  if (spk->bass > PHILLIPS_BASS_MAX)
    spk->bass = PHILLIPS_BASS_MAX;
  if (spk->bass < PHILLIPS_BASS_MIN)
    spk->bass = PHILLIPS_BASS_MIN;
  PSS_write (SET_MASTER_COMMAND);
  PSS_write (MASTER_BASS
	     | (PHILLIPS_BASS_CONSTANT + spk->bass / PHILLIPS_BASS_STEP));

  if (spk->treb > PHILLIPS_TREBLE_MAX)
    spk->treb = PHILLIPS_TREBLE_MAX;
  if (spk->treb < PHILLIPS_TREBLE_MIN)
    spk->treb = PHILLIPS_TREBLE_MIN;
  PSS_write (SET_MASTER_COMMAND);
  PSS_write (MASTER_TREBLE
	   | (PHILLIPS_TREBLE_CONSTANT + spk->treb / PHILLIPS_TREBLE_STEP));

  PSS_write (SET_MASTER_COMMAND);
  PSS_write (MASTER_SWITCH | spk->mode);
}

static void
pss_init1848 (void)
{
  /*_____ Wait for 1848 to init */
  while (INB (SoundPortAddress) & SP_IN_INIT);

  /*_____ Wait for 1848 to autocal */
  OUTB (SoundPortAddress, SP_TEST_AND_INIT);
  while (INB (SoundPortData) & AUTO_CAL_IN_PROG);
}

static int
pss_configure_registers_to_look_like_sb (void)
{
  pss_setaddr (wssAddr, pss_base + PSS_WSS_CONFIG);

  SoundPortAddress = wssAddr + 4;
  SoundPortData = wssAddr + 5;

  DEB (printk ("Turning Game Port %s.\n",
	       gamePort ? "On" : "Off"));

  /*_____ Turn on the Game port */
  if (gamePort)
    pss_outpw (pss_base + PSS_STATUS,
	       pss_inpw (pss_base + PSS_STATUS) | GAME_BIT);
  else
    pss_outpw (pss_base + PSS_STATUS,
	       pss_inpw (pss_base + PSS_STATUS) & GAME_BIT_MASK);


  DEB (printk ("PSS attaching base %x irq %d dma %d\n",
	       pss_base, pss_irq, pss_dma));

  /* Check if sb is enabled if it is check the interrupt */
  pss_outpw (pss_base + SB_CONFIG, 0);

  if (pss_irq != 0)
    {
      DEB (printk ("PSS Emulating Sound Blaster ADDR %04x\n", pss_base));
      DEB (printk ("PSS SBC: attaching base %x irq %d dma %d\n",
		   SBC_BASE, SBC_IRQ, SBC_DMA));

      if (pss_checkint (SBC_IRQ) == 0)
	{
	  printk ("PSS! attach: int_error\n");
	  return 0;
	}

      pss_setsbint (SBC_IRQ);
      pss_setsbdma (SBC_DMA);
      sb_ok = 1;
    }
  else
    {
      sb_ok = 0;
      printk ("PSS: sound blaster error init\n");
    }

  /* Check if cd is enabled if it is check the interrupt */
  pss_outpw (pss_base + CD_CONFIG, 0);

  if (cdAddr != 0)
    {
      DEB (printk ("PSS:CD drive %x irq: %d", cdAddr, cdInt));
      if (cdInt != 0)
	{
	  if (pss_checkint (cdInt) == 0)
	    {
	      printk ("Can't allocate cdInt %d\n", cdInt);
	    }
	  else
	    {
	      int             val;

	      printk ("CD poll ");
	      pss_setaddr (cdAddr, pss_base + CD_CONFIG);
	      pss_setint (cdInt, pss_base + CD_CONFIG);

	      /* set the correct bit in the
		 configuration register to
		 set the irq polarity for the CD-Rom.
		 NOTE: This bit is in the address config
		 field, It must be configured after setting
		 the CD-ROM ADDRESS!!! */
	      val = pss_inpw (pss_base + CD_CONFIG);
	      pss_outpw (pss_base + CD_CONFIG, 0);
	      val &= CD_POL_MASK;
	      if (cdPol)
		val |= CD_POL_BIT;
	      pss_outpw (pss_base + CD_CONFIG, val);
	    }
	}
    }

  /* Check if midi is enabled if it is check the interrupt */
  pss_outpw (pss_base + MIDI_CONFIG, 0);
  if (midiAddr != 0)
    {
      printk ("midi init %x %d\n", midiAddr, midiInt);
      if (pss_checkint (midiInt) == 0)
	{
	  printk ("midi init int error %x %d\n", midiAddr, midiInt);
	}
      else
	{
	  pss_setaddr (midiAddr, pss_base + MIDI_CONFIG);
	  pss_setint (midiInt, pss_base + MIDI_CONFIG);
	}
    }
  return 1;
}

long
attach_pss (long mem_start, struct address_info *hw_config)
{
  if (pss_ok)
    {
      if (hw_config)
	{
	  printk (" <PSS-ESC614>");
	}

      return mem_start;
    }

  pss_ok = 1;

  if (pss_configure_registers_to_look_like_sb () == 0)
    return mem_start;

  if (sb_ok)
    if (pss_synthLen
	&& pss_download_boot (pss_synth, pss_synthLen))
      {
	if (speaker)
	  pss_setspeaker (&default_speaker);
	pss_ok = 1;
      }
    else
      pss_reset_dsp ();

  return mem_start;
}

int
probe_pss (struct address_info *hw_config)
{
  pss_base = hw_config->io_base;
  pss_irq = hw_config->irq;
  pss_dma = hw_config->dma;

  if ((pss_inpw (pss_base + 4) & 0xff00) == 0x4500)
    {
      attach_pss (0, hw_config);
      return 1;
    }
  printk (" fail base %x irq %d dma %d\n", pss_base, pss_irq, pss_dma);
  return 0;
}


static int
pss_reattach (void)
{
  pss_ok = 0;
  attach_pss (0, 0);
  return 1;
}

static int
pss_reset_dsp ()
{
  unsigned long   i, limit = GET_TIME () + 10;

  pss_outpw (pss_base + PSS_CONTROL, 0x2000);

  for (i = 0; i < 32768 && GET_TIME () < limit; i++)
    pss_inpw (pss_base + PSS_CONTROL);

  pss_outpw (pss_base + PSS_CONTROL, 0x0000);

  return 1;
}


static int
pss_download_boot (unsigned char *block, int size)
{
  int             i, limit, val, count;

  printk ("PSS: downloading boot code synth.ld... ");

  /*_____ Warn DSP software that a boot is coming */
  pss_outpw (pss_base + PSS_DATA, 0x00fe);

  limit = GET_TIME () + 10;

  for (i = 0; i < 32768 && GET_TIME () < limit; i++)
    if (pss_inpw (pss_base + PSS_DATA) == 0x5500)
      break;

  pss_outpw (pss_base + PSS_DATA, *block++);

  pss_reset_dsp ();
  printk ("start ");

  count = 1;
  while (1)
    {
      int             j;

      for (j = 0; j < 327670; j++)
	{
	  /*_____ Wait for BG to appear */
	  if (pss_inpw (pss_base + PSS_STATUS) & PSS_FLAG3)
	    break;
	}

      if (j == 327670)
	{
	  /* It's ok we timed out when the file was empty */
	  if (count >= size)
	    break;
	  else
	    {
	      printk ("\nPSS: DownLoad timeout problems, byte %d=%d\n",
		      count, size);
	      return 0;
	    }
	}
      /*_____ Send the next byte */
      pss_outpw (pss_base + PSS_DATA, *block++);
      count++;
    }

  /*_____ Why */
  pss_outpw (pss_base + PSS_DATA, 0);

  limit = GET_TIME () + 10;
  for (i = 0; i < 32768 && GET_TIME () < limit; i++)
    val = pss_inpw (pss_base + PSS_STATUS);

  printk ("downloaded\n");

  limit = GET_TIME () + 10;
  for (i = 0; i < 32768 && GET_TIME () < limit; i++)
    {
      val = pss_inpw (pss_base + PSS_STATUS);
      if (val & 0x4000)
	break;
    }

  /* now read the version */
  for (i = 0; i < 32000; i++)
    {
      val = pss_inpw (pss_base + PSS_STATUS_REG);
      if (val & PSS_READ_FULL)
	break;
    }
  if (i == 32000)
    return 0;

  val = pss_inpw (pss_base + PSS_DATA_REG);

  return 1;
}


/* The following is a simple device driver for the pss.
   All I really care about is communication to and from the pss.

   The ability to reinitialize the <synth.ld>  This will be
   default when release is chosen.

   SNDCTL_PSS_DOWNLOAD:

   Okay we need to creat new minor numbers for the
   DOWNLOAD functionality.

   14,0x19 -- /dev/pssld where a read operation would output the
                         current ld to user space
                         where a write operation would effectively
			 download a new ld.

   14,0x09 -- /dev/psecho  would open up a communication path to the
                         esc614 asic.  Given the ability to send
			 messages to the asic and receive messages too.

			 All messages would get read and written in the
			 same manner.  It would be up to the application
			 and the ld to maintain a relationship
			 of what the messages mean.
			
			 for this device we need to implement select. */
#define CODE_BUFFER_LEN (64*1024)
static char    *code_buffer;
static int      code_length;

static int      lock_pss = 0;

int
pss_open (int dev, struct fileinfo *file)
{
  int             mode;

  DEB (printk ("pss_open\n"));

  if (pss_ok == 0)
    return RET_ERROR (EIO);

  if (lock_pss)
    return 0;

  lock_pss = 1;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;
  if (mode == O_WRONLY)
    {
      printk ("pss-open for WRONLY\n");
      code_length = 0;
    }

  RESET_WAIT_QUEUE (pss_sleeper, pss_sleep_flag);
  return 1;
}

void
pss_release (int dev, struct fileinfo *file)
{
  int             mode;

  DEB (printk ("pss_release\n"));
  if (pss_ok == 0)
    return RET_ERROR (EIO);

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;
  if (mode == O_WRONLY && code_length > 0)
    {
#ifdef linux
      /* This just allows interrupts while the conversion is running */
      __asm__ ("sti");
#endif
      if (!pss_download_boot (code_buffer, code_length))
	{
	  pss_reattach ();
	}
    }
  lock_pss = 0;
}

int
pss_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c, p;

  DEB (printk ("pss_read\n"));
  if (pss_ok == 0)
    return RET_ERROR (EIO);

  dev = dev >> 4;
  p = 0;
  c = count;

  return count - c;
}

int
pss_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  DEB (printk ("pss_write\n"));
  if (pss_ok == 0)
    return RET_ERROR (EIO);
  dev = dev >> 4;

  if (count)			/* Flush output */
    {
      COPY_FROM_USER (&code_buffer[code_length], buf, 0, count);
      code_length += count;
    }
  return count;
}


int
pss_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg)
{
  DEB (printk ("pss_ioctl dev=%d cmd=%x\n", dev, cmd));
  if (pss_ok == 0)
    return RET_ERROR (EIO);

  dev = dev >> 4;

  switch (cmd)
    {
    case SNDCTL_PSS_RESET:
      pss_reattach ();
      return 1;

    case SNDCTL_PSS_SETUP_REGISTERS:
      pss_configure_registers_to_look_like_sb ();
      return 1;

    case SNDCTL_PSS_SPEAKER:
      {
	struct pss_speaker params;
	COPY_FROM_USER (&params, (char *) arg, 0, sizeof (struct pss_speaker));

	pss_setspeaker (&params);
	return 0;
      }
    default:
      return RET_ERROR (EIO);
    }
}

/* This is going to be used to implement
   waiting on messages sent from the DSP and to the
   DSP when communication is used via the pss directly.

   We need to find out if the pss can generate a different
   interrupt other than the one it has been setup for.

   This way we can carry on a conversation with the pss
   on a separate channel.  This would be useful for debugging. */

pss_select (int dev, struct fileinfo * file, int sel_type, select_table * wait)
{
  return 0;
  if (pss_ok == 0)
    return RET_ERROR (EIO);

  dev = dev >> 4;

  switch (sel_type)
    {
    case SEL_IN:
      select_wait (&pss_sleeper, wait);
      return 0;
      break;

    case SEL_OUT:
      select_wait (&pss_sleeper, wait);
      return 0;
      break;

    case SEL_EX:
      return 0;
    }

  return 0;
}

long
pss_init (long mem_start)
{
  DEB (printk ("pss_init\n"));
  if (pss_ok)
    {
      code_buffer = mem_start;
      mem_start += CODE_BUFFER_LEN;
    }
  return mem_start;
}

#endif
