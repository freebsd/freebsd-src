/*
 * sound/clones.c
 * 
 * init code for enabling clone cards to work in sb/mss emulation.
 *
 * Note -- this code is currently unused!
 *
 * Copyright by Luigi Rizzo - 1997
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met: 1. Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the following
 * disclaimer. 2.  Redistributions in binary form must reproduce the
 * above copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * This file has been written using information from various sources
 * in the Voxware 3.5 distribution.
 */

#include <i386/isa/snd/sound.h>
#if NPCM > 0

/*
 * Known clones card include:
 *
 * Trix (emulating MSS)
 * MAD16 (emulating MSS)
 * OPTi930 -- same as the OPTi931, but no PnP ?
 */


#ifdef JAZZ16

/*
 * Initialization of a Media Vision ProSonic 16 Soundcard. The function
 * initializes a ProSonic 16 like PROS.EXE does for DOS. It sets the base
 * address, the DMA-channels, interrupts and enables the joystickport.
 * 
 * Also used by Jazz 16 (same card, different name)
 * 
 * written 1994 by Rainer Vranken E-Mail:
 * rvranken@polaris.informatik.uni-essen.de
 */

#ifdef SM_WAVE
/*
 * Logitech Soundman Wave detection and initialization by Hannu Savolainen.
 * 
 * There is a microcontroller (8031) in the SM Wave card for MIDI emulation.
 * it's located at address MPU_BASE+4.  MPU_BASE+7 is a SM Wave specific
 * control register for MC reset, SCSI, OPL4 and DSP (future expansion)
 * address decoding. Otherwise the SM Wave is just a ordinary MV Jazz16 based
 * soundcard.
 */

static void
smw_putmem(int base, int addr, u_char val)
{
    u_long   s;

    s = spltty();

    outb(base + 1, addr & 0xff);	/* Low address bits */
    outb(base + 2, addr >> 8);	/* High address bits */
    outb(base, val);	/* Data */

    splx(s);
}

static u_char
smw_getmem(int base, int addr)
{
    u_long   s;
    u_char   val;

    s = spltty();

    outb(base + 1, addr & 0xff);	/* Low address bits */
    outb(base + 2, addr >> 8);	/* High address bits */
    val = inb(base);	/* Data */

    splx(s);
    return val;
}

#ifdef SMW_MIDI0001_INCLUDED
#include </sys/i386/isa/snd/smw-midi0001.h>
#else
u_char  *smw_ucode = NULL;
int             smw_ucodeLen = 0;
#endif	/* SWM_MIDI0001_INCLUDED */

