/* $Id: ide.c,v 1.30 2003/07/08 07:24:47 pkj Exp $
 *
 * Etrax specific IDE functions, like init and PIO-mode setting etc.
 * Almost the entire ide.c is used for the rest of the Etrax ATA driver.
 * Copyright (c) 2000, 2001, 2002 Axis Communications AB 
 *
 * Authors:    Bjorn Wesen        (initial version)
 *             Mikael Starvik     (pio setup stuff)
 *
 * $Log: ide.c,v $
 * Revision 1.30  2003/07/08 07:24:47  pkj
 * Corrected spelling mistakes originally found in 2.5.x
 *
 * Revision 1.29  2003/06/17 13:57:49  starvik
 * Merge of Linux 2.4.21
 *
 * Revision 1.28  2003/01/22 12:41:12  starvik
 * Added LBA48 support
 * Fixed typo in e100_ideproc
 *
 * Revision 1.27  2003/01/09 18:03:47  starvik
 * init_ioremap is now called by kernel before device drivers are initialized
 *
 * Revision 1.26  2002/09/17 12:16:59  bjornw
 * Removed unnecessary cli/sti pair
 *
 * Revision 1.25  2002/08/19 08:07:19  matsfg
 * Added IN_WORD.
 *
 * Revision 1.24  2002/04/22 11:47:21  johana
 * Fix according to 2.4.19-pre7. time_after/time_before and
 * missing end of comment.
 * The patch has a typo for ethernet.c in e100_clear_network_leds(),
 *  that is fixed here.
 *
 * Revision 1.23  2002/03/19 15:35:51  bjornw
 * Cleaned up the bus-reset code a bit and made G27-reset work
 *
 * Revision 1.22  2002/03/19 15:23:05  bjornw
 * Added flush_etrax_cache before starting the receiving DMA
 *
 * Revision 1.21  2002/03/06 15:37:56  hp
 * 	* ide.c (OUT_BYTE): If timing out in the first busy-loop, do a
 * 	printk, fall through and still do the write.
 * 	(IN_BYTE): If timing out in the first loop, and reg indicated it's
 * 	the ATA status register in the device being read, return BUSY_STAT.
 *
 * Revision 1.20  2002/02/22 11:47:56  bjornw
 * Added a timeout to IN_BYTE and OUT_BYTE
 *
 * Revision 1.19  2001/05/09 12:53:16  johana
 * Added #include <asm/dma.h>
 *
 * Revision 1.18  2001/05/09 12:37:00  johana
 * Use DMA_NBR macros from dma.h.
 *
 * Revision 1.17  2001/04/23 13:36:30  matsfg
 * Changed CONFIG_IDE_DELAY to CONFIG_ETRAX_IDE_DELAY
 *
 * Revision 1.16  2001/04/05 08:30:07  matsfg
 * Corrected cse1 and csp0 reset.
 *
 * Revision 1.15  2001/04/04 14:34:06  bjornw
 * Re-instated code that mysteriously disappeared during review updates.
 *
 * Revision 1.14  2001/04/04 13:45:12  matsfg
 * Calls REG_SHADOW_SET for cse1 reset so only the resetbit is affected
 *
 * Revision 1.13  2001/04/04 13:26:40  matsfg
 * memmapping is done in init.c
 *
 * Revision 1.12  2001/04/04 11:37:56  markusl
 * Updated according to review remarks
 *
 * Revision 1.11  2001/03/29 12:49:14  matsfg
 * Changed check for ata_tot_size from >= to >.
 * Sets sw_len to 0 if size is exactly 65536.
 *
 * Revision 1.10  2001/03/16 09:39:30  matsfg
 * Support for reset on port CSP0
 *
 * Revision 1.9  2001/03/01 13:11:18  bjornw
 * 100 -> HZ
 *
 * Revision 1.8  2001/03/01 09:32:56  matsfg
 * Moved IDE delay to a CONFIG-parameter instead
 *
 * Revision 1.7  2001/02/23 13:46:38  bjornw
 * Spellling check
 *
 * Revision 1.6  2001/02/22 15:44:30  bjornw
 * * Use ioremap when mapping the CSE1 memory-mapped reset-line for LX v2
 * * sw_len for a 65536 descriptor is 0, not 65536
 * * Express concern for G27 reset code
 *
 * Revision 1.5  2001/02/16 07:35:38  matsfg
 * Now handles DMA request blocks between 64k and 128k by split into two descriptors.
 *
 * Revision 1.4  2001/01/10 21:14:32  bjornw
 * Initialize hwif->ideproc, for the new way of handling ide_xxx_data
 *
 * Revision 1.3  2000/12/01 17:48:18  bjornw
 * - atapi_output_bytes now uses DMA
 * - dma_active check removed - the kernel does proper serializing and it had
 *   a race-condition anyway
 * - ide_build_dmatable had a nameclash
 * - re-added the RESET_DMA thingys because sometimes the interface can get
 *   stuck apparently
 * - added ide_release_dma
 *
 * Revision 1.2  2000/11/29 17:31:29  bjornw
 * 2.4 port
 *
 * - The "register addresses" stored in the hwif are now 32-bit fields that
 *   don't need to be shifted into correct positions in R_ATA_CTRL_DATA
 * - PIO-mode detection temporarily disabled since ide-modes.c is not compiled
 * - All DMA uses virt_to_phys conversions for DMA buffers and descriptor ptrs
 * - Probably correct ide_dma_begin semantics in dmaproc now for ATAPI devices
 * - Removed RESET_DMA when starting a new transfer - why was this necessary ?
 * - Indentation fix
 *
 *
 */

