/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef __FreeBSD__
#include <dev/sym/sym_glue.h>
#else
#include "sym_glue.h"
#endif

/*
 *  Some poor and bogus sync table that refers to Tekram NVRAM layout.
 */
#if SYM_CONF_NVRAM_SUPPORT
static u_char Tekram_sync[16] =
	{25,31,37,43, 50,62,75,125, 12,15,18,21, 6,7,9,10};
#ifdef	SYM_CONF_DEBUG_NVRAM
static u_char Tekram_boot_delay[7] = {3, 5, 10, 20, 30, 60, 120};
#endif
#endif

/*
 *  Get host setup from NVRAM.
 */
void sym_nvram_setup_host (hcb_p np, struct sym_nvram *nvram)
{
#if SYM_CONF_NVRAM_SUPPORT
	/*
	 *  Get parity checking, host ID, verbose mode 
	 *  and miscellaneous host flags from NVRAM.
	 */
	switch(nvram->type) {
	case SYM_SYMBIOS_NVRAM:
		if (!(nvram->data.Symbios.flags & SYMBIOS_PARITY_ENABLE))
			np->rv_scntl0  &= ~0x0a;
		np->myaddr = nvram->data.Symbios.host_id & 0x0f;
		if (nvram->data.Symbios.flags & SYMBIOS_VERBOSE_MSGS)
			np->verbose += 1;
		if (nvram->data.Symbios.flags1 & SYMBIOS_SCAN_HI_LO)
			np->usrflags |= SYM_SCAN_TARGETS_HILO;
		if (nvram->data.Symbios.flags2 & SYMBIOS_AVOID_BUS_RESET)
			np->usrflags |= SYM_AVOID_BUS_RESET;
		break;
	case SYM_TEKRAM_NVRAM:
		np->myaddr = nvram->data.Tekram.host_id & 0x0f;
		break;
	default:
		break;
	}
#endif
}

/*
 *  Get target setup from NVRAM.
 */
#if SYM_CONF_NVRAM_SUPPORT
static void sym_Symbios_setup_target(hcb_p np,int target, Symbios_nvram *nvram);
static void sym_Tekram_setup_target(hcb_p np,int target, Tekram_nvram *nvram);
#endif

void sym_nvram_setup_target (hcb_p np, int target, struct sym_nvram *nvp)
{
#if SYM_CONF_NVRAM_SUPPORT
	switch(nvp->type) {
	case SYM_SYMBIOS_NVRAM:
		sym_Symbios_setup_target (np, target, &nvp->data.Symbios);
		break;
	case SYM_TEKRAM_NVRAM:
		sym_Tekram_setup_target (np, target, &nvp->data.Tekram);
		break;
	default:
		break;
	}
#endif
}

#if SYM_CONF_NVRAM_SUPPORT
/*
 *  Get target set-up from Symbios format NVRAM.
 */
static void
sym_Symbios_setup_target(hcb_p np, int target, Symbios_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	Symbios_target *tn = &nvram->target[target];

	tp->tinfo.user.period = tn->sync_period ? (tn->sync_period + 3) / 4 : 0;
	tp->tinfo.user.width  = tn->bus_width == 0x10 ? BUS_16_BIT : BUS_8_BIT;
	tp->usrtags =
		(tn->flags & SYMBIOS_QUEUE_TAGS_ENABLED)? SYM_SETUP_MAX_TAG : 0;

	if (!(tn->flags & SYMBIOS_DISCONNECT_ENABLE))
		tp->usrflags &= ~SYM_DISC_ENABLED;
	if (!(tn->flags & SYMBIOS_SCAN_AT_BOOT_TIME))
		tp->usrflags |= SYM_SCAN_BOOT_DISABLED;
	if (!(tn->flags & SYMBIOS_SCAN_LUNS))
		tp->usrflags |= SYM_SCAN_LUNS_DISABLED;
}

/*
 *  Get target set-up from Tekram format NVRAM.
 */
