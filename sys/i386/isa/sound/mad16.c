/*
 * sound/mad16.c
 * 
 * Initialization code for OPTi MAD16 compatible audio chips. Including
 * 
 * OPTi 82C928     MAD16           (replaced by C929) OAK OTI-601D    Mozart
 * OPTi 82C929     MAD16 Pro
 * 
 * These audio interface chips don't prduce sound themselves. They just connect
 * some other components (OPL-[234] and a WSS compatible codec) to the PC bus
 * and perform I/O, DMA and IRQ address decoding. There is also a UART for
 * the MPU-401 mode (not 82C928/Mozart). The Mozart chip appears to be
 * compatible with the 82C928 (can anybody confirm this?).
 * 
 * NOTE! If you want to set CD-ROM address and/or joystick enable, define
 * MAD16_CONF in local.h as combination of the following bits:
 * 
 * 0x01    - joystick disabled
 * 
 * CD-ROM type selection (select just one): 0x00    - none 0x02    - Sony 31A
 * 0x04    - Mitsumi 0x06    - Panasonic (type "LaserMate", not
 * "SoundBlaster") 0x08    - Secondary IDE (address 0x170) 0x0a    - Primary
 * IDE (address 0x1F0)
 * 
 * For example Mitsumi with joystick disabled = 0x04|0x01 = 0x05 For example
 * LaserMate (for use with sbpcd) plus joystick = 0x06
 * 
 * MAD16_CDSEL: This defaults to CD I/O 0x340, no IRQ and DMA3 (DMA5 with
 * Mitsumi or IDE). If you like to change these, define MAD16_CDSEL with the
 * following bits:
 * 
 * CD-ROM port: 0x00=340, 0x40=330, 0x80=360 or 0xc0=320 OPL4 select: 0x20=OPL4,
 * 0x00=OPL3 CD-ROM irq: 0x00=disabled, 0x04=IRQ5, 0x08=IRQ7, 0x0a=IRQ3,
 * 0x10=IRQ9, 0x14=IRQ10 and 0x18=IRQ11.
 * 
 * CD-ROM DMA (Sony or Panasonic): 0x00=DMA3, 0x01=DMA2, 0x02=DMA1 or
 * 0x03=disabled or CD-ROM DMA (Mitsumi or IDE):    0x00=DMA5, 0x01=DMA6,
 * 0x02=DMA7 or 0x03=disabled
 * 
 * For use with sbpcd, address 0x340, set MAD16_CDSEL to 0x03 or 0x23.
 * 
 * Copyright by Hannu Savolainen 1995
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

#if defined(CONFIG_MAD16)

static int      already_initialized = 0;

#define C928	1
#define MOZART	2
#define C929	3

/*
 * Registers
 * 
 * The MAD16 occupies I/O ports 0xf8d to 0xf93 (fixed locations). All ports are
 * inactive by default. They can be activated by writing 0xE2 or 0xE3 to the
 * password register. The password is valid only until the next I/O read or
 * write.
 */

#define MC1_PORT	0xf8d	/* SB address, CDROM interface type, joystick */
#define MC2_PORT	0xf8e	/* CDROM address, IRQ, DMA, plus OPL4 bit */
#define MC3_PORT	0xf8f
#define PASSWD_REG	0xf8f
#define MC4_PORT	0xf90
#define MC5_PORT	0xf91
#define MC6_PORT	0xf92
#define MC7_PORT	0xf93

static int      board_type = C928;

static sound_os_info *mad16_osp;

#ifndef DDB
#define DDB(x)
#endif

static unsigned char
mad_read(int port)
{
	unsigned long   flags;
	unsigned char   tmp;

	flags = splhigh();

	switch (board_type) {	/* Output password */
	case C928:
	case MOZART:
		outb(PASSWD_REG, 0xE2);
		break;

	case C929:
		outb(PASSWD_REG, 0xE3);
		break;
	}

	tmp = inb(port);
	splx(flags);

	return tmp;
}

