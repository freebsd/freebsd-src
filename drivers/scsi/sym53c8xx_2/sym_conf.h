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

#ifndef SYM_CONF_H
#define SYM_CONF_H

/*-------------------------------------------------------------------
 *  Static configuration.
 *-------------------------------------------------------------------
 */

/*
 *  Also support early NCR 810, 815 and 825 chips.
 */
#ifndef SYM_CONF_GENERIC_SUPPORT
#define SYM_CONF_GENERIC_SUPPORT	(1)
#endif

/*
 *  Use Normal IO instead of MMIO.
 */
/* #define SYM_CONF_IOMAPPED */

/*
 *  Max tags for a device (logical unit)
 * 	We use a power of 2, (7) means 2<<7=128
 *  Maximum is 8 -> 256 tags
 */
#ifndef SYM_CONF_MAX_TAG_ORDER
#define SYM_CONF_MAX_TAG_ORDER	(6)
#endif

/*
 *  Max number of scatter/gather entries for en IO.
 *  Each entry costs 8 bytes in the internal CCB data structure.
 */
#ifndef SYM_CONF_MAX_SG
#define SYM_CONF_MAX_SG		(33)
#endif

/*
 *  Max number of targets.
 *  Maximum is 16 and you are advised not to change this value.
 */
#ifndef SYM_CONF_MAX_TARGET
#define SYM_CONF_MAX_TARGET	(16)
#endif

/*
 *  Max number of logical units.
 *  SPI-2 allows up to 64 logical units, but in real life, target
 *  that implements more that 7 logical units are pretty rare.
 *  Anyway, the cost of accepting up to 64 logical unit is low in 
 *  this driver, thus going with the maximum is acceptable.
 */
#ifndef SYM_CONF_MAX_LUN
#define SYM_CONF_MAX_LUN	(64)
#endif

/*
 *  Max number of IO control blocks queued to the controller.
 *  Each entry needs 8 bytes and the queues are allocated contiguously.
 *  Since we donnot want to allocate more than a page, the theorical 
 *  maximum is PAGE_SIZE/8. For safety, we announce a bit less to the 
 *  access method. :)
 *  When not supplied, as it is suggested, the driver compute some 
 *  good value for this parameter.
 */
/* #define SYM_CONF_MAX_START	(PAGE_SIZE/8 - 16) */

/*
 *  Support for NVRAM.
 */
#ifndef SYM_CONF_NVRAM_SUPPORT
#define SYM_CONF_NVRAM_SUPPORT		(1)
#endif

/*
 *  Support for Immediate Arbitration.
 *  Not advised.
 */
/* #define SYM_CONF_IARB_SUPPORT */

/*
 *  Support for some PCI fix-ups (or assumed so).
 */
#define SYM_CONF_PCI_FIX_UP

/*
 *  Number of lists for the optimization of the IO timeout handling.
 *  Not used under FreeBSD and Linux.
 */
#ifndef SYM_CONF_TIMEOUT_ORDER_MAX
#define SYM_CONF_TIMEOUT_ORDER_MAX	(8)
#endif

/*
 *  How the driver handles DMA addressing of user data.
 *  0 :	32 bit addressing
 *  1 :	40 bit addressing
 *  2 :	64 bit addressing using segment registers
 */
#ifndef SYM_CONF_DMA_ADDRESSING_MODE
#define SYM_CONF_DMA_ADDRESSING_MODE	(0)
#endif

/*-------------------------------------------------------------------
 *  Configuration that could be dynamic if it was possible 
 *  to pass arguments to the driver.
 *-------------------------------------------------------------------
 */

/*
 *  HOST default scsi id.
 */
#ifndef SYM_SETUP_HOST_ID
#define SYM_SETUP_HOST_ID	7
#endif

/*
 *  Max synchronous transfers.
 */
#ifndef SYM_SETUP_MIN_SYNC
#define SYM_SETUP_MIN_SYNC	(9)
#endif

/*
 *  Max wide order.
 */
#ifndef SYM_SETUP_MAX_WIDE
#define SYM_SETUP_MAX_WIDE	(1)
#endif

/*
 *  Max SCSI offset.
 */
#ifndef SYM_SETUP_MAX_OFFS
#define SYM_SETUP_MAX_OFFS	(63)
#endif

/*
 *  Default number of tags.
 */
#ifndef SYM_SETUP_MAX_TAG
#define SYM_SETUP_MAX_TAG	(1<<SYM_CONF_MAX_TAG_ORDER)
#endif

