/*
 *  Device driver optimized for the Symbios/LSI 53C896/53C895A/53C1010 
 *  PCI-SCSI controllers.
 *
 *  Copyright (C) 1999  Gerard Roudier <groudier@club-internet.fr>
 *
 *  This driver also supports the following Symbios/LSI PCI-SCSI chips:
 *	53C810A, 53C825A, 53C860, 53C875, 53C876, 53C885, 53C895.
 *
 *  but does not support earlier chips as the following ones:
 *	53C810, 53C815, 53C825.
 *  
 *  This driver for FreeBSD-CAM is derived from the Linux sym53c8xx driver.
 *  Copyright (C) 1998-1999  Gerard Roudier
 *
 *  The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 *  a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 *  The original ncr driver has been written for 386bsd and FreeBSD by
 *          Wolfgang Stanglmeier        <wolf@cologne.de>
 *          Stefan Esser                <se@mi.Uni-Koeln.de>
 *  Copyright (C) 1994  Wolfgang Stanglmeier
 *
 *  The initialisation code, and part of the code that addresses 
 *  FreeBSD-CAM services is based on the aic7xxx driver for FreeBSD-CAM 
 *  written by Justin T. Gibbs.
 *
 *  Other major contributions:
 *
 *  NVRAM detection and reading.
 *  Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *  Support for earliest LSI53C1010 boards.
 *  Commercial chips will be fixed, and then the 
 *  corresponding code will get useless.
 */
/* #define	SYMCONF_BROKEN_U3EN_SUPPORT */

/*
 *  Use Normal IO instead of MMIO.
 */
/* #define SYMCONF_IOMAPPED */

/*
 *  Max tags for a device (logical unit)
 * 	We use a power of 2, (7) means 2<<7=128
 *  Maximum is 8 -> 256 tags
 */
#define SYMCONF_MAX_TAG_ORDER	(6)

/*
 *  Max number of scatter/gather entries for en IO.
 *  Each entry costs 8 bytes in the internal CCB data structure.
 *  For now 65 should suffice given the BSD O/Ses capabilities.
 */
#define SYMCONF_MAX_SG		(33)

/*
 *  Max number of targets.
 *  Maximum is 16 and you are advised not to change this value.
 */
#define SYMCONF_MAX_TARGET	(16)

/*
 *  Max number of logical units.
 *  SPI-2 allows up to 64 logical units, but in real life, target
 *  that implements more that 7 logical units are pretty rare.
 *  Anyway, the cost of accepting up to 64 logical unit is low in 
 *  this driver, thus going with the maximum is acceptable.
 */
#define SYMCONF_MAX_LUN		(8)

/*
 *  Max number of IO control blocks queued to the controller.
 *  Each entry needs 8 bytes and the queues are allocated contiguously.
 *  Since we donnot want to allocate more than a page, the theorical 
 *  maximum is PAGE_SIZE/8. For safety, we announce a bit less to the 
 *  access method. :)
 *  When not supplied, as it is suggested, the driver compute some 
 *  good value for this parameter.
 */
/* #define SYMCONF_MAX_START	(PAGE_SIZE/8 - 16) */

/*
 *  Support for NVRAM.
 */
#define SYMCONF_NVRAM_SUPPORT
/* #define SYMCONF_DEBUG_SUPPORT */

/*
 *  Support for Immediate Arbitration.
 *  Not advised.
 */
/* #define SYMCONF_IARB_SUPPORT */

/*
 *  Not needed on FreeBSD, since the system allocator 
 *  does provide naturally aligned addresses.
 */
#define	SYMCONF_USE_INTERNAL_ALLOCATOR

/*-------------------------------------------------------------------
 *  Configuration that could be dynamic if it was possible 
 *  to pass arguments to the driver.
 *-------------------------------------------------------------------
 */

/*
 *  HOST default scsi id.
 */
#define SYMSETUP_HOST_ID	7

/*
 *  Max synchronous transfers.
 */
#define SYMSETUP_MIN_SYNC	(9)

/*
 *  Max wide order.
 */
#define SYMSETUP_MAX_WIDE	(1)

/*
 *  Max SCSI offset.
 */
#define SYMSETUP_MAX_OFFS	(64)
/*
 *  Default number of tags.
 */
#define SYMSETUP_MAX_TAG	(64)

/*
 *  SYMBIOS NVRAM format support.
 */
#define SYMSETUP_SYMBIOS_NVRAM	(1)

/*
 *  TEKRAM NVRAM format support.
 */
#define SYMSETUP_TEKRAM_NVRAM	(1)

/*
 *  PCI parity checking.
 */
#define SYMSETUP_PCI_PARITY	(1)

/*
 *  SCSI parity checking.
 */
#define SYMSETUP_SCSI_PARITY	(1)

/*
 *  SCSI activity LED.
 */
#define SYMSETUP_SCSI_LED	(0)

/*
 *  SCSI differential.
 */
#define SYMSETUP_SCSI_DIFF	(0)

/*
 *  IRQ mode.
 */
#define SYMSETUP_IRQ_MODE	(0)

/*
 *  Check SCSI BUS signal on reset.
 */
#define SYMSETUP_SCSI_BUS_CHECK	(1)

/*
 *  Max burst for PCI (1<<value)
 *  7 means: (1<<7) = 128 DWORDS.
 */
#define SYMSETUP_BURST_ORDER	(7)

/*
 *  Only relevant if IARB support configured.
 *  - Max number of successive settings of IARB hints.
 *  - Set IARB on arbitration lost.
 */
#define SYMCONF_IARB_MAX 3
#define SYMCONF_SET_IARB_ON_ARB_LOST 1

/*
 *  Returning wrong residuals may make problems.
 *  When zero, this define tells the driver to 
 *  always return 0 as transfer residual.
 *  Btw, all my testings of residuals have succeeded.
 */
#define SYMCONF_RESIDUAL_SUPPORT 1

#endif /* SYM_CONF_H */