static void
mad_write(int port, int value)
{
	unsigned long   flags;

	flags = splhigh();

	switch (board_type) {	/* Output password */
	case C928:
	case MOZART:
		outb(PASSWD_REG, 0xE2);
		break;

	case C929:
		outb(PASSWD_REG, 0xE3);
		break;
	}

	outb(port, (unsigned char) (value & 0xff));
	splx(flags);
}

static int
detect_mad16(void)
{
	unsigned char   tmp, tmp2;

	/*
	 * Check that reading a register doesn't return bus float (0xff) when
	 * the card is accessed using password. This may fail in case the
	 * card is in low power mode. Normally at least the power saving mode
	 * bit should be 0.
	 */
	if ((tmp = mad_read(MC1_PORT)) == 0xff) {
		DDB(printf("MC1_PORT returned 0xff\n"));
		return 0;
	}
	/*
	 * Now check that the gate is closed on first I/O after writing the
	 * password. (This is how a MAD16 compatible card works).
	 */

	if ((tmp2 = inb(MC1_PORT)) == tmp) {	/* It didn't close */
		DDB(printf("MC1_PORT didn't close after read (0x%02x)\n", tmp2));
		return 0;
	}
	mad_write(MC1_PORT, tmp ^ 0x80);	/* Togge a bit */

	if ((tmp2 = mad_read(MC1_PORT)) != (tmp ^ 0x80)) {	/* Compare the bit */
		mad_write(MC1_PORT, tmp);	/* Restore */
		DDB(printf("Bit revert test failed (0x%02x, 0x%02x)\n", tmp, tmp2));
		return 0;
	}
	mad_write(MC1_PORT, tmp);	/* Restore */
	return 1;		/* Bingo */

}