static void
sym_Tekram_setup_target(hcb_p np, int target, Tekram_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	struct Tekram_target *tn = &nvram->target[target];
	int i;

	if (tn->flags & TEKRAM_SYNC_NEGO) {
		i = tn->sync_index & 0xf;
		tp->tinfo.user.period = Tekram_sync[i];
	}

	tp->tinfo.user.width =
		(tn->flags & TEKRAM_WIDE_NEGO) ? BUS_16_BIT : BUS_8_BIT;

	if (tn->flags & TEKRAM_TAGGED_COMMANDS) {
		tp->usrtags = 2 << nvram->max_tags_index;
	}

	if (tn->flags & TEKRAM_DISCONNECT_ENABLE)
		tp->usrflags |= SYM_DISC_ENABLED;
 
	/* If any device does not support parity, we will not use this option */
	if (!(tn->flags & TEKRAM_PARITY_CHECK))
		np->rv_scntl0  &= ~0x0a; /* SCSI parity checking disabled */
}

#ifdef	SYM_CONF_DEBUG_NVRAM
/*
 *  Dump Symbios format NVRAM for debugging purpose.
 */
static void sym_display_Symbios_nvram(sdev_p np, Symbios_nvram *nvram)
{
	int i;

	/* display Symbios nvram host data */
	printf("%s: HOST ID=%d%s%s%s%s%s%s\n",
		sym_name(np), nvram->host_id & 0x0f,
		(nvram->flags  & SYMBIOS_SCAM_ENABLE)	? " SCAM"	:"",
		(nvram->flags  & SYMBIOS_PARITY_ENABLE)	? " PARITY"	:"",
		(nvram->flags  & SYMBIOS_VERBOSE_MSGS)	? " VERBOSE"	:"", 
		(nvram->flags  & SYMBIOS_CHS_MAPPING)	? " CHS_ALT"	:"", 
		(nvram->flags2 & SYMBIOS_AVOID_BUS_RESET)?" NO_RESET"	:"",
		(nvram->flags1 & SYMBIOS_SCAN_HI_LO)	? " HI_LO"	:"");

	/* display Symbios nvram drive data */
	for (i = 0 ; i < 15 ; i++) {
		struct Symbios_target *tn = &nvram->target[i];
		printf("%s-%d:%s%s%s%s WIDTH=%d SYNC=%d TMO=%d\n",
		sym_name(np), i,
		(tn->flags & SYMBIOS_DISCONNECT_ENABLE)	? " DISC"	: "",
		(tn->flags & SYMBIOS_SCAN_AT_BOOT_TIME)	? " SCAN_BOOT"	: "",
		(tn->flags & SYMBIOS_SCAN_LUNS)		? " SCAN_LUNS"	: "",
		(tn->flags & SYMBIOS_QUEUE_TAGS_ENABLED)? " TCQ"	: "",
		tn->bus_width,
		tn->sync_period / 4,
		tn->timeout);
	}
}

/*
 *  Dump TEKRAM format NVRAM for debugging purpose.
 */
