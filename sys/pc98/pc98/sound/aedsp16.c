/*
   sound/aedsp16.c

   Audio Excel DSP 16 software configuration routines

   Copyright (C) 1995  Riccardo Facchetti (riccardo@cdc8g5.cdc.polimi.it)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met: 1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer. 2.
   Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   READ THIS

   This module is intended for Audio Excel DSP 16 Sound Card.

   Audio Excel DSP 16 is an SB pro II, Microsoft Sound System
   and MPU-401 compatible card.
   It is software-only configurable (no jumpers to hard-set irq/dma/mpu-irq),
   so before this module, the only way to configure the DSP under linux was
   boot the MS-BAU loading the sound.sys device driver (this driver soft-
   configure the sound board hardware by massaging someone of its registers),
   and then ctrl-alt-del to boot linux with the DSP configured by the DOG
   driver.

   This module works configuring your Audio Excel DSP 16's
   irq, dma and mpu-401-irq. The voxware probe routines rely on the
   fact that if the hardware is there, they can detect it. The problem
   with AEDSP16 is that no hardware can be found by the probe routines
   if the sound card is not well configured. Sometimes the kernel probe
   routines can find an SBPRO even when the card is not configured (this
   is the standard setup of the card), but the SBPRO emulation don't work
   well if the card is not properly initialized. For this reason

   InitAEDSP16_...()

   routines are called before the voxware probe routines try to detect the
   hardware.

   NOTE (READ THE NOTE TOO, IT CONTAIN USEFUL INFORMATIONS)

   The Audio Excel DSP 16 Sound Card emulates both SBPRO and MSS;
   the voxware sound driver can be configured for SBPRO and MSS cards
   at the same time, but the aedsp16 can't be two cards!!
   When we configure it, we have to choose the SBPRO or the MSS emulation
   for AEDSP16. We also can install a *REAL* card of the other type
   (see [1], not tested but I can't see any reason for it to fail).

   NOTE: If someone can test the combination AEDSP16+MSS or AEDSP16+SBPRO
   please let me know if it works.

   The MPU-401 support can be compiled in together with one of the other
   two operating modes.

   The board configuration calls, are in the probe_...() routines because
   we have to configure the board before probing it for a particular
   hardware. After card configuration, we can probe the hardware.

   NOTE: This is something like plug-and-play: we have only to plug
   the AEDSP16 board in the socket, and then configure and compile
   a kernel that uses the AEDSP16 software configuration capability.
   No jumper setting is needed!

   For example, if you want AEDSP16 to be an SBPro, on irq 10, dma 3
   you have just to make config the voxware package, configuring
   the SBPro sound card with that parameters, then when configure
   asks if you have an AEDSP16, answer yes. That's it.
   Compile the kernel and run it.

   NOTE: This means that you can choose irq and dma, but not the
   I/O addresses. To change I/O addresses you have to set them
   with jumpers.

   NOTE: InitAEDSP16_...() routines get as parameter the hw_config,
   the hardware configuration of the - to be configured - board.
   The InitAEDSP16() routine, configure the board following our
   wishes, that are in the hw_config structure.

   You can change the irq/dma/mirq settings WITHOUT THE NEED to open
   your computer and massage the jumpers (there are no irq/dma/mirq
   jumpers to be configured anyway, only I/O port ones have to be
   configured with jumpers)

   For some ununderstandable reason, the card default of irq 7, dma 1,
   don't work for me. Seems to be an IRQ or DMA conflict. Under heavy
   HDD work, the kernel start to erupt out a lot of messages like:

   'Sound: DMA timed out - IRQ/DRQ config error?'

   For what I can say, I have NOT any conflict at irq 7 (under linux I'm
   using the lp polling driver), and dma line 1 is unused as stated by
   /proc/dma. I can suppose this is a bug of AEDSP16. I know my hardware so
   I'm pretty sure I have not any conflict, but may be I'm wrong. Who knows!
   Anyway a setting of irq 10, dma 3 works really fine.

   NOTE: if someone can use AEDSP16 with irq 7, dma 1, please let me know
   the emulation mode, all the installed hardware and the hardware
   configuration (irq and dma settings of all the hardware).

   This init module should work with SBPRO+MSS, when one of the two is
   the AEDSP16 emulation and the other the real card. (see [1])
   For example:

   AEDSP16 (0x220) in SBPRO emu (0x220) + real MSS + other
   AEDSP16 (0x220) in MSS emu + real SBPRO (0x240) + other

   MPU401 should work. (see [1])

   [1] Not tested by me for lack of hardware.

   TODO, WISHES AND TECH

   May be there's lot of redundant delays, but for now I want to leave it
   this way.

   Should be interesting eventually write down a new ioctl for the
   aedsp16, to let the suser() change the irq/dma/mirq on the fly.
   The thing is not trivial.
   In the real world, there's no need to have such an ioctl because
   when we configure the kernel for compile, we can choose the config
   parameters. If we change our mind, we can easily re-config the kernel
   and re-compile.
   Why let the suser() change the config parameters on the fly ?
   If anyone have a reasonable answer to this question, I will write down
   the code to do it.

   More integration with voxware, using voxware low level routines to
   read-write dsp is not possible because you may want to have MSS
   support and in that case we can not rely on the functions included
   in sb_dsp.c to control 0x2yy I/O ports. I will continue to use my
   own I/O functions.

   - About I/O ports allocation -

   The request_region should be done at device probe in every sound card
   module. This module is not the best site for requesting regions.
   When the request_region code will be added to the main modules such as
   sb, adlib, gus, ad1848, etc, the requesting code in this module should
   go away.

   I think the request regions should be done this way:

   if (check_region(...))
   return ERR; // I/O region alredy reserved
   device_probe(...);
   device_attach(...);
   request_region(...); // reserve only when we are sure all is okay

   Request the 2x0h region in any case if we are using this card.

   NOTE: the "(sbpro)" string with which we are requesting the aedsp16 region
   (see code) does not mean necessarly that we are emulating sbpro.
   It mean that the region is the sbpro I/O ports region. We use this
   region to access the control registers of the card, and if emulating
   sbpro, I/O sbpro registers too. If we are emulating MSS, the sbpro
   registers are not used, in no way, to emulate an sbpro: they are
   used only for configuration pourposes.

   Someone pointed out that should be possible use both the SBPRO and MSS
   modes because the sound card have all the two chipsets, supposing that
   the card is really two cards. I have tried something to have the two
   modes work together, but, for some reason unknown to me, without success.

   I think all the soft-config only cards have an init sequence similar to
   this. If you have a card that is not an aedsp16, you can try to start
   with this module changing it (mainly in the CMD? I think) to fit your
   needs.

   Started Fri Mar 17 16:13:18 MET 1995

   v0.1 (ALPHA, was an user-level program called AudioExcelDSP16.c)
   - Initial code.
   v0.2 (ALPHA)
   - Cleanups.
   - Integrated with Linux voxware v 2.90-2 kernel sound driver.
   - SoundBlaster Pro mode configuration.
   - Microsoft Sound System mode configuration.
   - MPU-401 mode configuration.
   v0.3 (ALPHA)
   - Cleanups.
   - Rearranged the code to let InitAEDSP16 be more general.
   - Erased the REALLY_SLOW_IO. We don't need it. Erased the linux/io.h
   inclusion too. We rely on os.h
   - Used the INB and OUTB #defined in os.h instead of inb and outb.
   - Corrected the code for GetCardName (DSP Copyright) to get a variable
   len string (we are not sure about the len of Copyright string).
   This works with any SB and compatible.
   - Added the code to request_region at device init (should go in
   the main body of voxware).
   v0.4 (BETA)
   - Better configure.c patch for aedsp16 configuration (better
   logic of inclusion of AEDSP16 support)
   - Modified the conditional compilation to better support more than
   one sound card of the emulated type (read the NOTES above)
   - Moved the sb init routine from the attach to the very first
   probe in sb_card.c
   - Rearrangemens and cleanups
   - Wiped out some unnecessary code and variables: this is kernel
   code so it is better save some TEXT and DATA
   - Fixed the request_region code. We must allocate the aedsp16 (sbpro)
   I/O ports in any case because they are used to access the DSP
   configuration registers and we can not allow anyone to get them.
   v0.5
   - cleanups on comments
   - prep for diffs against v3.0-proto-950402

 */