/*
 *  SYMBIOS NVRAM format support.
 */
#ifndef SYM_SETUP_SYMBIOS_NVRAM
#define SYM_SETUP_SYMBIOS_NVRAM	(1)
#endif

/*
 *  TEKRAM NVRAM format support.
 */
#ifndef SYM_SETUP_TEKRAM_NVRAM
#define SYM_SETUP_TEKRAM_NVRAM	(1)
#endif

/*
 *  PCI parity checking.
 *  It should not be an option, but some poor or broken 
 *  PCI-HOST bridges have been reported to make problems 
 *  when this feature is enabled.
 *  Setting this option to 0 tells the driver not to 
 *  enable the checking against PCI parity.
 */
#ifndef SYM_SETUP_PCI_PARITY
#define SYM_SETUP_PCI_PARITY	(2)
#endif

/*
 *  SCSI parity checking.
 */
#ifndef SYM_SETUP_SCSI_PARITY
#define SYM_SETUP_SCSI_PARITY	(1)
#endif

/*
 *  SCSI activity LED.
 */
#ifndef SYM_SETUP_SCSI_LED
#define SYM_SETUP_SCSI_LED	(0)
#endif

/*
 *  SCSI High Voltage Differential support.
 *
 *  HVD/LVD/SE capable controllers (895, 895A, 896, 1010) 
 *  report the actual SCSI BUS mode from the STEST4 IO 
 *  register.
 *
 *  But for HVD/SE only capable chips (825a, 875, 885), 
 *  the driver uses some heuristic to probe against HVD. 
 *  Normally, the chip senses the DIFFSENS signal and 
 *  should switch its BUS tranceivers to high impedance 
 *  in situation of the driver having been wrong about 
 *  the actual BUS mode. May-be, the BUS mode probing of 
 *  the driver is safe, but, given that it may be partially 
 *  based on some previous IO register settings, it 
 *  cannot be stated so. Thus, decision has been taken 
 *  to require a user option to be set for the DIFF probing 
 *  to be applied for the 825a, 875 and 885 chips.
 *  
 *  This setup option works as follows:
 *
 *    0  ->  HVD only supported for 895, 895A, 896, 1010.
 *    1  ->  HVD probed  for 825A, 875, 885.
 *    2  ->  HVD assumed for 825A, 875, 885 (not advised).
 */
#ifndef SYM_SETUP_SCSI_DIFF
#define SYM_SETUP_SCSI_DIFF	(0)
#endif

/*
 *  IRQ mode.
 */
#ifndef SYM_SETUP_IRQ_MODE
#define SYM_SETUP_IRQ_MODE	(0)
#endif

/*
 *  Check SCSI BUS signal on reset.
 */
#ifndef SYM_SETUP_SCSI_BUS_CHECK
#define SYM_SETUP_SCSI_BUS_CHECK (1)
#endif

/*
 *  Max burst for PCI (1<<value)
 *  7 means: (1<<7) = 128 DWORDS.
 */
#ifndef SYM_SETUP_BURST_ORDER
#define SYM_SETUP_BURST_ORDER	(7)
#endif

/*
 *  Only relevant if IARB support configured.
 *  - Max number of successive settings of IARB hints.
 *  - Set IARB on arbitration lost.
 */
#define SYM_CONF_IARB_MAX 3
#define SYM_CONF_SET_IARB_ON_ARB_LOST 1

/*
 *  Returning wrong residuals may make problems.
 *  When zero, this define tells the driver to 
 *  always return 0 as transfer residual.
 *  Btw, all my testings of residuals have succeeded.
 */
#define SYM_SETUP_RESIDUAL_SUPPORT 1

/*
 *  Supported maximum number of LUNs to announce to 
 *  the access method.
 *  The driver supports up to 64 LUNs per target as 
 *  required by SPI-2/SPI-3. However some SCSI devices  
 *  designed prior to these specifications or not being  
 *  conformant may be highly confused when they are 
 *  asked about a LUN > 7.
 */
#ifndef SYM_SETUP_MAX_LUN
#define SYM_SETUP_MAX_LUN	(8)
#endif

/*
 *  Bits indicating what kind of fix-ups we want.
 *
 *  Bit 0 (1) : cache line size configuration register.
 *  Bit 1 (2) : MWI bit in command register.
 *  Bit 2 (4) : latency timer if seems too low.
 */

#ifndef SYM_SETUP_PCI_FIX_UP
#define SYM_SETUP_PCI_FIX_UP (3)
#endif

#endif /* SYM_CONF_H */