static void sym_display_Tekram_nvram(sdev_p np, Tekram_nvram *nvram)
{
	int i, tags, boot_delay;
	char *rem;

	/* display Tekram nvram host data */
	tags = 2 << nvram->max_tags_index;
	boot_delay = 0;
	if (nvram->boot_delay_index < 6)
		boot_delay = Tekram_boot_delay[nvram->boot_delay_index];
	switch((nvram->flags & TEKRAM_REMOVABLE_FLAGS) >> 6) {
	default:
	case 0:	rem = "";			break;
	case 1: rem = " REMOVABLE=boot device";	break;
	case 2: rem = " REMOVABLE=all";		break;
	}

	printf("%s: HOST ID=%d%s%s%s%s%s%s%s%s%s BOOT DELAY=%d tags=%d\n",
		sym_name(np), nvram->host_id & 0x0f,
		(nvram->flags1 & SYMBIOS_SCAM_ENABLE)	? " SCAM"	:"",
		(nvram->flags & TEKRAM_MORE_THAN_2_DRIVES) ? " >2DRIVES":"",
		(nvram->flags & TEKRAM_DRIVES_SUP_1GB)	? " >1GB"	:"",
		(nvram->flags & TEKRAM_RESET_ON_POWER_ON) ? " RESET"	:"",
		(nvram->flags & TEKRAM_ACTIVE_NEGATION)	? " ACT_NEG"	:"",
		(nvram->flags & TEKRAM_IMMEDIATE_SEEK)	? " IMM_SEEK"	:"",
		(nvram->flags & TEKRAM_SCAN_LUNS)	? " SCAN_LUNS"	:"",
		(nvram->flags1 & TEKRAM_F2_F6_ENABLED)	? " F2_F6"	:"",
		rem, boot_delay, tags);

	/* display Tekram nvram drive data */
	for (i = 0; i <= 15; i++) {
		int sync, j;
		struct Tekram_target *tn = &nvram->target[i];
		j = tn->sync_index & 0xf;
		sync = Tekram_sync[j];
		printf("%s-%d:%s%s%s%s%s%s PERIOD=%d\n",
		sym_name(np), i,
		(tn->flags & TEKRAM_PARITY_CHECK)	? " PARITY"	: "",
		(tn->flags & TEKRAM_SYNC_NEGO)		? " SYNC"	: "",
		(tn->flags & TEKRAM_DISCONNECT_ENABLE)	? " DISC"	: "",
		(tn->flags & TEKRAM_START_CMD)		? " START"	: "",
		(tn->flags & TEKRAM_TAGGED_COMMANDS)	? " TCQ"	: "",
		(tn->flags & TEKRAM_WIDE_NEGO)		? " WIDE"	: "",
		sync);
	}
}
#endif	/* SYM_CONF_DEBUG_NVRAM */
#endif	/* SYM_CONF_NVRAM_SUPPORT */


/*
 *  Try reading Symbios or Tekram NVRAM
 */
#if SYM_CONF_NVRAM_SUPPORT
static int sym_read_Symbios_nvram (sdev_p np, Symbios_nvram *nvram);
static int sym_read_Tekram_nvram  (sdev_p np, Tekram_nvram *nvram);
#endif

int sym_read_nvram (sdev_p np, struct sym_nvram *nvp)
{
#if SYM_CONF_NVRAM_SUPPORT
	/*
	 *  Try to read SYMBIOS nvram.
	 *  Try to read TEKRAM nvram if Symbios nvram not found.
	 */
	if	(SYM_SETUP_SYMBIOS_NVRAM &&
		 !sym_read_Symbios_nvram (np, &nvp->data.Symbios)) {
		nvp->type = SYM_SYMBIOS_NVRAM;
#ifdef SYM_CONF_DEBUG_NVRAM
		sym_display_Symbios_nvram(np, &nvp->data.Symbios);
#endif
	}
	else if	(SYM_SETUP_TEKRAM_NVRAM &&
		 !sym_read_Tekram_nvram (np, &nvp->data.Tekram)) {
		nvp->type = SYM_TEKRAM_NVRAM;
#ifdef SYM_CONF_DEBUG_NVRAM
		sym_display_Tekram_nvram(np, &nvp->data.Tekram);
#endif
	}
	else
		nvp->type = 0;
#else
	nvp->type = 0;
#endif
	return nvp->type;
}


#if SYM_CONF_NVRAM_SUPPORT
/*
 *  24C16 EEPROM reading.
 *
 *  GPOI0 - data in/data out
 *  GPIO1 - clock
 *  Symbios NVRAM wiring now also used by Tekram.
 */

#define SET_BIT 0
#define CLR_BIT 1
#define SET_CLK 2
#define CLR_CLK 3

/*
 *  Set/clear data/clock bit in GPIO0
 */
static void S24C16_set_bit(sdev_p np, u_char write_bit, u_char *gpreg, 
			  int bit_mode)
{
	UDELAY (5);
	switch (bit_mode){
	case SET_BIT:
		*gpreg |= write_bit;
		break;
	case CLR_BIT:
		*gpreg &= 0xfe;
		break;
	case SET_CLK:
		*gpreg |= 0x02;
		break;
	case CLR_CLK:
		*gpreg &= 0xfd;
		break;

	}
	OUTB (nc_gpreg, *gpreg);
	UDELAY (5);
}

/*
 *  Send START condition to NVRAM to wake it up.
 */