/* Regarding DMA: 
 *
 * There are two forms of DMA - "DMA handshaking" between the interface and the drive,
 * and DMA between the memory and the interface. We can ALWAYS use the latter, since it's
 * something built-in in the Etrax. However only some drives support the DMA-mode handshaking
 * on the ATA-bus. The normal PC driver and Triton interface disables memory-if DMA when the
 * device can't do DMA handshaking for some stupid reason. We don't need to do that.
 */ 

#undef REALLY_SLOW_IO           /* most systems can safely undef this */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/svinto.h>
#include <asm/dma.h>

/* number of Etrax DMA descriptors */
#define MAX_DMA_DESCRS 64

/* number of times to retry busy-flags when reading/writing IDE-registers 
 * this can't be too high because a hung harddisk might cause the watchdog
 * to trigger (sometimes IN_BYTE and OUT_BYTE are called with irq's disabled)
 */

#define IDE_REGISTER_TIMEOUT 300

#ifdef CONFIG_ETRAX_IDE_CSE1_16_RESET
/* address where the memory-mapped IDE reset bit lives, if used */
static volatile unsigned long *reset_addr;
#endif

static int e100_read_command = 0;

#define LOWDB(x)
#define D(x) 

void 
OUT_BYTE(unsigned char data, ide_ioreg_t reg) {
	int timeleft;
	LOWDB(printk("ob: data 0x%x, reg 0x%x\n", data, reg));

	/* note the lack of handling any timeouts. we stop waiting, but we don't
	 * really notify anybody.
	 */

	timeleft = IDE_REGISTER_TIMEOUT;
	/* wait for busy flag */
	while(timeleft && (*R_ATA_STATUS_DATA & IO_MASK(R_ATA_STATUS_DATA, busy)))
		timeleft--;

	/*
	 * Fall through at a timeout, so the ongoing command will be
	 * aborted by the write below, which is expected to be a dummy
	 * command to the command register.  This happens when a faulty
	 * drive times out on a command.  See comment on timeout in
	 * IN_BYTE.
	 */
	if(!timeleft)
		printk("ATA timeout reg 0x%lx := 0x%x\n", reg, data);

	*R_ATA_CTRL_DATA = reg | data; /* write data to the drive's register */

	timeleft = IDE_REGISTER_TIMEOUT;
	/* wait for transmitter ready */
	while(timeleft && !(*R_ATA_STATUS_DATA &
			    IO_MASK(R_ATA_STATUS_DATA, tr_rdy)))
		timeleft--;
}

unsigned short 
IN_BYTE(ide_ioreg_t reg) {
	int status;
	int timeleft;

	timeleft = IDE_REGISTER_TIMEOUT;
	/* wait for busy flag */
	while(timeleft && (*R_ATA_STATUS_DATA & IO_MASK(R_ATA_STATUS_DATA, busy)))
		timeleft--;

	if(!timeleft) {
		/*
		 * If we're asked to read the status register, like for
		 * example when a command does not complete for an
		 * extended time, but the ATA interface is stuck in a
		 * busy state at the *ETRAX* ATA interface level (as has
		 * happened repeatedly with at least one bad disk), then
		 * the best thing to do is to pretend that we read
		 * "busy" in the status register, so the IDE driver will
		 * time-out, abort the ongoing command and perform a
		 * reset sequence.  Note that the subsequent OUT_BYTE
		 * call will also timeout on busy, but as long as the
		 * write is still performed, everything will be fine.
		 */
		if ((reg & IO_MASK (R_ATA_CTRL_DATA, addr))
		    == IO_FIELD (R_ATA_CTRL_DATA, addr, IDE_STATUS_OFFSET))
			return BUSY_STAT;
		else
			/* For other rare cases we assume 0 is good enough.  */
			return 0;
	}

	*R_ATA_CTRL_DATA = reg | IO_STATE(R_ATA_CTRL_DATA, rw, read); /* read data */ 

	timeleft = IDE_REGISTER_TIMEOUT;
	/* wait for available */
	while(timeleft && !((status = *R_ATA_STATUS_DATA) &
			    IO_MASK(R_ATA_STATUS_DATA, dav)))
		timeleft--;

	if(!timeleft)
		return 0;

	LOWDB(printk("inb: 0x%x from reg 0x%x\n", status & 0xff, reg));

        return (unsigned char)status;
}

/* PIO timing (in R_ATA_CONFIG)
 *
 *                        _____________________________
 * ADDRESS :     ________/
 *
 *                            _______________
 * DIOR    :     ____________/               \__________
 *
 *                               _______________
 * DATA    :     XXXXXXXXXXXXXXXX_______________XXXXXXXX
 *
 *
 * DIOR is unbuffered while address and data is buffered.
 * This creates two problems:
 * 1. The DIOR pulse is to early (because it is unbuffered)
 * 2. The rise time of DIOR is long
 *
 * There are at least three different plausible solutions
 * 1. Use a pad capable of larger currents in Etrax
 * 2. Use an external buffer
 * 3. Make the strobe pulse longer
 *
 * Some of the strobe timings below are modified to compensate
 * for this. This implies a slight performance decrease.
 *
 * THIS SHOULD NEVER BE CHANGED!
 *
 * TODO: Is this true for the latest LX boards still ?
 */