/*
 * Include the main voxware header file. It include all the os/voxware/etc
 * headers needed by this source.
 */
#include <i386/isa/sound/sound_config.h>
/*
 * all but ioport.h :)
 */
#include <linux/ioport.h>

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_AEDSP16)

#define VERSION "0.5"		/* Version of Audio Excel DSP 16 driver */

#undef AEDSP16_DEBUG		/* Define this to enable debug code     */
/* Actually no debug code is activated  */

/*
 * Hardware related defaults
 */
#define IRQ  7			/* 5 7(default) 9 10 11                 */
#define MIRQ 0			/* 5 7 9 10 0(default), 0 means disable */
#define DMA  1			/* 0 1(default) 3                       */

/*
 * Commands of AEDSP16's DSP (SBPRO+special).
 * For now they are CMDn, in the future they may change.
 */
#define CMD1 0xe3		/* Get DSP Copyright                    */
#define CMD2 0xe1		/* Get DSP Version                      */
#define CMD3 0x88		/*                                      */
#define CMD4 0x5c		/*                                      */
#define CMD5 0x50		/* Set M&I&DRQ mask (the real config)   */
#define CMD6 0x8c		/* Enable Microsoft Sound System mode   */

/*
 * Offsets of AEDSP16 DSP I/O ports. The offest is added to portbase
 * to have the actual I/O port.
 * Register permissions are:
 * (wo) == Write Only
 * (ro) == Read  Only
 * (w-) == Write
 * (r-) == Read
 */