static int
initialize_smw(int mpu_base)
{

    int      i, mp_base = mpu_base + 4;	/* Microcontroller base */
    u_char   control;

    /*
     * Reset the microcontroller so that the RAM can be accessed
     */

    control = inb(mpu_base + 7);
    outb(mpu_base + 7, control | 3);	/* Set last two bits to 1 (?) */
    outb(mpu_base + 7, (control & 0xfe) | 2);	/* xxxxxxx0 resets the mc */
    DELAY(3000); /* Wait at least 1ms */
	
    outb(mpu_base + 7, control & 0xfc);	/* xxxxxx00 enables RAM */

    /*
     * Detect microcontroller by probing the 8k RAM area
     */
    smw_putmem(mp_base, 0, 0x00);
    smw_putmem(mp_base, 1, 0xff);
    DELAY(10);

    if (smw_getmem(mp_base, 0) != 0x00 || smw_getmem(mp_base, 1) != 0xff) {
	printf("\nSM Wave: No microcontroller RAM detected (%02x, %02x)\n",
	       smw_getmem(mp_base, 0), smw_getmem(mp_base, 1));
	return 0;	/* No RAM */
    }
    /*
     * There is RAM so assume it's really a SM Wave
     */

    if (smw_ucodeLen > 0) {
	if (smw_ucodeLen != 8192) {
	    printf("\nSM Wave: Invalid microcode (MIDI0001.BIN) length\n");
	    return 1;
	}
	/*
	 * Download microcode
	 */

	for (i = 0; i < 8192; i++)
	    smw_putmem(mp_base, i, smw_ucode[i]);

	/*
	 * Verify microcode
	 */

	for (i = 0; i < 8192; i++)
	    if (smw_getmem(mp_base, i) != smw_ucode[i]) {
		printf("SM Wave: Microcode verification failed\n");
		return 0;
	    }
    }
    control = 0;
#ifdef SMW_SCSI_IRQ
    /*
     * Set the SCSI interrupt (IRQ2/9, IRQ3 or IRQ10). The SCSI interrupt
     * is disabled by default.
     * 
     * Btw the Zilog 5380 SCSI controller is located at MPU base + 0x10.
     */
    {
	static u_char scsi_irq_bits[] =
	    {0, 0, 3, 1, 0, 0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0};

	control |= scsi_irq_bits[SMW_SCSI_IRQ] << 6;
    }
#endif

#ifdef SMW_OPL4_ENABLE
    /*
     * Make the OPL4 chip visible on the PC bus at 0x380.
     * 
     * There is no need to enable this feature since VoxWare doesn't support
     * OPL4 yet. Also there is no RAM in SM Wave so enabling OPL4 is
     * pretty useless.
     */
    control |= 0x10;	/* Uses IRQ12 if bit 0x20 == 0 */
    /* control |= 0x20;      Uncomment this if you want to use IRQ7 */
#endif

    outb(mpu_base + 7, control | 0x03);	/* xxxxxx11 restarts */
    return 1;
}

#endif

/*
 * this is only called during the probe. Variables are found in
 * sb_probed.
 */
static sbdev_info sb_probed ;
static int
initialize_ProSonic16(snddev_info *d)
{
    int             x;
    static u_char int_translat[16] =
	    {0, 0, 2, 3, 0, 1, 0, 4, 0, 2, 5, 0, 0, 0, 0, 6},
	dma_translat[8] =
	    {0, 1, 0, 2, 0, 3, 0, 4};

    struct address_info *mpu_config;

    int             mpu_base, mpu_irq;

    if ((mpu_config = NULL)) {
	mpu_base = mpu_config->io_base;
	mpu_irq = mpu_config->irq;
    } else {
	mpu_base = mpu_irq = 0;
    }

    outb(0x201, 0xAF);	/* ProSonic/Jazz16 wakeup */
    DELAY(15000);	/* wait at least 10 milliseconds */
    outb(0x201, 0x50);
    outb(0x201, (sb_probed.io_base & 0x70) | ((mpu_base & 0x30) >> 4));

    if (sb_reset_dsp(sb_probed.io_base)) {	/* OK. We have at least a SB */

	/* Check the version number of ProSonic (I guess) */

	if (!sb_cmd(sb_probed.io_base, 0xFA))
	    return 1;
	if (sb_get_byte(sb_probed.io_base) != 0x12)
	    return 1;

	if (sb_cmd(sb_probed.io_base, 0xFB) &&	/* set DMA and irq */
	    sb_cmd(sb_probed.io_base,
		(dma_translat[JAZZ_DMA16]<<4)|dma_translat[sb_probed.dma1]) &&
	    sb_cmd(sb_probed.io_base,
		(int_translat[mpu_irq]<<4)|int_translat[sb_probed.irq])) {
	    d->bf_flags |= BD_F_JAZZ16 ;
	    if (mpu_base == 0)
		printf("Jazz16: No MPU401 devices configured "
			"- MIDI port not initialized\n");

#ifdef SM_WAVE
		if (mpu_base != 0)
		    if (initialize_smw(mpu_base))
			d->bf_flags |= BD_F_JAZZ16_2 ;
#endif
	    /* sb_dsp_disable_midi(); */
	}
	return 1;	/* There was at least a SB */
    }
    return 0;		/* No SB or ProSonic16 detected */
}

#endif				/* ifdef JAZZ16  */

#endif /* NPCM */