#define ATA_DMA2_STROBE  4 
#define ATA_DMA2_HOLD    0
#define ATA_DMA1_STROBE  4 
#define ATA_DMA1_HOLD    1
#define ATA_DMA0_STROBE 12 
#define ATA_DMA0_HOLD    9
#define ATA_PIO4_SETUP   1
#define ATA_PIO4_STROBE  5
#define ATA_PIO4_HOLD    0
#define ATA_PIO3_SETUP   1
#define ATA_PIO3_STROBE  5
#define ATA_PIO3_HOLD    1
#define ATA_PIO2_SETUP   1
#define ATA_PIO2_STROBE  6
#define ATA_PIO2_HOLD    2
#define ATA_PIO1_SETUP   2
#define ATA_PIO1_STROBE 11
#define ATA_PIO1_HOLD    4
#define ATA_PIO0_SETUP   4
#define ATA_PIO0_STROBE 19
#define ATA_PIO0_HOLD    4

static int e100_dma_check (ide_drive_t *drive);
static int e100_dma_begin (ide_drive_t *drive);
static int e100_dma_end (ide_drive_t *drive);
static int e100_dma_read (ide_drive_t *drive);
static int e100_dma_write (ide_drive_t *drive);
static void e100_ide_input_data (ide_drive_t *drive, void *, unsigned int);
static void e100_ide_output_data (ide_drive_t *drive, void *, unsigned int);
static void e100_atapi_input_bytes(ide_drive_t *drive, void *, unsigned int);
static void e100_atapi_output_bytes(ide_drive_t *drive, void *, unsigned int);

/*
 * good_dma_drives() lists the model names (from "hdparm -i")
 * of drives which do not support mword2 DMA but which are
 * known to work fine with this interface under Linux.
 */

const char *good_dma_drives[] = {"Micropolis 2112A",
				 "CONNER CTMA 4000",
				 "CONNER CTT8000-A",
				 NULL};

static void tune_e100_ide(ide_drive_t *drive, byte pio)
{
	pio = 4;
	/* pio = ide_get_best_pio_mode(drive, pio, 4, NULL); */
	
	/* set pio mode! */
  	 
	switch(pio) {
		case 0:
			*R_ATA_CONFIG = ( IO_FIELD( R_ATA_CONFIG, enable,     1 ) |
					  IO_FIELD( R_ATA_CONFIG, dma_strobe, ATA_DMA2_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, dma_hold,   ATA_DMA2_HOLD ) |
					  IO_FIELD( R_ATA_CONFIG, pio_setup,  ATA_PIO0_SETUP ) |
					  IO_FIELD( R_ATA_CONFIG, pio_strobe, ATA_PIO0_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, pio_hold,   ATA_PIO0_HOLD ) );
			break;
		case 1:
			*R_ATA_CONFIG = ( IO_FIELD( R_ATA_CONFIG, enable,     1 ) |
					  IO_FIELD( R_ATA_CONFIG, dma_strobe, ATA_DMA2_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, dma_hold,   ATA_DMA2_HOLD ) |
					  IO_FIELD( R_ATA_CONFIG, pio_setup,  ATA_PIO1_SETUP ) |
					  IO_FIELD( R_ATA_CONFIG, pio_strobe, ATA_PIO1_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, pio_hold,   ATA_PIO1_HOLD ) );
			break;
		case 2:
			*R_ATA_CONFIG = ( IO_FIELD( R_ATA_CONFIG, enable,     1 ) |
					  IO_FIELD( R_ATA_CONFIG, dma_strobe, ATA_DMA2_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, dma_hold,   ATA_DMA2_HOLD ) |
					  IO_FIELD( R_ATA_CONFIG, pio_setup,  ATA_PIO2_SETUP ) |
					  IO_FIELD( R_ATA_CONFIG, pio_strobe, ATA_PIO2_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, pio_hold,   ATA_PIO2_HOLD ) );
			break;
		case 3:
			*R_ATA_CONFIG = ( IO_FIELD( R_ATA_CONFIG, enable,     1 ) |
					  IO_FIELD( R_ATA_CONFIG, dma_strobe, ATA_DMA2_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, dma_hold,   ATA_DMA2_HOLD ) |
					  IO_FIELD( R_ATA_CONFIG, pio_setup,  ATA_PIO3_SETUP ) |
					  IO_FIELD( R_ATA_CONFIG, pio_strobe, ATA_PIO3_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, pio_hold,   ATA_PIO3_HOLD ) );
			break;
		case 4:
			*R_ATA_CONFIG = ( IO_FIELD( R_ATA_CONFIG, enable,     1 ) |
					  IO_FIELD( R_ATA_CONFIG, dma_strobe, ATA_DMA2_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, dma_hold,   ATA_DMA2_HOLD ) |
					  IO_FIELD( R_ATA_CONFIG, pio_setup,  ATA_PIO4_SETUP ) |
					  IO_FIELD( R_ATA_CONFIG, pio_strobe, ATA_PIO4_STROBE ) |
					  IO_FIELD( R_ATA_CONFIG, pio_hold,   ATA_PIO4_HOLD ) );
			break;
	}
}