#define DSP_RESET    0x06	/* offset of DSP RESET             (wo) */
#define DSP_READ     0x0a	/* offset of DSP READ              (ro) */
#define DSP_WRITE    0x0c	/* offset of DSP WRITE             (w-) */
#define DSP_COMMAND  0x0c	/* offset of DSP COMMAND           (w-) */
#define DSP_STATUS   0x0c	/* offset of DSP STATUS            (r-) */
#define DSP_DATAVAIL 0x0e	/* offset of DSP DATA AVAILABLE    (ro) */


#define RETRY           10	/* Various retry values on I/O opera-   */
#define STATUSRETRY   1000	/* tions. Sometimes we have to          */
#define HARDRETRY   500000	/* wait for previous cmd to complete    */

/*
 * Size of character arrays that store name and version of sound card
 */
#define CARDNAMELEN 15		/* Size of the card's name in chars     */
#define CARDVERLEN  2		/* Size of the card's version in chars  */

/*
 * Bit mapped flags for calling InitAEDSP16(), and saving the current
 * emulation mode.
 */
#define INIT_NONE   (0   )
#define INIT_SBPRO  (1<<0)
#define INIT_MSS    (1<<1)
#define INIT_MPU401 (1<<2)
#define RESET_DSP16 (1<<3)

/* Base HW Port for Audio Card          */
static int      portbase = AEDSP16_BASE;
static int      irq = IRQ;	/* irq for DSP I/O                      */
static int      mirq = MIRQ;	/* irq for MPU-401 I/O                  */
static int      dma = DMA;	/* dma for DSP I/O                      */

/* Init status of the card              */
static int      ae_init = INIT_NONE;	/* (bitmapped variable)                 */
static int      oredparams = 0;	/* Will contain or'ed values of params  */
static int      gc = 0;		/* generic counter (utility counter)    */
struct orVals
  {				/* Contain the values to be or'ed       */
    int             val;	/* irq|mirq|dma                         */
    int             or;		/* oredparams |= TheStruct.or           */
  };

/*
 * Magic values that the DSP will eat when configuring irq/mirq/dma
 */
/* DSP IRQ conversion array             */
static struct orVals orIRQ[] =
{
  {0x05, 0x28},
  {0x07, 0x08},
  {0x09, 0x10},
  {0x0a, 0x18},
  {0x0b, 0x20},
  {0x00, 0x00}
};

/* MPU-401 IRQ conversion array         */
static struct orVals orMIRQ[] =
{
  {0x05, 0x04},
  {0x07, 0x44},
  {0x09, 0x84},
  {0x0a, 0xc4},
  {0x00, 0x00}
};