static void S24C16_start(sdev_p np, u_char *gpreg)
{
	S24C16_set_bit(np, 1, gpreg, SET_BIT);
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	S24C16_set_bit(np, 0, gpreg, CLR_BIT);
	S24C16_set_bit(np, 0, gpreg, CLR_CLK);
}

/*
 *  Send STOP condition to NVRAM - puts NVRAM to sleep... ZZzzzz!!
 */
static void S24C16_stop(sdev_p np, u_char *gpreg)
{
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	S24C16_set_bit(np, 1, gpreg, SET_BIT);
}

/*
 *  Read or write a bit to the NVRAM,
 *  read if GPIO0 input else write if GPIO0 output
 */
static void S24C16_do_bit(sdev_p np, u_char *read_bit, u_char write_bit, 
			 u_char *gpreg)
{
	S24C16_set_bit(np, write_bit, gpreg, SET_BIT);
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	if (read_bit)
		*read_bit = INB (nc_gpreg);
	S24C16_set_bit(np, 0, gpreg, CLR_CLK);
	S24C16_set_bit(np, 0, gpreg, CLR_BIT);
}

/*
 *  Output an ACK to the NVRAM after reading,
 *  change GPIO0 to output and when done back to an input
 */
static void S24C16_write_ack(sdev_p np, u_char write_bit, u_char *gpreg, 
			    u_char *gpcntl)
{
	OUTB (nc_gpcntl, *gpcntl & 0xfe);
	S24C16_do_bit(np, 0, write_bit, gpreg);
	OUTB (nc_gpcntl, *gpcntl);
}

/*
 *  Input an ACK from NVRAM after writing,
 *  change GPIO0 to input and when done back to an output
 */
static void S24C16_read_ack(sdev_p np, u_char *read_bit, u_char *gpreg, 
			   u_char *gpcntl)
{
	OUTB (nc_gpcntl, *gpcntl | 0x01);
	S24C16_do_bit(np, read_bit, 1, gpreg);
	OUTB (nc_gpcntl, *gpcntl);
}

/*
 *  WRITE a byte to the NVRAM and then get an ACK to see it was accepted OK,
 *  GPIO0 must already be set as an output
 */
static void S24C16_write_byte(sdev_p np, u_char *ack_data, u_char write_data, 
			     u_char *gpreg, u_char *gpcntl)
{
	int x;
	
	for (x = 0; x < 8; x++)
		S24C16_do_bit(np, 0, (write_data >> (7 - x)) & 0x01, gpreg);
		
	S24C16_read_ack(np, ack_data, gpreg, gpcntl);
}

/*
 *  READ a byte from the NVRAM and then send an ACK to say we have got it,
 *  GPIO0 must already be set as an input
 */
static void S24C16_read_byte(sdev_p np, u_char *read_data, u_char ack_data, 
			    u_char *gpreg, u_char *gpcntl)
{
	int x;
	u_char read_bit;

	*read_data = 0;
	for (x = 0; x < 8; x++) {
		S24C16_do_bit(np, &read_bit, 1, gpreg);
		*read_data |= ((read_bit & 0x01) << (7 - x));
	}

	S24C16_write_ack(np, ack_data, gpreg, gpcntl);
}

/*
 *  Read 'len' bytes starting at 'offset'.
 */