void __init 
init_e100_ide (void)
{
	volatile unsigned int dummy;
	int h;

	printk("ide: ETRAX 100LX built-in ATA DMA controller\n");

	/* first fill in some stuff in the ide_hwifs fields */
	
	for(h = 0; h < MAX_HWIFS; h++) {
		ide_hwif_t *hwif = &ide_hwifs[h];
		hwif->chipset = ide_etrax100;
		hwif->tuneproc = &tune_e100_ide;
                hwif->ata_input_data = &e100_ide_input_data;
                hwif->ata_output_data = &e100_ide_output_data;
                hwif->atapi_input_bytes = &e100_atapi_input_bytes;
                hwif->atapi_output_bytes = &e100_atapi_output_bytes;
                hwif->ide_dma_check = &e100_dma_check;
                hwif->ide_dma_end = &e100_dma_end;
		hwif->ide_dma_write = &e100_dma_write;
		hwif->ide_dma_read = &e100_dma_read;
		hwif->ide_dma_begin = &e100_dma_begin;
	}

	/* actually reset and configure the etrax100 ide/ata interface */

	*R_ATA_CTRL_DATA = 0;
	*R_ATA_TRANSFER_CNT = 0;
	*R_ATA_CONFIG = 0;

	genconfig_shadow = (genconfig_shadow & 
			    ~IO_MASK(R_GEN_CONFIG, dma2) &
			    ~IO_MASK(R_GEN_CONFIG, dma3) &
			    ~IO_MASK(R_GEN_CONFIG, ata)) | 
		( IO_STATE( R_GEN_CONFIG, dma3, ata    ) |
		  IO_STATE( R_GEN_CONFIG, dma2, ata    ) |
		  IO_STATE( R_GEN_CONFIG, ata,  select ) );

	*R_GEN_CONFIG = genconfig_shadow;

        /* pull the chosen /reset-line low */
        
#ifdef CONFIG_ETRAX_IDE_G27_RESET
        REG_SHADOW_SET(R_PORT_G_DATA, port_g_data_shadow, 27, 0);
#endif 
#ifdef CONFIG_ETRAX_IDE_CSE1_16_RESET
        REG_SHADOW_SET(port_cse1_addr, port_cse1_shadow, 16, 0);
#endif
#ifdef CONFIG_ETRAX_IDE_CSP0_8_RESET
        REG_SHADOW_SET(port_csp0_addr, port_csp0_shadow, 8, 0);
#endif
#ifdef CONFIG_ETRAX_IDE_PB7_RESET  
	port_pb_dir_shadow = port_pb_dir_shadow | 
		IO_STATE(R_PORT_PB_DIR, dir7, output);
	*R_PORT_PB_DIR = port_pb_dir_shadow;
	REG_SHADOW_SET(R_PORT_PB_DATA, port_pb_data_shadow, 7, 1);
#endif

	/* wait some */

	udelay(25);

	/* de-assert bus-reset */

#ifdef CONFIG_ETRAX_IDE_CSE1_16_RESET
	REG_SHADOW_SET(port_cse1_addr, port_cse1_shadow, 16, 1);
#endif
#ifdef CONFIG_ETRAX_IDE_CSP0_8_RESET
	REG_SHADOW_SET(port_csp0_addr, port_csp0_shadow, 8, 1);
#endif
#ifdef CONFIG_ETRAX_IDE_G27_RESET
	REG_SHADOW_SET(R_PORT_G_DATA, port_g_data_shadow, 27, 1);
#endif 

	/* make a dummy read to set the ata controller in a proper state */
	dummy = *R_ATA_STATUS_DATA;
  
	*R_ATA_CONFIG = ( IO_FIELD( R_ATA_CONFIG, enable,     1 ) |
			  IO_FIELD( R_ATA_CONFIG, dma_strobe, ATA_DMA2_STROBE ) |
			  IO_FIELD( R_ATA_CONFIG, dma_hold,   ATA_DMA2_HOLD ) |
			  IO_FIELD( R_ATA_CONFIG, pio_setup,  ATA_PIO4_SETUP ) |
			  IO_FIELD( R_ATA_CONFIG, pio_strobe, ATA_PIO4_STROBE ) |
			  IO_FIELD( R_ATA_CONFIG, pio_hold,   ATA_PIO4_HOLD ) );

	*R_ATA_CTRL_DATA = ( IO_STATE( R_ATA_CTRL_DATA, rw,   read) |
			     IO_FIELD( R_ATA_CTRL_DATA, addr, 1   ) );

	while(*R_ATA_STATUS_DATA & IO_MASK(R_ATA_STATUS_DATA, busy)); /* wait for busy flag*/

	*R_IRQ_MASK0_SET = ( IO_STATE( R_IRQ_MASK0_SET, ata_irq0, set ) |
			     IO_STATE( R_IRQ_MASK0_SET, ata_irq1, set ) |
			     IO_STATE( R_IRQ_MASK0_SET, ata_irq2, set ) |
			     IO_STATE( R_IRQ_MASK0_SET, ata_irq3, set ) );

	printk("ide: waiting %d seconds for drives to regain consciousness\n",
	       CONFIG_ETRAX_IDE_DELAY);

	h = jiffies + (CONFIG_ETRAX_IDE_DELAY * HZ);
	while(time_before(jiffies, h)) /* nothing */ ;

	/* reset the dma channels we will use */

	RESET_DMA(ATA_TX_DMA_NBR);
	RESET_DMA(ATA_RX_DMA_NBR);
	WAIT_DMA(ATA_TX_DMA_NBR);
	WAIT_DMA(ATA_RX_DMA_NBR);

}