int
probe_mad16(struct address_info * hw_config)
{
	int             i;
	static int      valid_ports[] =
	{0x530, 0xe80, 0xf40, 0x604};
	unsigned char   tmp;
	unsigned char   cs4231_mode = 0;

	int             ad_flags = 0;

	if (already_initialized)
		return 0;

	mad16_osp = hw_config->osp;
	/*
	 * Check that all ports return 0xff (bus float) when no password is
	 * written to the password register.
	 */

	DDB(printf("--- Detecting MAD16 / Mozart ---\n"));


	/*
	 * Then try to detect with the old password
	 */
	board_type = C928;

	DDB(printf("Detect using password = 0xE2\n"));

	if (!detect_mad16()) {	/* No luck. Try different model */
		board_type = C929;

		DDB(printf("Detect using password = 0xE3\n"));

		if (!detect_mad16())
			return 0;

		DDB(printf("mad16.c: 82C929 detected\n"));
	} else {
		unsigned char   model;

		if (((model = mad_read(MC3_PORT)) & 0x03) == 0x03) {
			DDB(printf("mad16.c: Mozart detected\n"));
			board_type = MOZART;
		} else {
			DDB(printf("mad16.c: 82C928 detected???\n"));
			board_type = C928;
		}
	}

	for (i = 0xf8d; i <= 0xf93; i++)
		DDB(printf("port %03x = %03x\n", i, mad_read(i)));

	/*
	 * Set the WSS address
	 */

	tmp = 0x80;		/* Enable WSS, Disable SB */

	for (i = 0; i < 5; i++) {
		if (i > 3) {	/* Not a valid port */
			printf("MAD16/Mozart: Bad WSS base address 0x%x\n", hw_config->io_base);
			return 0;
		}
		if (valid_ports[i] == hw_config->io_base) {
			tmp |= i << 4;	/* WSS port select bits */
			break;
		}
	}

	/*
	 * Set optional CD-ROM and joystick settings.
	 */

#ifdef MAD16_CONF
	tmp |= ((MAD16_CONF) & 0x0f);	/* CD-ROM and joystick bits */
#endif
	mad_write(MC1_PORT, tmp);

#if defined(MAD16_CONF) && defined(MAD16_CDSEL)
	tmp = MAD16_CDSEL;
#else
	tmp = 0x03;
#endif

#ifdef MAD16_OPL4
	tmp |= 0x20;		/* Enable OPL4 access */
#endif

	mad_write(MC2_PORT, tmp);
	mad_write(MC3_PORT, 0xf0);	/* Disable SB */

	if (!ad1848_detect(hw_config->io_base + 4, &ad_flags, mad16_osp))
		return 0;

	if (ad_flags & (AD_F_CS4231 | AD_F_CS4248))
		cs4231_mode = 0x02;	/* CS4248/CS4231 sync delay switch */

	if (board_type == C929) {
		mad_write(MC4_PORT, 0xa2);
		mad_write(MC5_PORT, 0xA5 | cs4231_mode);
		mad_write(MC6_PORT, 0x03);	/* Disable MPU401 */
	} else {
		mad_write(MC4_PORT, 0x02);
		mad_write(MC5_PORT, 0x30 | cs4231_mode);
	}

	for (i = 0xf8d; i <= 0xf93; i++)
		DDB(printf("port %03x after init = %03x\n", i, mad_read(i)));

	/*
	 * Verify the WSS parameters
	 */

	if (0) {
		printf("MSS: I/O port conflict\n");
		return 0;
	}
	/*
	 * Check if the IO port returns valid signature. The original MS
	 * Sound system returns 0x04 while some cards (AudioTriX Pro for
	 * example) return 0x00.
	 */

	if ((inb(hw_config->io_base + 3) & 0x3f) != 0x04 &&
	    (inb(hw_config->io_base + 3) & 0x3f) != 0x00) {
		DDB(printf("No MSS signature detected on port 0x%x (0x%x)\n",
			   hw_config->io_base, inb(hw_config->io_base + 3)));
		return 0;
	}
	if (hw_config->irq > 11) {
		printf("MSS: Bad IRQ %d\n", hw_config->irq);
		return 0;
	}
	if (hw_config->dma != 0 && hw_config->dma != 1 && hw_config->dma != 3) {
		printf("MSS: Bad DMA %d\n", hw_config->dma);
		return 0;
	}
	/*
	 * Check that DMA0 is not in use with a 8 bit board.
	 */

	if (hw_config->dma == 0 && inb(hw_config->io_base + 3) & 0x80) {
		printf("MSS: Can't use DMA0 with a 8 bit card/slot\n");
		return 0;
	}
	if (hw_config->irq > 7 && hw_config->irq != 9 && inb(hw_config->io_base + 3) & 0x80) {
		printf("MSS: Can't use IRQ%d with a 8 bit card/slot\n", hw_config->irq);
		return 0;
	}
	return 1;
}

void
attach_mad16(struct address_info * hw_config)
{

	static char     interrupt_bits[12] =
	{
		-1, -1, -1, -1, -1, -1, -1, 0x08, -1, 0x10, 0x18, 0x20
	};
	char            bits;

	static char     dma_bits[4] =
	{
		1, 2, 0, 3
	};

	int             config_port = hw_config->io_base + 0, version_port = hw_config->io_base + 3;
	int             ad_flags = 0, dma = hw_config->dma, dma2 = hw_config->dma2;
	unsigned char   dma2_bit = 0;

	already_initialized = 1;

	if (!ad1848_detect(hw_config->io_base + 4, &ad_flags, mad16_osp))
		return;

	/*
	 * Set the IRQ and DMA addresses.
	 */

	bits = interrupt_bits[hw_config->irq];
	if (bits == -1)
		return;

	outb(config_port, bits | 0x40);
	if ((inb(version_port) & 0x40) == 0)
		printf("[IRQ Conflict?]");

	/*
	 * Handle the capture DMA channel
	 */

	if (ad_flags & AD_F_CS4231 && dma2 != -1 && dma2 != dma) {
		if ((dma == 0 && dma2 == 1) ||
		    (dma == 1 && dma2 == 0) ||
		    (dma == 3 && dma2 == 0)) {
			dma2_bit = 0x04;	/* Enable capture DMA */
		} else {
			printf("MAD16: Invalid capture DMA\n");
			dma2 = dma;
		}
	} else
		dma2 = dma;

	outb(config_port, bits | dma_bits[dma] | dma2_bit);	/* Write IRQ+DMA setup */

	ad1848_init("MAD16 WSS", hw_config->io_base + 4,
		    hw_config->irq,
		    dma,
		    dma2, 0,
		    hw_config->osp);
}