/* DMA Channels conversion array        */
static struct orVals orDMA[] =
{
  {0x00, 0x01},
  {0x01, 0x02},
  {0x03, 0x03},
  {0x00, 0x00}
};

/*
 * Buffers to store audio card informations
 */
static char     AudioExcelName[CARDNAMELEN + 1];
static char     AudioExcelVersion[CARDVERLEN + 1];

static void
tenmillisec (void)
{

  for (gc = 0; gc < 1000; gc++)
    tenmicrosec ();
}

static int
WaitForDataAvail (int port)
{
  int             loop = STATUSRETRY;
  unsigned char   ret = 0;

  do
    {
      ret = INB (port + DSP_DATAVAIL);
      /*
         * Wait for data available (bit 7 of ret == 1)
       */
    }
  while (!(ret & 0x80) && loop--);

  if (ret & 0x80)
    return 0;

  return -1;
}

static int
ReadData (int port)
{
  if (WaitForDataAvail (port))
    return -1;
  return INB (port + DSP_READ);
}

static int
CheckDSPOkay (int port)
{
  return ((ReadData (port) == 0xaa) ? 0 : -1);
}

static int
ResetBoard (int port)
{
  /*
     * Reset DSP
   */
  OUTB (1, (port + DSP_RESET));
  tenmicrosec ();
  OUTB (0, (port + DSP_RESET));
  tenmicrosec ();
  tenmicrosec ();
  return CheckDSPOkay (port);
}

static int
WriteDSPCommand (int port, int cmd)
{
  unsigned char   ret;
  int             loop = HARDRETRY;

  do
    {
      ret = INB (port + DSP_STATUS);
      /*
         * DSP ready to receive data if bit 7 of ret == 0
       */
      if (!(ret & 0x80))
	{
	  OUTB (cmd, port + DSP_COMMAND);
	  return 0;
	}
    }
  while (loop--);

  printk ("[aedsp16] DSP Command (0x%x) timeout.\n", cmd);
  return -1;
}

int
InitMSS (int port)
{

  tenmillisec ();

  if (WriteDSPCommand (port, CMD6))
    {
      printk ("[aedsp16] CMD 0x%x: failed!\n", CMD6);
      return -1;
    }

  tenmillisec ();

  return 0;
}

static int
SetUpBoard (int port)
{
  int             loop = RETRY;

  do
    {
      if (WriteDSPCommand (portbase, CMD3))
	{
	  printk ("[aedsp16] CMD 0x%x: failed!\n", CMD3);
	  return -1;
	}

      tenmillisec ();

    }
  while (WaitForDataAvail (port) && loop--);

#if defined(THIS_SHOULD_GO_AWAY)
  if (CheckDSPOkay (port))
    {
      printk ("[aedsp16]     CheckDSPOkay: failed\n");
      return -1;
    }
#else
  if (ReadData (port) == -1)
    {
      printk ("[aedsp16] ReadData after CMD 0x%x: failed\n", CMD3);
      return -1;
    }
#endif

  if (WriteDSPCommand (portbase, CMD4))
    {
      printk ("[aedsp16] CMD 0x%x: failed!\n", CMD4);
      return -1;
    }

  if (WriteDSPCommand (portbase, CMD5))
    {
      printk ("[aedsp16] CMD 0x%x: failed!\n", CMD5);
      return -1;
    }

  if (WriteDSPCommand (portbase, oredparams))
    {
      printk ("[aedsp16] Initialization of (M)IRQ and DMA: failed!\n");
      return -1;
    }
  return 0;
}

static int
GetCardVersion (int port)
{
  int             len = 0;
  int             ret;
  int             ver[3];

  do
    {
      if ((ret = ReadData (port)) == -1)
	return -1;
      /*
         * We alredy know how many int are stored (2), so we know when the
         * string is finished.
       */
      ver[len++] = ret;
    }
  while (len < CARDVERLEN);
  sprintf (AudioExcelVersion, "%d.%d", ver[0], ver[1]);
  return 0;
}

static int
GetCardName (int port)
{
  int             len = 0;
  int             ret;

  do
    {
      if ((ret = ReadData (port)) == -1)
	/*
	   * If no more data availabe, return to the caller, no error if len>0.
	   * We have no other way to know when the string is finished.
	 */
	return (len ? 0 : -1);

      AudioExcelName[len++] = ret;

    }
  while (len < CARDNAMELEN);
  return 0;
}