static etrax_dma_descr mydescr;

/*
 * The following routines are mainly used by the ATAPI drivers.
 *
 * These routines will round up any request for an odd number of bytes,
 * so if an odd bytecount is specified, be sure that there's at least one
 * extra byte allocated for the buffer.
 */
static void 
e100_atapi_input_bytes (ide_drive_t *drive, void *buffer, unsigned int bytecount)
{
	ide_ioreg_t data_reg = IDE_DATA_REG;

	D(printk("atapi_input_bytes, dreg 0x%x, buffer 0x%x, count %d\n",
		 data_reg, buffer, bytecount));
	
	if(bytecount & 1) {
		printk("warning, odd bytecount in cdrom_in_bytes = %d.\n", bytecount);
		bytecount++; /* to round off */
	}
	
	/* make sure the DMA channel is available */
	RESET_DMA(ATA_RX_DMA_NBR);
	WAIT_DMA(ATA_RX_DMA_NBR); 
	
	/* setup DMA descriptor */
	
	mydescr.sw_len = bytecount;
	mydescr.ctrl   = d_eol;
	mydescr.buf    = virt_to_phys(buffer);
	
	/* start the dma channel */
	
	*R_DMA_CH3_FIRST = virt_to_phys(&mydescr);
	*R_DMA_CH3_CMD   = IO_STATE(R_DMA_CH3_CMD, cmd, start);
	
	/* initiate a multi word dma read using PIO handshaking */
	
	*R_ATA_TRANSFER_CNT = IO_FIELD(R_ATA_TRANSFER_CNT, count, bytecount >> 1);
	
	*R_ATA_CTRL_DATA = data_reg |
		IO_STATE(R_ATA_CTRL_DATA, rw,       read) |
		IO_STATE(R_ATA_CTRL_DATA, src_dst,  dma) |
		IO_STATE(R_ATA_CTRL_DATA, handsh,   pio) |
		IO_STATE(R_ATA_CTRL_DATA, multi,    on) |
		IO_STATE(R_ATA_CTRL_DATA, dma_size, word);
	
	/* wait for completion */
	
	LED_DISK_READ(1);
	WAIT_DMA(ATA_RX_DMA_NBR);
	LED_DISK_READ(0);

#if 0
        /* old polled transfer code
	 * this should be moved into a new function that can do polled
	 * transfers if DMA is not available
	 */
        
        /* initiate a multi word read */
        
        *R_ATA_TRANSFER_CNT = wcount << 1;
        
        *R_ATA_CTRL_DATA = data_reg |
                IO_STATE(R_ATA_CTRL_DATA, rw,       read) |
                IO_STATE(R_ATA_CTRL_DATA, src_dst,  register) |
                IO_STATE(R_ATA_CTRL_DATA, handsh,   pio) |
                IO_STATE(R_ATA_CTRL_DATA, multi,    on) |
                IO_STATE(R_ATA_CTRL_DATA, dma_size, word);
        
        /* svinto has a latency until the busy bit actually is set */
        
        nop(); nop();
        nop(); nop();
        nop(); nop();
        nop(); nop();
        nop(); nop();
        
        /* unit should be busy during multi transfer */
        while((status = *R_ATA_STATUS_DATA) & IO_MASK(R_ATA_STATUS_DATA, busy)) {
                while(!(status & IO_MASK(R_ATA_STATUS_DATA, dav)))
                        status = *R_ATA_STATUS_DATA;
                *ptr++ = (unsigned short)(status & 0xffff);
        }
#endif
}

