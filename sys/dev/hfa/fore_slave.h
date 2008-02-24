/*-
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/dev/hfa/fore_slave.h,v 1.4 2005/01/06 01:42:43 imp Exp $
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Slave Interface definitions
 *
 */

#ifndef _FORE_SLAVE_H
#define _FORE_SLAVE_H

/*
 * This file contains the (mostly hardware) definitions for each of the
 * supported 200-series slave interfaces.
 */


/*
 * Structure defining the supported FORE 200-series interfaces
 */
struct fore_device {
	char		*fd_name;	/* Device name (from PROM) */
	Atm_device	fd_devtyp;	/* Device type */
};
typedef struct fore_device	Fore_device;



/*
 * Common definitions
 * ------------------
 */
#define	MON960_BASE	0x400		/* Address offset of Mon960 */
#define	AALI_BASE	0x4d40		/* Address offset of Aali */

typedef volatile unsigned int	Fore_reg;	/* Slave control register */
typedef volatile unsigned char	Fore_mem;	/* Slave memory */


/*
 * SBA-200E SBus Slave Interface
 * -----------------------------
 */

#define	SBA200E_PROM_NAME	"FORE,sba-200e"

/*
 * SBA-200E Host Control Register (HCR)
 */
#define	SBA200E_READ_BITS	0x1ff	/* Valid read data bits */
#define	SBA200E_WRITE_BITS	0x01f	/* Valid write data bits */
#define	SBA200E_STICKY_BITS	0x013	/* Sticky data bits */

/* Read access */
#define	SBA200E_SBUS_INTR_RD	0x100	/* State of SBus interrupt */
#define	SBA200E_TEST_MODE	0x080	/* Device is in test-mode  */
#define	SBA200E_IFIFO_FULL	0x040	/* Input FIFO almost full (when 0) */
#define	SBA200E_ESP_HOLD_RD	0x020	/* State of ESP bus hold */
#define	SBA200E_SBUS_ENA_RD	0x010	/* State of SBus interrupt enable */
#define	SBA200E_OFIFO_FULL	0x008	/* Output FIFO almost full */
#define	SBA200E_SELFTEST_FAIL	0x004	/* i960 self-test failed (when 0) */
#define	SBA200E_HOLD_LOCK_RD	0x002	/* State of i960 hold lock signal */
#define	SBA200E_RESET_RD	0x001	/* State of board reset signal */

/* Write access - bit set (clear) */
#define	SBA200E_SBUS_ENA	0x010	/* Enable (disable) SBus interrupts */
#define	SBA200E_CLR_SBUS_INTR	0x008	/* Clear SBus interrupt */
#define	SBA200E_I960_INTR	0x004	/* Issue interrupt to i960 */
#define	SBA200E_HOLD_LOCK	0x002	/* Set (clear) i960 hold lock signal */
#define	SBA200E_RESET		0x001	/* Set (clear) board reset signal */

#define	SBA200E_HCR_INIT(hcr,bits) \
			((hcr) = (SBA200E_WRITE_BITS & (bits)))
#define	SBA200E_HCR_SET(hcr,bits) \
			((hcr) = (((hcr) & SBA200E_STICKY_BITS) | (bits)))
#define	SBA200E_HCR_CLR(hcr,bits) \
			((hcr) = ((hcr) & (SBA200E_STICKY_BITS ^ (bits))))



/*
 * SBA-200 SBus Slave Interface
 * ----------------------------
 */

#define	SBA200_PROM_NAME	"FORE,sba-200"

/*
 * SBA-200 Board Control Register (BCR)
 */
/* Write access - bit set */
#define	SBA200_CLR_SBUS_INTR	0x04	/* Clear SBus interrupt */
#define	SBA200_RESET		0x01	/* Assert board reset signal */

/* Write access - bit clear */
#define	SBA200_RESET_CLR	0x00	/* Clear board reset signal */



/*
 * PCA-200E PCI Bus Slave Interface
 * --------------------------------
 */

/*
 * PCI Identifiers
 */
#define	FORE_VENDOR_ID		0x1127

/*
 * PCA-200E PCI Configuration Space
 */
#define	PCA200E_PCI_MEMBASE	0x10	/* Memory base address */
#define	PCA200E_PCI_MCTL	0x40	/* Master control */

/*
 * PCA-200E Address Space
 */
#define	PCA200E_RAM_SIZE	0x100000
#define	PCA200E_HCR_OFFSET	0x100000
#define	PCA200E_IMASK_OFFSET	0x100004
#define	PCA200E_PSR_OFFSET	0x100008
#define	PCA200E_MMAP_SIZE	0x10000c

/*
 * PCA-200E Master Control
 */
#define	PCA200E_MCTL_SWAP	0x4000	/* Convert Slave endianess */

/*
 * PCA-200E Host Control Register (HCR)
 */
#define	PCA200E_READ_BITS	0x0ff	/* Valid read data bits */
#define	PCA200E_WRITE_BITS	0x01f	/* Valid write data bits */
#define	PCA200E_STICKY_BITS	0x000	/* Sticky data bits */

/* Read access */
#define	PCA200E_TEST_MODE	0x080	/* Device is in test-mode */
#define	PCA200E_IFIFO_FULL	0x040	/* Input FIFO almost full */
#define	PCA200E_ESP_HOLD_RD	0x020	/* State of ESP hold bus */
#define	PCA200E_OFIFO_FULL	0x010	/* Output FIFO almost full */
#define	PCA200E_HOLD_ACK	0x008	/* State of Hold Ack */
#define	PCA200E_SELFTEST_FAIL	0x004	/* i960 self-test failed */
#define	PCA200E_HOLD_LOCK_RD	0x002	/* State of i960 hold lock signal */
#define	PCA200E_RESET_BD	0x001	/* State of board reset signal */

/* Write access */
#define	PCA200E_CLR_HBUS_INT	0x010	/* Clear host bus interrupt */
#define	PCA200E_I960_INTRA	0x008	/* Set slave interrupt A */
#define	PCA200E_I960_INTRB	0x004	/* Set slave interrupt B */
#define	PCA200E_HOLD_LOCK	0x002	/* Set (clear) i960 hold lock signal */
#define	PCA200E_RESET		0x001	/* Set (clear) board reset signal */

#define	PCA200E_HCR_INIT(hcr,bits) \
			((hcr) = (PCA200E_WRITE_BITS & (bits)))
#define	PCA200E_HCR_SET(hcr,bits) \
			((hcr) = (bits))
#define	PCA200E_HCR_CLR(hcr,bits) \
			((hcr) = 0)

#endif	/* _FORE_SLAVE_H */