static void
InitializeHardParams (void)
{

  memset (AudioExcelName, 0, CARDNAMELEN + 1);
  memset (AudioExcelVersion, 0, CARDVERLEN + 1);

  for (gc = 0; orIRQ[gc].or; gc++)
    if (orIRQ[gc].val == irq)
      oredparams |= orIRQ[gc].or;

  for (gc = 0; orMIRQ[gc].or; gc++)
    if (orMIRQ[gc].or == mirq)
      oredparams |= orMIRQ[gc].or;

  for (gc = 0; orDMA[gc].or; gc++)
    if (orDMA[gc].val == dma)
      oredparams |= orDMA[gc].or;
}

static int
InitAEDSP16 (int which)
{
  static char    *InitName = NULL;

  InitializeHardParams ();

  if (ResetBoard (portbase))
    {
      printk ("[aedsp16] ResetBoard: failed!\n");
      return -1;
    }

#if defined(THIS_SHOULD_GO_AWAY)
  if (CheckDSPOkay (portbase))
    {
      printk ("[aedsp16] CheckDSPOkay: failed!\n");
      return -1;
    }
#endif

  if (WriteDSPCommand (portbase, CMD1))
    {
      printk ("[aedsp16] CMD 0x%x: failed!\n", CMD1);
      return -1;
    }

  if (GetCardName (portbase))
    {
      printk ("[aedsp16] GetCardName: failed!\n");
      return -1;
    }

  /*
     * My AEDSP16 card return SC-6000 in AudioExcelName, so
     * if we have something different, we have to be warned.
   */
  if (strcmp ("SC-6000", AudioExcelName))
    printk ("[aedsp16] Warning: non SC-6000 audio card!\n");

  if (WriteDSPCommand (portbase, CMD2))
    {
      printk ("[aedsp16] CMD 0x%x: failed!\n", CMD2);
      return -1;
    }

  if (GetCardVersion (portbase))
    {
      printk ("[aedsp16] GetCardVersion: failed!\n");
      return -1;
    }

  if (SetUpBoard (portbase))
    {
      printk ("[aedsp16] SetUpBoard: failed!\n");
      return -1;
    }

  if (which == INIT_MSS)
    {
      if (InitMSS (portbase))
	{
	  printk ("[aedsp16] Can't initialize Microsoft Sound System mode.\n");
	  return -1;
	}
    }

  /*
     * If we are resetting, do not print any message because we may be
     * in playing and we do not want lost too much time.
   */
  if (!(which & RESET_DSP16))
    {
      if (which & INIT_MPU401)
	InitName = "MPU401";
      else if (which & INIT_SBPRO)
	InitName = "SBPro";
      else if (which & INIT_MSS)
	InitName = "MSS";
      else
	InitName = "None";

      printk ("Audio Excel DSP 16 init v%s (%s %s) [%s]\n",
	      VERSION, AudioExcelName,
	      AudioExcelVersion, InitName);
    }

  tenmillisec ();

  return 0;
}

#if defined(AEDSP16_SBPRO)

int
InitAEDSP16_SBPRO (struct address_info *hw_config)
{
  /*
     * If the card is alredy init'ed MSS, we can not init it to SBPRO too
     * because the board can not emulate simultaneously MSS and SBPRO.
   */
  if (ae_init & INIT_MSS)
    return -1;
  if (ae_init & INIT_SBPRO)
    return 0;

  /*
     * For now we will leave this
     * code included only when INCLUDE_AEDSP16 is configured in, but it should
     * be better include it every time.
   */
  if (!(ae_init & INIT_MPU401))
    {
      if (check_region (hw_config->io_base, 0x0f))
	{
	  printk ("AEDSP16/SBPRO I/O port region is alredy in use.\n");
	  return -1;
	}
    }

  /*
     * Set up the internal hardware parameters, to let the driver reach
     * the Sound Card.
   */
  portbase = hw_config->io_base;
  irq = hw_config->irq;
  dma = hw_config->dma;
  if (InitAEDSP16 (INIT_SBPRO))
    return -1;

  if (!(ae_init & INIT_MPU401))
    request_region (hw_config->io_base, 0x0f, "aedsp16 (sbpro)");

  ae_init |= INIT_SBPRO;
  return 0;
}