static void 
e100_atapi_output_bytes (ide_drive_t *drive, void *buffer, unsigned int bytecount)
{
	ide_ioreg_t data_reg = IDE_DATA_REG;
	
	D(printk("atapi_output_bytes, dreg 0x%x, buffer 0x%x, count %d\n",
		 data_reg, buffer, bytecount));

	if(bytecount & 1) {
		printk("odd bytecount %d in atapi_out_bytes!\n", bytecount);
		bytecount++;
	}

	/* make sure the DMA channel is available */
	RESET_DMA(ATA_TX_DMA_NBR);
	WAIT_DMA(ATA_TX_DMA_NBR); 
	
	/* setup DMA descriptor */
	
	mydescr.sw_len = bytecount;
	mydescr.ctrl   = d_eol;
	mydescr.buf    = virt_to_phys(buffer);

	/* start the dma channel */
	
	*R_DMA_CH2_FIRST = virt_to_phys(&mydescr);
	*R_DMA_CH2_CMD   = IO_STATE(R_DMA_CH2_CMD, cmd, start);
	
	/* initiate a multi word dma write using PIO handshaking */
	
	*R_ATA_TRANSFER_CNT = IO_FIELD(R_ATA_TRANSFER_CNT, count, bytecount >> 1);
	
	*R_ATA_CTRL_DATA = data_reg |
		IO_STATE(R_ATA_CTRL_DATA, rw,       write) |
		IO_STATE(R_ATA_CTRL_DATA, src_dst,  dma) |
		IO_STATE(R_ATA_CTRL_DATA, handsh,   pio) |
		IO_STATE(R_ATA_CTRL_DATA, multi,    on) |
		IO_STATE(R_ATA_CTRL_DATA, dma_size, word);
	
	/* wait for completion */
	
	LED_DISK_WRITE(1);
	WAIT_DMA(ATA_TX_DMA_NBR);
	LED_DISK_WRITE(0);

#if 0
        /* old polled write code - see comment in input_bytes */

	/* wait for busy flag */
        while(*R_ATA_STATUS_DATA & IO_MASK(R_ATA_STATUS_DATA, busy)); 

        /* initiate a multi word write */

        *R_ATA_TRANSFER_CNT = bytecount >> 1;

        ctrl = data_reg |
                IO_STATE(R_ATA_CTRL_DATA, rw,       write) |
                IO_STATE(R_ATA_CTRL_DATA, src_dst,  register) |
                IO_STATE(R_ATA_CTRL_DATA, handsh,   pio) |
                IO_STATE(R_ATA_CTRL_DATA, multi,    on) |
                IO_STATE(R_ATA_CTRL_DATA, dma_size, word);
        
        LED_DISK_WRITE(1);
        
        /* Etrax will set busy = 1 until the multi pio transfer has finished
         * and tr_rdy = 1 after each successful word transfer.
         * When the last byte has been transferred Etrax will first set tr_tdy = 1 
         * and then busy = 0 (not in the same cycle). If we read busy before it
         * has been set to 0 we will think that we should transfer more bytes 
         * and then tr_rdy would be 0 forever. This is solved by checking busy
         * in the inner loop.
         */
        
        do {
                *R_ATA_CTRL_DATA = ctrl | *ptr++;
                while(!(*R_ATA_STATUS_DATA & IO_MASK(R_ATA_STATUS_DATA, tr_rdy)) &&
                      (*R_ATA_STATUS_DATA & IO_MASK(R_ATA_STATUS_DATA, busy)));
        } while(*R_ATA_STATUS_DATA & IO_MASK(R_ATA_STATUS_DATA, busy));

        LED_DISK_WRITE(0);
#endif  

}

/*
 * This is used for most PIO data transfers *from* the IDE interface
 */
static void 
e100_ide_input_data (ide_drive_t *drive, void *buffer, unsigned int wcount)
{
	e100_atapi_input_bytes(drive, buffer, wcount << 2);
}

/*
 * This is used for most PIO data transfers *to* the IDE interface
 */
static void
e100_ide_output_data (ide_drive_t *drive, void *buffer, unsigned int wcount)
{
	e100_atapi_output_bytes(drive, buffer, wcount << 2);
}

/* we only have one DMA channel on the chip for ATA, so we can keep these statically */
static etrax_dma_descr ata_descrs[MAX_DMA_DESCRS];
static unsigned int ata_tot_size;

/*
 * e100_ide_build_dmatable() prepares a dma request.
 * Returns 0 if all went okay, returns 1 otherwise.
 */
static int e100_ide_build_dmatable (ide_drive_t *drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	struct buffer_head *bh = rq->bh;
	unsigned long size, addr;
	unsigned int count = 0;

	ata_tot_size = 0;

	do {
		/*
		 * Determine addr and size of next buffer area.  We assume that
		 * individual virtual buffers are always composed linearly in
		 * physical memory.  For example, we assume that any 8kB buffer
		 * is always composed of two adjacent physical 4kB pages rather
		 * than two possibly non-adjacent physical 4kB pages.
		 */
		if (bh == NULL) {  /* paging and tape requests have (rq->bh == NULL) */
			addr = virt_to_phys (rq->buffer);
			size = rq->nr_sectors << 9;
		} else {
			/* group sequential buffers into one large buffer */
			addr = virt_to_phys (bh->b_data);
			size = bh->b_size;
			while ((bh = bh->b_reqnext) != NULL) {
				if ((addr + size) != virt_to_phys (bh->b_data))
					break;
				size += bh->b_size;
			}
		}

		/* did we run out of descriptors? */

		if(count >= MAX_DMA_DESCRS) {
			printk("%s: too few DMA descriptors\n", drive->name);
			return 1;
		}

		/* however, this case is more difficult - R_ATA_TRANSFER_CNT cannot be more
		   than 65536 words per transfer, so in that case we need to either 
		   1) use a DMA interrupt to re-trigger R_ATA_TRANSFER_CNT and continue with
		      the descriptors, or
		   2) simply do the request here, and get dma_intr to only ide_end_request on 
		      those blocks that were actually set-up for transfer.
		*/

		if(ata_tot_size + size > 131072) {
			printk("too large total ATA DMA request, %d + %d!\n", ata_tot_size, size);
			return 1;
		}

		/* If size > 65536 it has to be splitted into new descriptors. Since we don't handle 
                   size > 131072 only one split is necessary */

		if(size > 65536) {
 		        /* ok we want to do IO at addr, size bytes. set up a new descriptor entry */
                        ata_descrs[count].sw_len = 0;  /* 0 means 65536, this is a 16-bit field */
                        ata_descrs[count].ctrl = 0;
                        ata_descrs[count].buf = addr;
                        ata_descrs[count].next = virt_to_phys(&ata_descrs[count + 1]);
                        count++;
                        ata_tot_size += 65536;
                        /* size and addr should refere to not handled data */
                        size -= 65536;
                        addr += 65536;
                }
		/* ok we want to do IO at addr, size bytes. set up a new descriptor entry */
                if(size == 65536) {
			ata_descrs[count].sw_len = 0;  /* 0 means 65536, this is a 16-bit field */
                } else {
			ata_descrs[count].sw_len = size;
                }
		ata_descrs[count].ctrl = 0;
		ata_descrs[count].buf = addr;
		ata_descrs[count].next = virt_to_phys(&ata_descrs[count + 1]);
		count++;
		ata_tot_size += size;

	} while (bh != NULL);

	if (count) {
		/* set the end-of-list flag on the last descriptor */
		ata_descrs[count - 1].ctrl |= d_eol;
		/* return and say all is ok */
		return 0;
	}

	printk("%s: empty DMA table?\n", drive->name);
	return 1;	/* let the PIO routines handle this weirdness */
}