void
attach_mad16_mpu(struct address_info * hw_config)
{
	if (board_type < C929) {/* Early chip. No MPU support. Just SB MIDI */
#ifdef CONFIG_MIDI

		if (mad_read(MC1_PORT) & 0x20)
			hw_config->io_base = 0x240;
		else
			hw_config->io_base = 0x220;

		return mad16_sb_dsp_init(hw_config);
#else
		return 0;
#endif
	}
#if (defined(CONFIG_MPU401) || defined(CONFIG_MPU_EMU)) && defined(CONFIG_MIDI)
	if (!already_initialized)
		return;

	attach_mpu401(hw_config);
#endif
}

int
probe_mad16_mpu(struct address_info * hw_config)
{
#if (defined(CONFIG_MPU401) || defined(CONFIG_MPU_EMU)) && defined(CONFIG_MIDI)
	static int      mpu_attached = 0;
	static int      valid_ports[] =
	{0x330, 0x320, 0x310, 0x300};
	static short    valid_irqs[] =
	{9, 10, 5, 7};
	unsigned char   tmp;

	int             i;	/* A variable with secret power */

	if (!already_initialized)	/* The MSS port must be initialized
					 * first */
		return 0;

	if (mpu_attached)	/* Don't let them call this twice */
		return 0;
	mpu_attached = 1;

	if (board_type < C929) {/* Early chip. No MPU support. Just SB MIDI */

#ifdef CONFIG_MIDI
		unsigned char   tmp;

		tmp = mad_read(MC3_PORT);

		/*
		 * MAD16 SB base is defined by the WSS base. It cannot be
		 * changed alone. Ignore configured I/O base. Use the active
		 * setting.
		 */

		if (mad_read(MC1_PORT) & 0x20)
			hw_config->io_base = 0x240;
		else
			hw_config->io_base = 0x220;

		switch (hw_config->irq) {
		case 5:
			tmp = (tmp & 0x3f) | 0x80;
			break;
		case 7:
			tmp = (tmp & 0x3f);
			break;
		case 11:
			tmp = (tmp & 0x3f) | 0x40;
			break;
		default:
			printf("mad16/Mozart: Invalid MIDI IRQ\n");
			return 0;
		}

		mad_write(MC3_PORT, tmp | 0x04);
		return mad16_sb_dsp_detect(hw_config);
#else
		return 0;
#endif
	}
	tmp = 0x83;		/* MPU-401 enable */

	/*
	 * Set the MPU base bits
	 */

	for (i = 0; i < 5; i++) {
		if (i > 3) {	/* Out of array bounds */
			printf("MAD16 / Mozart: Invalid MIDI port 0x%x\n", hw_config->io_base);
			return 0;
		}
		if (valid_ports[i] == hw_config->io_base) {
			tmp |= i << 5;
			break;
		}
	}

	/*
	 * Set the MPU IRQ bits
	 */

	for (i = 0; i < 5; i++) {
		if (i > 3) {	/* Out of array bounds */
			printf("MAD16 / Mozart: Invalid MIDI IRQ %d\n", hw_config->irq);
			return 0;
		}
		if (valid_irqs[i] == hw_config->irq) {
			tmp |= i << 3;
			break;
		}
	}
	mad_write(MC6_PORT, tmp);	/* Write MPU401 config */

	return probe_mpu401(hw_config);
#else
	return 0;
#endif
}

/* That's all folks */
#endif