#endif /* AEDSP16_SBPRO */

#if defined(AEDSP16_MSS)

int
InitAEDSP16_MSS (struct address_info *hw_config)
{
  /*
     * If the card is alredy init'ed SBPRO, we can not init it to MSS too
     * because the board can not emulate simultaneously MSS and SBPRO.
   */
  if (ae_init & INIT_SBPRO)
    return -1;
  if (ae_init & INIT_MSS)
    return 0;

  /*
     * For now we will leave this
     * code included only when INCLUDE_AEDSP16 is configured in, but it should
     * be better include it every time.
   */
  if (check_region (hw_config->io_base, 0x08))
    {
      printk ("MSS I/O port region is alredy in use.\n");
      return -1;
    }

  /*
     * We must allocate the AEDSP16 region too because these are the I/O ports
     * to access card's control registers.
   */
  if (!(ae_init & INIT_MPU401))
    {
      if (check_region (AEDSP16_BASE, 0x0f))
	{
	  printk ("AEDSP16 I/O port region is alredy in use.\n");
	  return -1;
	}
    }


  /*
     * If we are configuring the card for MSS, the portbase for card configuration
     * is the default one (0x220 unless you have changed the factory default
     * with board switches), so no need to modify the portbase variable.
     * The default is AEDSP16_BASE, that is the right value.
   */
  irq = hw_config->irq;
  dma = hw_config->dma;
  if (InitAEDSP16 (INIT_MSS))
    return -1;

  request_region (hw_config->io_base, 0x08, "aedsp16 (mss)");

  if (!(ae_init & INIT_MPU401))
    request_region (AEDSP16_BASE, 0x0f, "aedsp16 (sbpro)");

  ae_init |= INIT_MSS;
  return 0;
}

#endif /* AEDSP16_MSS */

#if defined(AEDSP16_MPU401)

int
InitAEDSP16_MPU401 (struct address_info *hw_config)
{
  if (ae_init & INIT_MPU401)
    return 0;

  /*
     * For now we will leave this
     * code included only when INCLUDE_AEDSP16 is configured in, but it should
     * be better include it every time.
   */
  if (check_region (hw_config->io_base, 0x02))
    {
      printk ("SB I/O port region is alredy in use.\n");
      return -1;
    }

  /*
     * We must allocate the AEDSP16 region too because these are the I/O ports
     * to access card's control registers.
   */
  if (!(ae_init & (INIT_MSS | INIT_SBPRO)))
    {
      if (check_region (AEDSP16_BASE, 0x0f))
	{
	  printk ("AEDSP16 I/O port region is alredy in use.\n");
	  return -1;
	}
    }

  /*
     * If mpu401, the irq and dma are not important, do not touch it
     * because we may use the default if sbpro is not yet configured,
     * we may use the sbpro ones if configured, and nothing wrong
     * should happen.
     *
     * The mirq default is 0, but once set it to non-0 value, we should
     * not touch it anymore (unless I write an ioctl to do it, of course).
   */
  mirq = hw_config->irq;
  if (InitAEDSP16 (INIT_MPU401))
    return -1;

  request_region (hw_config->io_base, 0x02, "aedsp16 (mpu401)");

  if (!(ae_init & (INIT_MSS | INIT_SBPRO)))
    request_region (AEDSP16_BASE, 0x0f, "aedsp16 (sbpro)");

  ae_init |= INIT_MPU401;
  return 0;
}

#endif /* AEDSP16_MPU401 */

#if 0				/* Leave it out for now. We are not using this portion of code. */

/*
 * Entry point for a reset function.
 * May be I will write the infamous ioctl :)
 */
int
ResetAEDSP16 (void)
{
#if defined(AEDSP16_DEBUG)
  printk ("[aedsp16] ResetAEDSP16 called.\n");
#endif
  return InitAEDSP16 (RESET_DSP16);
}

#endif /* 0 */

#endif /* !EXCLUDE_AEDSP16 */