static int config_drive_for_dma (ide_drive_t *drive)
{
        const char **list;
        struct hd_driveid *id = drive->id;

        if (id && (id->capability & 1)) {
                /* Enable DMA on any drive that supports mword2 DMA */
                if ((id->field_valid & 2) && (id->dma_mword & 0x404) == 0x404) {
                        drive->using_dma = 1;
                        return 0;               /* DMA enabled */
                }

                /* Consult the list of known "good" drives */
                list = good_dma_drives;
                while (*list) {
                        if (!strcmp(*list++,id->model)) {
                                drive->using_dma = 1;
                                return 0;       /* DMA enabled */
                        }
                }
        }
        return 1;       /* DMA not enabled */
}

/*
 * etrax_dma_intr() is the handler for disk read/write DMA interrupts
 */
static ide_startstop_t etrax_dma_intr (ide_drive_t *drive)
{
	int i, dma_stat;
	byte stat;

	LED_DISK_READ(0);
	LED_DISK_WRITE(0);

	dma_stat = HWIF(drive)->ide_dma_end(drive);
	stat = HWIF(drive)->INB(IDE_STATUS_REG);		/* get drive status */
	if (OK_STAT(stat,DRIVE_READY,drive->bad_wstat|DRQ_STAT)) {
		if (!dma_stat) {
			struct request *rq;
			rq = HWGROUP(drive)->rq;
			for (i = rq->nr_sectors; i > 0;) {
				i -= rq->current_nr_sectors;
				DRIVER(drive)->end_request(drive, 1);
			}
			return ide_stopped;
		}
		printk("%s: bad DMA status\n", drive->name);
	}
	return DRIVER(drive)->error(drive, "dma_intr", stat);
}

/*
 * Functions below initiates/aborts DMA read/write operations on a drive.
 *
 * The caller is assumed to have selected the drive and programmed the drive's
 * sector address using CHS or LBA.  All that remains is to prepare for DMA
 * and then issue the actual read/write DMA/PIO command to the drive.
 *
 * For ATAPI devices, we just prepare for DMA and return. The caller should
 * then issue the packet command to the drive and call us again with
 * ide_dma_begin afterwards.
 *
 * Returns 0 if all went well.
 * Returns 1 if DMA read/write could not be started, in which case
 * the caller should revert to PIO for the current request.
 */

static int e100_dma_check(ide_drive_t *drive)
{
	return config_drive_for_dma (drive);
}

static int e100_dma_end(ide_drive_t *drive)
{
	/* TODO: check if something went wrong with the DMA */
	return 0;
}