static int sym_read_S24C16_nvram (sdev_p np, int offset, u_char *data, int len)
{
	u_char	gpcntl, gpreg;
	u_char	old_gpcntl, old_gpreg;
	u_char	ack_data;
	int	retv = 1;
	int	x;

	/* save current state of GPCNTL and GPREG */
	old_gpreg	= INB (nc_gpreg);
	old_gpcntl	= INB (nc_gpcntl);
	gpcntl		= old_gpcntl & 0x1c;

	/* set up GPREG & GPCNTL to set GPIO0 and GPIO1 in to known state */
	OUTB (nc_gpreg,  old_gpreg);
	OUTB (nc_gpcntl, gpcntl);

	/* this is to set NVRAM into a known state with GPIO0/1 both low */
	gpreg = old_gpreg;
	S24C16_set_bit(np, 0, &gpreg, CLR_CLK);
	S24C16_set_bit(np, 0, &gpreg, CLR_BIT);
		
	/* now set NVRAM inactive with GPIO0/1 both high */
	S24C16_stop(np, &gpreg);
	
	/* activate NVRAM */
	S24C16_start(np, &gpreg);

	/* write device code and random address MSB */
	S24C16_write_byte(np, &ack_data,
		0xa0 | ((offset >> 7) & 0x0e), &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* write random address LSB */
	S24C16_write_byte(np, &ack_data,
		offset & 0xff, &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* regenerate START state to set up for reading */
	S24C16_start(np, &gpreg);
	
	/* rewrite device code and address MSB with read bit set (lsb = 0x01) */
	S24C16_write_byte(np, &ack_data,
		0xa1 | ((offset >> 7) & 0x0e), &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* now set up GPIO0 for inputting data */
	gpcntl |= 0x01;
	OUTB (nc_gpcntl, gpcntl);
		
	/* input all requested data - only part of total NVRAM */
	for (x = 0; x < len; x++) 
		S24C16_read_byte(np, &data[x], (x == (len-1)), &gpreg, &gpcntl);

	/* finally put NVRAM back in inactive mode */
	gpcntl &= 0xfe;
	OUTB (nc_gpcntl, gpcntl);
	S24C16_stop(np, &gpreg);
	retv = 0;
out:
	/* return GPIO0/1 to original states after having accessed NVRAM */
	OUTB (nc_gpcntl, old_gpcntl);
	OUTB (nc_gpreg,  old_gpreg);

	return retv;
}

#undef SET_BIT
#undef CLR_BIT
#undef SET_CLK
#undef CLR_CLK

/*
 *  Try reading Symbios NVRAM.
 *  Return 0 if OK.
 */
static int sym_read_Symbios_nvram (sdev_p np, Symbios_nvram *nvram)
{
	static u_char Symbios_trailer[6] = {0xfe, 0xfe, 0, 0, 0, 0};
	u_char *data = (u_char *) nvram;
	int len  = sizeof(*nvram);
	u_short	csum;
	int x;

	/* probe the 24c16 and read the SYMBIOS 24c16 area */
	if (sym_read_S24C16_nvram (np, SYMBIOS_NVRAM_ADDRESS, data, len))
		return 1;

	/* check valid NVRAM signature, verify byte count and checksum */
	if (nvram->type != 0 ||
	    bcmp(nvram->trailer, Symbios_trailer, 6) ||
	    nvram->byte_count != len - 12)
		return 1;

	/* verify checksum */
	for (x = 6, csum = 0; x < len - 6; x++)
		csum += data[x];
	if (csum != nvram->checksum)
		return 1;

	return 0;
}

/*
 *  93C46 EEPROM reading.
 *
 *  GPOI0 - data in
 *  GPIO1 - data out
 *  GPIO2 - clock
 *  GPIO4 - chip select
 *
 *  Used by Tekram.
 */

/*
 *  Pulse clock bit in GPIO0
 */
static void T93C46_Clk(sdev_p np, u_char *gpreg)
{
	OUTB (nc_gpreg, *gpreg | 0x04);
	UDELAY (2);
	OUTB (nc_gpreg, *gpreg);
}

/* 
 *  Read bit from NVRAM
 */
static void T93C46_Read_Bit(sdev_p np, u_char *read_bit, u_char *gpreg)
{
	UDELAY (2);
	T93C46_Clk(np, gpreg);
	*read_bit = INB (nc_gpreg);
}

/*
 *  Write bit to GPIO0
 */
static void T93C46_Write_Bit(sdev_p np, u_char write_bit, u_char *gpreg)
{
	if (write_bit & 0x01)
		*gpreg |= 0x02;
	else
		*gpreg &= 0xfd;
		
	*gpreg |= 0x10;
		
	OUTB (nc_gpreg, *gpreg);
	UDELAY (2);

	T93C46_Clk(np, gpreg);
}

/*
 *  Send STOP condition to NVRAM - puts NVRAM to sleep... ZZZzzz!!
 */
static void T93C46_Stop(sdev_p np, u_char *gpreg)
{
	*gpreg &= 0xef;
	OUTB (nc_gpreg, *gpreg);
	UDELAY (2);

	T93C46_Clk(np, gpreg);
}

/*
 *  Send read command and address to NVRAM
 */
static void T93C46_Send_Command(sdev_p np, u_short write_data, 
				u_char *read_bit, u_char *gpreg)
{
	int x;

	/* send 9 bits, start bit (1), command (2), address (6)  */
	for (x = 0; x < 9; x++)
		T93C46_Write_Bit(np, (u_char) (write_data >> (8 - x)), gpreg);

	*read_bit = INB (nc_gpreg);
}

/*
 *  READ 2 bytes from the NVRAM
 */
static void T93C46_Read_Word(sdev_p np, u_short *nvram_data, u_char *gpreg)
{
	int x;
	u_char read_bit;

	*nvram_data = 0;
	for (x = 0; x < 16; x++) {
		T93C46_Read_Bit(np, &read_bit, gpreg);

		if (read_bit & 0x01)
			*nvram_data |=  (0x01 << (15 - x));
		else
			*nvram_data &= ~(0x01 << (15 - x));
	}
}

/*
 *  Read Tekram NvRAM data.
 */
static int T93C46_Read_Data(sdev_p np, u_short *data,int len,u_char *gpreg)
{
	u_char	read_bit;
	int	x;

	for (x = 0; x < len; x++)  {

		/* output read command and address */
		T93C46_Send_Command(np, 0x180 | x, &read_bit, gpreg);
		if (read_bit & 0x01)
			return 1; /* Bad */
		T93C46_Read_Word(np, &data[x], gpreg);
		T93C46_Stop(np, gpreg);
	}

	return 0;
}

/*
 *  Try reading 93C46 Tekram NVRAM.
 */
static int sym_read_T93C46_nvram (sdev_p np, Tekram_nvram *nvram)
{
	u_char gpcntl, gpreg;
	u_char old_gpcntl, old_gpreg;
	int retv = 1;

	/* save current state of GPCNTL and GPREG */
	old_gpreg	= INB (nc_gpreg);
	old_gpcntl	= INB (nc_gpcntl);

	/* set up GPREG & GPCNTL to set GPIO0/1/2/4 in to known state, 0 in,
	   1/2/4 out */
	gpreg = old_gpreg & 0xe9;
	OUTB (nc_gpreg, gpreg);
	gpcntl = (old_gpcntl & 0xe9) | 0x09;
	OUTB (nc_gpcntl, gpcntl);

	/* input all of NVRAM, 64 words */
	retv = T93C46_Read_Data(np, (u_short *) nvram,
				sizeof(*nvram) / sizeof(short), &gpreg);
	
	/* return GPIO0/1/2/4 to original states after having accessed NVRAM */
	OUTB (nc_gpcntl, old_gpcntl);
	OUTB (nc_gpreg,  old_gpreg);

	return retv;
}

/*
 *  Try reading Tekram NVRAM.
 *  Return 0 if OK.
 */
static int sym_read_Tekram_nvram (sdev_p np, Tekram_nvram *nvram)
{
	u_char *data = (u_char *) nvram;
	int len = sizeof(*nvram);
	u_short	csum;
	int x;

	switch (np->device_id) {
	case PCI_ID_SYM53C885:
	case PCI_ID_SYM53C895:
	case PCI_ID_SYM53C896:
		x = sym_read_S24C16_nvram(np, TEKRAM_24C16_NVRAM_ADDRESS,
					  data, len);
		break;
	case PCI_ID_SYM53C875:
		x = sym_read_S24C16_nvram(np, TEKRAM_24C16_NVRAM_ADDRESS,
					  data, len);
		if (!x)
			break;
	default:
		x = sym_read_T93C46_nvram(np, nvram);
		break;
	}
	if (x)
		return 1;

	/* verify checksum */
	for (x = 0, csum = 0; x < len - 1; x += 2)
		csum += data[x] + (data[x+1] << 8);
	if (csum != 0x1234)
		return 1;

	return 0;
}

#endif	/* SYM_CONF_NVRAM_SUPPORT */