static int e100_start_dma(ide_drive_t *drive, int atapi, int reading)
{
	if(reading) {

		RESET_DMA(ATA_RX_DMA_NBR); /* sometimes the DMA channel get stuck so we need to do this */
		WAIT_DMA(ATA_RX_DMA_NBR);

		/* set up the Etrax DMA descriptors */
		
		if(e100_ide_build_dmatable (drive))
			return 1;

		if(!atapi) {
			/* set the irq handler which will finish the request when DMA is done */
		
			ide_set_handler(drive, &etrax_dma_intr, WAIT_CMD, NULL);
			
			/* issue cmd to drive */
                        if ((HWGROUP(drive)->rq->cmd == IDE_DRIVE_TASKFILE) &&
			    (drive->addressing == 1)) {
				ide_task_t *args = HWGROUP(drive)->rq->special;
				OUT_BYTE(args->tfRegister[IDE_COMMAND_OFFSET], IDE_COMMAND_REG);
			} else if (drive->addressing) {
				OUT_BYTE(WIN_READDMA_EXT, IDE_COMMAND_REG);
			} else {
				OUT_BYTE(WIN_READDMA, IDE_COMMAND_REG);
			}
		}

		/* begin DMA */
		
		/* need to do this before RX DMA due to a chip bug
		 * it is enough to just flush the part of the cache that
		 * corresponds to the buffers we start, but since HD transfers
		 * usually are more than 8 kB, it is easier to optimize for the
		 * normal case and just flush the entire cache. its the only
		 * way to be sure! (OB movie quote)
		 */
		flush_etrax_cache();  
		*R_DMA_CH3_FIRST = virt_to_phys(ata_descrs);
		*R_DMA_CH3_CMD   = IO_STATE(R_DMA_CH3_CMD, cmd, start);
		
		/* initiate a multi word dma read using DMA handshaking */
		
		*R_ATA_TRANSFER_CNT =
			IO_FIELD(R_ATA_TRANSFER_CNT, count, ata_tot_size >> 1);
		
		*R_ATA_CTRL_DATA =
			IO_FIELD(R_ATA_CTRL_DATA, data, IDE_DATA_REG) |
			IO_STATE(R_ATA_CTRL_DATA, rw,       read) |
			IO_STATE(R_ATA_CTRL_DATA, src_dst,  dma)  |
			IO_STATE(R_ATA_CTRL_DATA, handsh,   dma)  |
			IO_STATE(R_ATA_CTRL_DATA, multi,    on)   |
			IO_STATE(R_ATA_CTRL_DATA, dma_size, word);

		LED_DISK_READ(1);

		D(printk("dma read of %d bytes.\n", ata_tot_size));
 
	} else {
		/* writing */

		RESET_DMA(ATA_TX_DMA_NBR); /* sometimes the DMA channel get stuck so we need to do this */
		WAIT_DMA(ATA_TX_DMA_NBR);

		/* set up the Etrax DMA descriptors */

		if(e100_ide_build_dmatable (drive))
			return 1;

		if(!atapi) {
			/* set the irq handler which will finish the request when DMA is done */
				
			ide_set_handler(drive, &etrax_dma_intr, WAIT_CMD, NULL);
			
			/* issue cmd to drive */
			if ((HWGROUP(drive)->rq->cmd == IDE_DRIVE_TASKFILE) &&
			    (drive->addressing == 1)) {
				ide_task_t *args = HWGROUP(drive)->rq->special;
				OUT_BYTE(args->tfRegister[IDE_COMMAND_OFFSET], IDE_COMMAND_REG);
			} else if (drive->addressing) {
				OUT_BYTE(WIN_WRITEDMA_EXT, IDE_COMMAND_REG);
			} else {
				OUT_BYTE(WIN_WRITEDMA, IDE_COMMAND_REG);
			}
		}

		/* begin DMA */
		
		*R_DMA_CH2_FIRST = virt_to_phys(ata_descrs);
		*R_DMA_CH2_CMD   = IO_STATE(R_DMA_CH2_CMD, cmd, start);
		
		/* initiate a multi word dma write using DMA handshaking */
		
		*R_ATA_TRANSFER_CNT =
			IO_FIELD(R_ATA_TRANSFER_CNT, count, ata_tot_size >> 1);
		
		*R_ATA_CTRL_DATA =
			IO_FIELD(R_ATA_CTRL_DATA, data,     IDE_DATA_REG) |
			IO_STATE(R_ATA_CTRL_DATA, rw,       write) |
			IO_STATE(R_ATA_CTRL_DATA, src_dst,  dma) |
			IO_STATE(R_ATA_CTRL_DATA, handsh,   dma) |
			IO_STATE(R_ATA_CTRL_DATA, multi,    on) |
			IO_STATE(R_ATA_CTRL_DATA, dma_size, word);

		LED_DISK_WRITE(1);

		D(printk("dma write of %d bytes.\n", ata_tot_size));
	}
	return 0;
}

static int e100_dma_write(ide_drive_t *drive)
{
	e100_read_command = 0;
	/* ATAPI-devices (not disks) first call ide_dma_read/write to set the direction
	 * then they call ide_dma_begin after they have issued the appropriate drive command
	 * themselves to actually start the chipset DMA. so we just return here if we're
	 * not a diskdrive.
	 */
	if (drive->media != ide_disk)
                return 0;
	return e100_start_dma(drive, 0, 0);
}

static int e100_dma_read(ide_drive_t *drive)
{
	e100_read_command = 1;
	/* ATAPI-devices (not disks) first call ide_dma_read/write to set the direction
	 * then they call ide_dma_begin after they have issued the appropriate drive command
	 * themselves to actually start the chipset DMA. so we just return here if we're
	 * not a diskdrive.
	 */
	if (drive->media != ide_disk)
                return 0;
	return e100_start_dma(drive, 0, 1);
}

static int e100_dma_begin(ide_drive_t *drive)
{
	/* begin DMA, used by ATAPI devices which want to issue the 
	 * appropriate IDE command themselves.
	 *
	 * they have already called ide_dma_read/write to set the
	 * static reading flag, now they call ide_dma_begin to do
	 * the real stuff. we tell our code below not to issue
	 * any IDE commands itself and jump into it. 
	 */
	 return e100_start_dma(drive, 1, e100_read_command);
}
