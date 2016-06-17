/******************************************************************************
**  High Performance device driver for the Symbios 53C896 controller.
**
**  Copyright (C) 1998-2001  Gerard Roudier <groudier@free.fr>
**
**  This driver also supports all the Symbios 53C8XX controller family, 
**  except 53C810 revisions < 16, 53C825 revisions < 16 and all 
**  revisions of 53C815 controllers.
**
**  This driver is based on the Linux port of the FreeBSD ncr driver.
** 
**  Copyright (C) 1994  Wolfgang Stanglmeier
**  
**-----------------------------------------------------------------------------
**  
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
**
**  The Linux port of the FreeBSD ncr driver has been achieved in 
**  november 1995 by:
**
**          Gerard Roudier              <groudier@free.fr>
**
**  Being given that this driver originates from the FreeBSD version, and
**  in order to keep synergy on both, any suggested enhancements and corrections
**  received on Linux are automatically a potential candidate for the FreeBSD 
**  version.
**
**  The original driver has been written for 386bsd and FreeBSD by
**          Wolfgang Stanglmeier        <wolf@cologne.de>
**          Stefan Esser                <se@mi.Uni-Koeln.de>
**
**-----------------------------------------------------------------------------
**
**  Major contributions:
**  --------------------
**
**  NVRAM detection and reading.
**    Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
**
**  Added support for MIPS big endian systems.
**  Carsten Langgaard, carstenl@mips.com
**  Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
**
*******************************************************************************
*/

#ifndef SYM53C8XX_DEFS_H
#define SYM53C8XX_DEFS_H

/*
**	Check supported Linux versions
*/

#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif
#include <linux/config.h>

#define LinuxVersionCode(v, p, s) (((v)<<16)+((p)<<8)+(s))

/*
 * NCR PQS/PDS special device support.
 */
#ifdef CONFIG_SCSI_NCR53C8XX_PQS_PDS
#define SCSI_NCR_PQS_PDS_SUPPORT
#endif

/*
 *	No more an option, enabled by default.
 */
#ifndef CONFIG_SCSI_NCR53C8XX_NVRAM_DETECT
#define CONFIG_SCSI_NCR53C8XX_NVRAM_DETECT
#endif

/*
**	These options are not tunable from 'make config'
*/
#define	SCSI_NCR_PROC_INFO_SUPPORT

/*
**	If you want a driver as small as possible, donnot define the 
**	following options.
*/
#define SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
#define SCSI_NCR_DEBUG_INFO_SUPPORT
#define SCSI_NCR_PCI_FIX_UP_SUPPORT
#ifdef	SCSI_NCR_PROC_INFO_SUPPORT
#	define	SCSI_NCR_USER_COMMAND_SUPPORT
#	define	SCSI_NCR_USER_INFO_SUPPORT
#endif

/*
**	To disable integrity checking, do not define the 
**	following option.
*/
#ifdef	CONFIG_SCSI_NCR53C8XX_INTEGRITY_CHECK
#	define SCSI_NCR_ENABLE_INTEGRITY_CHECK
#endif

/*==========================================================
**
** nvram settings - #define SCSI_NCR_NVRAM_SUPPORT to enable
**
**==========================================================
*/

#ifdef CONFIG_SCSI_NCR53C8XX_NVRAM_DETECT
#define SCSI_NCR_NVRAM_SUPPORT
/* #define SCSI_NCR_DEBUG_NVRAM */
#endif

/* ---------------------------------------------------------------------
** Take into account kernel configured parameters.
** Most of these options can be overridden at startup by a command line.
** ---------------------------------------------------------------------
*/

/*
 * For Ultra2 and Ultra3 SCSI support option, use special features. 
 *
 * Value (default) means:
 *	bit 0 : all features enabled, except:
 *		bit 1 : PCI Write And Invalidate.
 *		bit 2 : Data Phase Mismatch handling from SCRIPTS.
 *
 * Use boot options ncr53c8xx=specf:1 if you want all chip features to be 
 * enabled by the driver.
 */
#define	SCSI_NCR_SETUP_SPECIAL_FEATURES		(3)

#define SCSI_NCR_MAX_SYNC			(80)

/*
 * Allow tags from 2 to 256, default 8
 */
#ifdef	CONFIG_SCSI_NCR53C8XX_MAX_TAGS
#if	CONFIG_SCSI_NCR53C8XX_MAX_TAGS < 2
#define SCSI_NCR_MAX_TAGS	(2)
#elif	CONFIG_SCSI_NCR53C8XX_MAX_TAGS > 256
#define SCSI_NCR_MAX_TAGS	(256)
#else
#define	SCSI_NCR_MAX_TAGS	CONFIG_SCSI_NCR53C8XX_MAX_TAGS
#endif
#else
#define SCSI_NCR_MAX_TAGS	(8)
#endif

/*
 * Allow tagged command queuing support if configured with default number 
 * of tags set to max (see above).
 */
#ifdef	CONFIG_SCSI_NCR53C8XX_DEFAULT_TAGS
#define	SCSI_NCR_SETUP_DEFAULT_TAGS	CONFIG_SCSI_NCR53C8XX_DEFAULT_TAGS
#elif	defined CONFIG_SCSI_NCR53C8XX_TAGGED_QUEUE
#define	SCSI_NCR_SETUP_DEFAULT_TAGS	SCSI_NCR_MAX_TAGS
#else
#define	SCSI_NCR_SETUP_DEFAULT_TAGS	(0)
#endif

/*
 * Use normal IO if configured. Forced for alpha.
 */
#if defined(CONFIG_SCSI_NCR53C8XX_IOMAPPED)
#define	SCSI_NCR_IOMAPPED
#elif defined(__alpha__)
#define	SCSI_NCR_IOMAPPED
#elif defined(__powerpc__)
#if LINUX_VERSION_CODE <= LinuxVersionCode(2,4,3)
#define	SCSI_NCR_IOMAPPED
#define SCSI_NCR_PCI_MEM_NOT_SUPPORTED
#endif
#elif defined(__sparc__)
#undef SCSI_NCR_IOMAPPED
#endif

/*
 * Immediate arbitration
 */
#if defined(CONFIG_SCSI_NCR53C8XX_IARB)
#define SCSI_NCR_IARB_SUPPORT
#endif

/*
 * Sync transfer frequency at startup.
 * Allow from 5Mhz to 80Mhz default 20 Mhz.
 */
#ifndef	CONFIG_SCSI_NCR53C8XX_SYNC
#define	CONFIG_SCSI_NCR53C8XX_SYNC	(20)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC > SCSI_NCR_MAX_SYNC
#undef	CONFIG_SCSI_NCR53C8XX_SYNC
#define	CONFIG_SCSI_NCR53C8XX_SYNC	SCSI_NCR_MAX_SYNC
#endif

#if	CONFIG_SCSI_NCR53C8XX_SYNC == 0
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(255)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 5
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(50)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 20
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(250/(CONFIG_SCSI_NCR53C8XX_SYNC))
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 33
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(11)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 40
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(10)
#else
#define	SCSI_NCR_SETUP_DEFAULT_SYNC 	(9)
#endif

/*
 * Disallow disconnections at boot-up
 */
#ifdef CONFIG_SCSI_NCR53C8XX_NO_DISCONNECT
#define SCSI_NCR_SETUP_DISCONNECTION	(0)
#else
#define SCSI_NCR_SETUP_DISCONNECTION	(1)
#endif

/*
 * Force synchronous negotiation for all targets
 */
#ifdef CONFIG_SCSI_NCR53C8XX_FORCE_SYNC_NEGO
#define SCSI_NCR_SETUP_FORCE_SYNC_NEGO	(1)
#else
#define SCSI_NCR_SETUP_FORCE_SYNC_NEGO	(0)
#endif

/*
 * Disable master parity checking (flawed hardwares need that)
 */
#ifdef CONFIG_SCSI_NCR53C8XX_DISABLE_MPARITY_CHECK
#define SCSI_NCR_SETUP_MASTER_PARITY	(0)
#else
#define SCSI_NCR_SETUP_MASTER_PARITY	(1)
#endif

/*
 * Disable scsi parity checking (flawed devices may need that)
 */
#ifdef CONFIG_SCSI_NCR53C8XX_DISABLE_PARITY_CHECK
#define SCSI_NCR_SETUP_SCSI_PARITY	(0)
#else
#define SCSI_NCR_SETUP_SCSI_PARITY	(1)
#endif

/*
 * Vendor specific stuff
 */
#ifdef CONFIG_SCSI_NCR53C8XX_SYMBIOS_COMPAT
#define SCSI_NCR_SETUP_LED_PIN		(1)
#define SCSI_NCR_SETUP_DIFF_SUPPORT	(4)
#else
#define SCSI_NCR_SETUP_LED_PIN		(0)
#define SCSI_NCR_SETUP_DIFF_SUPPORT	(0)
#endif

/*
 * Settle time after reset at boot-up
 */
#define SCSI_NCR_SETUP_SETTLE_TIME	(2)

/*
**	Bridge quirks work-around option defaulted to 1.
*/
#ifndef	SCSI_NCR_PCIQ_WORK_AROUND_OPT
#define	SCSI_NCR_PCIQ_WORK_AROUND_OPT	1
#endif

/*
**	Work-around common bridge misbehaviour.
**
**	- Do not flush posted writes in the opposite 
**	  direction on read.
**	- May reorder DMA writes to memory.
**
**	This option should not affect performances 
**	significantly, so it is the default.
*/
#if	SCSI_NCR_PCIQ_WORK_AROUND_OPT == 1
#define	SCSI_NCR_PCIQ_MAY_NOT_FLUSH_PW_UPSTREAM
#define	SCSI_NCR_PCIQ_MAY_REORDER_WRITES
#define	SCSI_NCR_PCIQ_MAY_MISS_COMPLETIONS

/*
**	Same as option 1, but also deal with 
**	misconfigured interrupts.
**
**	- Edge triggerred instead of level sensitive.
**	- No interrupt line connected.
**	- IRQ number misconfigured.
**	
**	If no interrupt is delivered, the driver will 
**	catch the interrupt conditions 10 times per 
**	second. No need to say that this option is 
**	not recommended.
*/
#elif	SCSI_NCR_PCIQ_WORK_AROUND_OPT == 2
#define	SCSI_NCR_PCIQ_MAY_NOT_FLUSH_PW_UPSTREAM
#define	SCSI_NCR_PCIQ_MAY_REORDER_WRITES
#define	SCSI_NCR_PCIQ_MAY_MISS_COMPLETIONS
#define	SCSI_NCR_PCIQ_BROKEN_INTR

/*
**	Some bridge designers decided to flush 
**	everything prior to deliver the interrupt.
**	This option tries to deal with such a 
**	behaviour.
*/
#elif	SCSI_NCR_PCIQ_WORK_AROUND_OPT == 3
#define	SCSI_NCR_PCIQ_SYNC_ON_INTR
#endif

/*
**	Other parameters not configurable with "make config"
**	Avoid to change these constants, unless you know what you are doing.
*/

#define SCSI_NCR_ALWAYS_SIMPLE_TAG
#define SCSI_NCR_MAX_SCATTER	(127)
#define SCSI_NCR_MAX_TARGET	(16)

/*
**   Compute some desirable value for CAN_QUEUE 
**   and CMD_PER_LUN.
**   The driver will use lower values if these 
**   ones appear to be too large.
*/
#define SCSI_NCR_CAN_QUEUE	(8*SCSI_NCR_MAX_TAGS + 2*SCSI_NCR_MAX_TARGET)
#define SCSI_NCR_CMD_PER_LUN	(SCSI_NCR_MAX_TAGS)

#define SCSI_NCR_SG_TABLESIZE	(SCSI_NCR_MAX_SCATTER)
#define SCSI_NCR_TIMER_INTERVAL	(HZ)

#if 1 /* defined CONFIG_SCSI_MULTI_LUN */
#define SCSI_NCR_MAX_LUN	(16)
#else
#define SCSI_NCR_MAX_LUN	(1)
#endif

#ifndef HOSTS_C

/*
**	These simple macros limit expression involving 
**	kernel time values (jiffies) to some that have 
**	chance not to be too much incorrect. :-)
*/
#define ktime_get(o)		(jiffies + (u_long) o)
#define ktime_exp(b)		((long)(jiffies) - (long)(b) >= 0)
#define ktime_dif(a, b)		((long)(a) - (long)(b))
/* These ones are not used in this driver */
#define ktime_add(a, o)		((a) + (u_long)(o))
#define ktime_sub(a, o)		((a) - (u_long)(o))


/*
 *  IO functions definition for big/little endian CPU support.
 *  For now, the NCR is only supported in little endian addressing mode, 
 */

#ifdef	__BIG_ENDIAN

#if	LINUX_VERSION_CODE < LinuxVersionCode(2,1,0)
#error	"BIG ENDIAN byte ordering needs kernel version >= 2.1.0"
#endif

#define	inw_l2b		inw
#define	inl_l2b		inl
#define	outw_b2l	outw
#define	outl_b2l	outl

#define	readb_raw	readb
#define	writeb_raw	writeb

#if defined(__hppa__)
#define	readw_l2b(a)	le16_to_cpu(readw(a))
#define	readl_l2b(a)	le32_to_cpu(readl(a))
#define	writew_b2l(v,a)	writew(cpu_to_le16(v),a)
#define	writel_b2l(v,a)	writel(cpu_to_le32(v),a)
#elif defined(__mips__)
#define readw_l2b	readw
#define readl_l2b	readl
#define writew_b2l	writew
#define writel_b2l	writel
#define inw_l2b 	inw
#define inl_l2b 	inl
#define outw_b2l	outw
#define outl_b2l	outl
#else	/* Other big-endian */
#define	readw_l2b	readw
#define	readl_l2b	readl
#define	writew_b2l	writew
#define	writel_b2l	writel
#endif

#else	/* little endian */

#define	inw_raw		inw
#define	inl_raw		inl
#define	outw_raw	outw
#define	outl_raw	outl

#if defined(__i386__)	/* i386 implements full FLAT memory/MMIO model */
#define readb_raw(a)	(*(volatile unsigned char *) (a))
#define readw_raw(a)	(*(volatile unsigned short *) (a))
#define readl_raw(a)	(*(volatile unsigned int *) (a))
#define writeb_raw(b,a)	((*(volatile unsigned char *) (a)) = (b))
#define writew_raw(b,a)	((*(volatile unsigned short *) (a)) = (b))
#define writel_raw(b,a)	((*(volatile unsigned int *) (a)) = (b))

#else	/* Other little-endian */
#define	readb_raw	readb
#define	readw_raw	readw
#define	readl_raw	readl
#define	writeb_raw	writeb
#define	writew_raw	writew
#define	writel_raw	writel

#endif
#endif

#ifdef	SCSI_NCR_BIG_ENDIAN
#error	"The NCR in BIG ENDIAN addressing mode is not (yet) supported"
#endif


/*
 *  IA32 architecture does not reorder STORES and prevents
 *  LOADS from passing STORES. It is called `program order' 
 *  by Intel and allows device drivers to deal with memory 
 *  ordering by only ensuring that the code is not reordered  
 *  by the compiler when ordering is required.
 *  Other architectures implement a weaker ordering that 
 *  requires memory barriers (and also IO barriers when they 
 *  make sense) to be used.
 *  We want to be paranoid for ppc and ia64. :)
 */

#if	defined(__i386__) || defined(__x86_64__)
#define MEMORY_BARRIER()	do { ; } while(0)
#elif	defined	__powerpc__
#define MEMORY_BARRIER()	__asm__ volatile("eieio; sync" : : : "memory")
#elif	defined	__ia64__
#define MEMORY_BARRIER()	__asm__ volatile("mf.a; mf" : : : "memory")
#else
#define MEMORY_BARRIER()	mb()
#endif


/*
 *  If the NCR uses big endian addressing mode over the 
 *  PCI, actual io register addresses for byte and word 
 *  accesses must be changed according to lane routing.
 *  Btw, ncr_offb() and ncr_offw() macros only apply to 
 *  constants and so donnot generate bloated code.
 */

#if	defined(SCSI_NCR_BIG_ENDIAN)

#define ncr_offb(o)	(((o)&~3)+((~((o)&3))&3))
#define ncr_offw(o)	(((o)&~3)+((~((o)&3))&2))

#else

#define ncr_offb(o)	(o)
#define ncr_offw(o)	(o)

#endif

/*
 *  If the CPU and the NCR use same endian-ness adressing,
 *  no byte reordering is needed for script patching.
 *  Macro cpu_to_scr() is to be used for script patching.
 *  Macro scr_to_cpu() is to be used for getting a DWORD 
 *  from the script.
 */

#if	defined(__BIG_ENDIAN) && !defined(SCSI_NCR_BIG_ENDIAN)

#define cpu_to_scr(dw)	cpu_to_le32(dw)
#define scr_to_cpu(dw)	le32_to_cpu(dw)

#elif	defined(__LITTLE_ENDIAN) && defined(SCSI_NCR_BIG_ENDIAN)

#define cpu_to_scr(dw)	cpu_to_be32(dw)
#define scr_to_cpu(dw)	be32_to_cpu(dw)

#else

#define cpu_to_scr(dw)	(dw)
#define scr_to_cpu(dw)	(dw)

#endif

/*
 *  Access to the controller chip.
 *
 *  If SCSI_NCR_IOMAPPED is defined, the driver will use 
 *  normal IOs instead of the MEMORY MAPPED IO method  
 *  recommended by PCI specifications.
 *  If all PCI bridges, host brigdes and architectures 
 *  would have been correctly designed for PCI, this 
 *  option would be useless.
 *
 *  If the CPU and the NCR use same endian-ness adressing,
 *  no byte reordering is needed for accessing chip io 
 *  registers. Functions suffixed by '_raw' are assumed 
 *  to access the chip over the PCI without doing byte 
 *  reordering. Functions suffixed by '_l2b' are 
 *  assumed to perform little-endian to big-endian byte 
 *  reordering, those suffixed by '_b2l' blah, blah,
 *  blah, ...
 */

#if defined(SCSI_NCR_IOMAPPED)

/*
 *  IO mapped only input / ouput
 */

#define	INB_OFF(o)		inb (np->base_io + ncr_offb(o))
#define	OUTB_OFF(o, val)	outb ((val), np->base_io + ncr_offb(o))

#if	defined(__BIG_ENDIAN) && !defined(SCSI_NCR_BIG_ENDIAN)

#define	INW_OFF(o)		inw_l2b (np->base_io + ncr_offw(o))
#define	INL_OFF(o)		inl_l2b (np->base_io + (o))

#define	OUTW_OFF(o, val)	outw_b2l ((val), np->base_io + ncr_offw(o))
#define	OUTL_OFF(o, val)	outl_b2l ((val), np->base_io + (o))

#elif	defined(__LITTLE_ENDIAN) && defined(SCSI_NCR_BIG_ENDIAN)

#define	INW_OFF(o)		inw_b2l (np->base_io + ncr_offw(o))
#define	INL_OFF(o)		inl_b2l (np->base_io + (o))

#define	OUTW_OFF(o, val)	outw_l2b ((val), np->base_io + ncr_offw(o))
#define	OUTL_OFF(o, val)	outl_l2b ((val), np->base_io + (o))

#else

#define	INW_OFF(o)		inw_raw (np->base_io + ncr_offw(o))
#define	INL_OFF(o)		inl_raw (np->base_io + (o))

#define	OUTW_OFF(o, val)	outw_raw ((val), np->base_io + ncr_offw(o))
#define	OUTL_OFF(o, val)	outl_raw ((val), np->base_io + (o))

#endif	/* ENDIANs */

#else	/* defined SCSI_NCR_IOMAPPED */

/*
 *  MEMORY mapped IO input / output
 */

#define INB_OFF(o)		readb_raw((char *)np->reg + ncr_offb(o))
#define OUTB_OFF(o, val)	writeb_raw((val), (char *)np->reg + ncr_offb(o))

#if	defined(__BIG_ENDIAN) && !defined(SCSI_NCR_BIG_ENDIAN)

#define INW_OFF(o)		readw_l2b((char *)np->reg + ncr_offw(o))
#define INL_OFF(o)		readl_l2b((char *)np->reg + (o))

#define OUTW_OFF(o, val)	writew_b2l((val), (char *)np->reg + ncr_offw(o))
#define OUTL_OFF(o, val)	writel_b2l((val), (char *)np->reg + (o))

#elif	defined(__LITTLE_ENDIAN) && defined(SCSI_NCR_BIG_ENDIAN)

#define INW_OFF(o)		readw_b2l((char *)np->reg + ncr_offw(o))
#define INL_OFF(o)		readl_b2l((char *)np->reg + (o))

#define OUTW_OFF(o, val)	writew_l2b((val), (char *)np->reg + ncr_offw(o))
#define OUTL_OFF(o, val)	writel_l2b((val), (char *)np->reg + (o))

#else

#define INW_OFF(o)		readw_raw((char *)np->reg + ncr_offw(o))
#define INL_OFF(o)		readl_raw((char *)np->reg + (o))

#define OUTW_OFF(o, val)	writew_raw((val), (char *)np->reg + ncr_offw(o))
#define OUTL_OFF(o, val)	writel_raw((val), (char *)np->reg + (o))

#endif

#endif	/* defined SCSI_NCR_IOMAPPED */

#define INB(r)		INB_OFF (offsetof(struct ncr_reg,r))
#define INW(r)		INW_OFF (offsetof(struct ncr_reg,r))
#define INL(r)		INL_OFF (offsetof(struct ncr_reg,r))

#define OUTB(r, val)	OUTB_OFF (offsetof(struct ncr_reg,r), (val))
#define OUTW(r, val)	OUTW_OFF (offsetof(struct ncr_reg,r), (val))
#define OUTL(r, val)	OUTL_OFF (offsetof(struct ncr_reg,r), (val))

/*
 *  Set bit field ON, OFF 
 */

#define OUTONB(r, m)	OUTB(r, INB(r) | (m))
#define OUTOFFB(r, m)	OUTB(r, INB(r) & ~(m))
#define OUTONW(r, m)	OUTW(r, INW(r) | (m))
#define OUTOFFW(r, m)	OUTW(r, INW(r) & ~(m))
#define OUTONL(r, m)	OUTL(r, INL(r) | (m))
#define OUTOFFL(r, m)	OUTL(r, INL(r) & ~(m))

/*
 *  We normally want the chip to have a consistent view
 *  of driver internal data structures when we restart it.
 *  Thus these macros.
 */
#define OUTL_DSP(v)				\
	do {					\
		MEMORY_BARRIER();		\
		OUTL (nc_dsp, (v));		\
	} while (0)

#define OUTONB_STD()				\
	do {					\
		MEMORY_BARRIER();		\
		OUTONB (nc_dcntl, (STD|NOCOM));	\
	} while (0)


/*
**	NCR53C8XX Device Ids
*/

#ifndef PCI_DEVICE_ID_NCR_53C810
#define PCI_DEVICE_ID_NCR_53C810 1
#endif

#ifndef PCI_DEVICE_ID_NCR_53C810AP
#define PCI_DEVICE_ID_NCR_53C810AP 5
#endif

#ifndef PCI_DEVICE_ID_NCR_53C815
#define PCI_DEVICE_ID_NCR_53C815 4
#endif

#ifndef PCI_DEVICE_ID_NCR_53C820
#define PCI_DEVICE_ID_NCR_53C820 2
#endif

#ifndef PCI_DEVICE_ID_NCR_53C825
#define PCI_DEVICE_ID_NCR_53C825 3
#endif

#ifndef PCI_DEVICE_ID_NCR_53C860
#define PCI_DEVICE_ID_NCR_53C860 6
#endif

#ifndef PCI_DEVICE_ID_NCR_53C875
#define PCI_DEVICE_ID_NCR_53C875 0xf
#endif

#ifndef PCI_DEVICE_ID_NCR_53C875J
#define PCI_DEVICE_ID_NCR_53C875J 0x8f
#endif

#ifndef PCI_DEVICE_ID_NCR_53C885
#define PCI_DEVICE_ID_NCR_53C885 0xd
#endif

#ifndef PCI_DEVICE_ID_NCR_53C895
#define PCI_DEVICE_ID_NCR_53C895 0xc
#endif

#ifndef PCI_DEVICE_ID_NCR_53C896
#define PCI_DEVICE_ID_NCR_53C896 0xb
#endif

#ifndef PCI_DEVICE_ID_NCR_53C895A
#define PCI_DEVICE_ID_NCR_53C895A 0x12
#endif

#ifndef PCI_DEVICE_ID_NCR_53C875A
#define PCI_DEVICE_ID_NCR_53C875A 0x13
#endif

#ifndef PCI_DEVICE_ID_NCR_53C1510D
#define PCI_DEVICE_ID_NCR_53C1510D 0xa
#endif

#ifndef PCI_DEVICE_ID_LSI_53C1010
#define PCI_DEVICE_ID_LSI_53C1010 0x20
#endif

#ifndef PCI_DEVICE_ID_LSI_53C1010_66
#define PCI_DEVICE_ID_LSI_53C1010_66 0x21
#endif


/*
**   NCR53C8XX devices features table.
*/
typedef struct {
	unsigned short	device_id;
	unsigned short	revision_id;
	char	*name;
	unsigned char	burst_max;	/* log-base-2 of max burst */
	unsigned char	offset_max;
	unsigned char	nr_divisor;
	unsigned int	features;
#define FE_LED0		(1<<0)
#define FE_WIDE		(1<<1)    /* Wide data transfers */
#define FE_ULTRA	(1<<2)	  /* Ultra speed 20Mtrans/sec */
#define FE_ULTRA2	(1<<3)	  /* Ultra 2 - 40 Mtrans/sec */
#define FE_DBLR		(1<<4)	  /* Clock doubler present */
#define FE_QUAD		(1<<5)	  /* Clock quadrupler present */
#define FE_ERL		(1<<6)    /* Enable read line */
#define FE_CLSE		(1<<7)    /* Cache line size enable */
#define FE_WRIE		(1<<8)    /* Write & Invalidate enable */
#define FE_ERMP		(1<<9)    /* Enable read multiple */
#define FE_BOF		(1<<10)   /* Burst opcode fetch */
#define FE_DFS		(1<<11)   /* DMA fifo size */
#define FE_PFEN		(1<<12)   /* Prefetch enable */
#define FE_LDSTR	(1<<13)   /* Load/Store supported */
#define FE_RAM		(1<<14)   /* On chip RAM present */
#define FE_VARCLK	(1<<15)   /* SCSI clock may vary */
#define FE_RAM8K	(1<<16)   /* On chip RAM sized 8Kb */
#define FE_64BIT	(1<<17)   /* Have a 64-bit PCI interface */
#define FE_IO256	(1<<18)   /* Requires full 256 bytes in PCI space */
#define FE_NOPM		(1<<19)   /* Scripts handles phase mismatch */
#define FE_LEDC		(1<<20)   /* Hardware control of LED */
#define FE_DIFF		(1<<21)   /* Support Differential SCSI */
#define FE_ULTRA3	(1<<22)   /* Ultra-3 80Mtrans/sec */
#define FE_66MHZ 	(1<<23)   /* 66MHz PCI Support */
#define FE_DAC	 	(1<<24)   /* Support DAC cycles (64 bit addressing) */
#define FE_ISTAT1 	(1<<25)   /* Have ISTAT1, MBOX0, MBOX1 registers */
#define FE_DAC_IN_USE	(1<<26)	  /* Platform does DAC cycles */

#define FE_CACHE_SET	(FE_ERL|FE_CLSE|FE_WRIE|FE_ERMP)
#define FE_SCSI_SET	(FE_WIDE|FE_ULTRA|FE_ULTRA2|FE_DBLR|FE_QUAD|F_CLK80)
#define FE_SPECIAL_SET	(FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM)
} ncr_chip;

/*
**	DEL 397 - 53C875 Rev 3 - Part Number 609-0392410 - ITEM 3.
**	Memory Read transaction terminated by a retry followed by 
**	Memory Read Line command.
*/
#define FE_CACHE0_SET	(FE_CACHE_SET & ~FE_ERL)

/*
**	DEL 397 - 53C875 Rev 3 - Part Number 609-0392410 - ITEM 5.
**	On paper, this errata is harmless. But it is a good reason for 
**	using a shorter programmed burst length (64 DWORDS instead of 128).
*/

#define SCSI_NCR_CHIP_TABLE						\
{									\
 {PCI_DEVICE_ID_NCR_53C810, 0x0f, "810",  4,  8, 4,			\
 FE_ERL}								\
 ,									\
 {PCI_DEVICE_ID_NCR_53C810, 0xff, "810a", 4,  8, 4,			\
 FE_CACHE_SET|FE_LDSTR|FE_PFEN|FE_BOF}					\
 ,									\
 {PCI_DEVICE_ID_NCR_53C815, 0xff, "815",  4,  8, 4,			\
 FE_ERL|FE_BOF}								\
 ,									\
 {PCI_DEVICE_ID_NCR_53C820, 0xff, "820",  4,  8, 4,			\
 FE_WIDE|FE_ERL}							\
 ,									\
 {PCI_DEVICE_ID_NCR_53C825, 0x0f, "825",  4,  8, 4,			\
 FE_WIDE|FE_ERL|FE_BOF|FE_DIFF}						\
 ,									\
 {PCI_DEVICE_ID_NCR_53C825, 0xff, "825a", 6,  8, 4,			\
 FE_WIDE|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM|FE_DIFF}	\
 ,									\
 {PCI_DEVICE_ID_NCR_53C860, 0xff, "860",  4,  8, 5,			\
 FE_ULTRA|FE_CACHE_SET|FE_BOF|FE_LDSTR|FE_PFEN}				\
 ,									\
 {PCI_DEVICE_ID_NCR_53C875, 0x01, "875",  6, 16, 5,			\
 FE_WIDE|FE_ULTRA|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|		\
 FE_RAM|FE_DIFF|FE_VARCLK}						\
 ,									\
 {PCI_DEVICE_ID_NCR_53C875, 0xff, "875",  6, 16, 5,			\
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM|FE_DIFF|FE_VARCLK}						\
 ,									\
 {PCI_DEVICE_ID_NCR_53C875J,0xff, "875J", 6, 16, 5,			\
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM|FE_VARCLK}							\
 ,									\
 {PCI_DEVICE_ID_NCR_53C885, 0xff, "885",  6, 16, 5,			\
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM|FE_DIFF|FE_VARCLK}						\
 ,									\
 {PCI_DEVICE_ID_NCR_53C895, 0xff, "895",  6, 31, 7,			\
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM}								\
 ,									\
 {PCI_DEVICE_ID_NCR_53C896, 0xff, "896",  6, 31, 7,			\
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_ISTAT1}	\
 ,									\
 {PCI_DEVICE_ID_NCR_53C895A, 0xff, "895a",  6, 31, 7,			\
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM|FE_RAM8K|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC}			\
 ,									\
 {PCI_DEVICE_ID_NCR_53C875A, 0xff, "875a",  6, 31, 7,			\
 FE_WIDE|FE_ULTRA|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC}				\
 ,									\
 {PCI_DEVICE_ID_NCR_53C1510D, 0xff, "1510D",  7, 31, 7,			\
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|	\
 FE_RAM|FE_IO256}							\
 ,									\
 {PCI_DEVICE_ID_LSI_53C1010, 0xff, "1010-33",  6, 62, 7,		\
 FE_WIDE|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_ISTAT1|	\
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_ULTRA3}	\
 ,									\
 {PCI_DEVICE_ID_LSI_53C1010_66, 0xff, "1010-66",  6, 62, 7,		\
 FE_WIDE|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_ISTAT1|	\
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_ULTRA3|	\
 FE_66MHZ}								\
}

/*
 * List of supported NCR chip ids
 */
#define SCSI_NCR_CHIP_IDS		\
{					\
	PCI_DEVICE_ID_NCR_53C810,	\
	PCI_DEVICE_ID_NCR_53C815,	\
	PCI_DEVICE_ID_NCR_53C820,	\
	PCI_DEVICE_ID_NCR_53C825,	\
	PCI_DEVICE_ID_NCR_53C860,	\
	PCI_DEVICE_ID_NCR_53C875,	\
	PCI_DEVICE_ID_NCR_53C875J,	\
	PCI_DEVICE_ID_NCR_53C885,	\
	PCI_DEVICE_ID_NCR_53C895,	\
	PCI_DEVICE_ID_NCR_53C896,	\
	PCI_DEVICE_ID_NCR_53C895A,	\
	PCI_DEVICE_ID_NCR_53C1510D,	\
 	PCI_DEVICE_ID_LSI_53C1010,	\
 	PCI_DEVICE_ID_LSI_53C1010_66	\
}

/*
**	Driver setup structure.
**
**	This structure is initialized from linux config options.
**	It can be overridden at boot-up by the boot command line.
*/
#define SCSI_NCR_MAX_EXCLUDES 8
struct ncr_driver_setup {
	u_char	master_parity;
	u_char	scsi_parity;
	u_char	disconnection;
	u_char	special_features;
	u_char	force_sync_nego;
	u_char	reverse_probe;
	u_char	pci_fix_up;
	u_char	use_nvram;
	u_char	verbose;
	u_char	default_tags;
	u_short	default_sync;
	u_short	debug;
	u_char	burst_max;
	u_char	led_pin;
	u_char	max_wide;
	u_char	settle_delay;
	u_char	diff_support;
	u_char	irqm;
	u_char	bus_check;
	u_char	optimize;
	u_char	recovery;
	u_char	host_id;
	u_short	iarb;
	u_long	excludes[SCSI_NCR_MAX_EXCLUDES];
	char	tag_ctrl[100];
};

/*
**	Initial setup.
**	Can be overriden at startup by a command line.
*/
#define SCSI_NCR_DRIVER_SETUP			\
{						\
	SCSI_NCR_SETUP_MASTER_PARITY,		\
	SCSI_NCR_SETUP_SCSI_PARITY,		\
	SCSI_NCR_SETUP_DISCONNECTION,		\
	SCSI_NCR_SETUP_SPECIAL_FEATURES,	\
	SCSI_NCR_SETUP_FORCE_SYNC_NEGO,		\
	0,					\
	0,					\
	1,					\
	0,					\
	SCSI_NCR_SETUP_DEFAULT_TAGS,		\
	SCSI_NCR_SETUP_DEFAULT_SYNC,		\
	0x00,					\
	7,					\
	SCSI_NCR_SETUP_LED_PIN,			\
	1,					\
	SCSI_NCR_SETUP_SETTLE_TIME,		\
	SCSI_NCR_SETUP_DIFF_SUPPORT,		\
	0,					\
	1,					\
	0,					\
	0,					\
	255,					\
	0x00					\
}

/*
**	Boot fail safe setup.
**	Override initial setup from boot command line:
**	ncr53c8xx=safe:y
*/
#define SCSI_NCR_DRIVER_SAFE_SETUP		\
{						\
	0,					\
	1,					\
	0,					\
	0,					\
	0,					\
	0,					\
	0,					\
	1,					\
	2,					\
	0,					\
	255,					\
	0x00,					\
	255,					\
	0,					\
	0,					\
	10,					\
	1,					\
	1,					\
	1,					\
	0,					\
	0,					\
	255					\
}

#ifdef SCSI_NCR_NVRAM_SUPPORT
/*
**	Symbios NvRAM data format
*/
#define SYMBIOS_NVRAM_SIZE 368
#define SYMBIOS_NVRAM_ADDRESS 0x100

struct Symbios_nvram {
/* Header 6 bytes */
	u_short type;		/* 0x0000 */
	u_short byte_count;	/* excluding header/trailer */
	u_short checksum;

/* Controller set up 20 bytes */
	u_char	v_major;	/* 0x00 */
	u_char	v_minor;	/* 0x30 */
	u_int32	boot_crc;
	u_short	flags;
#define SYMBIOS_SCAM_ENABLE	(1)
#define SYMBIOS_PARITY_ENABLE	(1<<1)
#define SYMBIOS_VERBOSE_MSGS	(1<<2)
#define SYMBIOS_CHS_MAPPING	(1<<3)
#define SYMBIOS_NO_NVRAM	(1<<3)	/* ??? */
	u_short	flags1;
#define SYMBIOS_SCAN_HI_LO	(1)
	u_short	term_state;
#define SYMBIOS_TERM_CANT_PROGRAM	(0)
#define SYMBIOS_TERM_ENABLED		(1)
#define SYMBIOS_TERM_DISABLED		(2)
	u_short	rmvbl_flags;
#define SYMBIOS_RMVBL_NO_SUPPORT	(0)
#define SYMBIOS_RMVBL_BOOT_DEVICE	(1)
#define SYMBIOS_RMVBL_MEDIA_INSTALLED	(2)
	u_char	host_id;
	u_char	num_hba;	/* 0x04 */
	u_char	num_devices;	/* 0x10 */
	u_char	max_scam_devices;	/* 0x04 */
	u_char	num_valid_scam_devives;	/* 0x00 */
	u_char	rsvd;

/* Boot order 14 bytes * 4 */
	struct Symbios_host{
		u_short	type;		/* 4:8xx / 0:nok */
		u_short	device_id;	/* PCI device id */
		u_short	vendor_id;	/* PCI vendor id */
		u_char	bus_nr;		/* PCI bus number */
		u_char	device_fn;	/* PCI device/function number << 3*/
		u_short	word8;
		u_short	flags;
#define	SYMBIOS_INIT_SCAN_AT_BOOT	(1)
		u_short	io_port;	/* PCI io_port address */
	} host[4];

/* Targets 8 bytes * 16 */
	struct Symbios_target {
		u_char	flags;
#define SYMBIOS_DISCONNECT_ENABLE	(1)
#define SYMBIOS_SCAN_AT_BOOT_TIME	(1<<1)
#define SYMBIOS_SCAN_LUNS		(1<<2)
#define SYMBIOS_QUEUE_TAGS_ENABLED	(1<<3)
		u_char	rsvd;
		u_char	bus_width;	/* 0x08/0x10 */
		u_char	sync_offset;
		u_short	sync_period;	/* 4*period factor */
		u_short	timeout;
	} target[16];
/* Scam table 8 bytes * 4 */
	struct Symbios_scam {
		u_short	id;
		u_short	method;
#define SYMBIOS_SCAM_DEFAULT_METHOD	(0)
#define SYMBIOS_SCAM_DONT_ASSIGN	(1)
#define SYMBIOS_SCAM_SET_SPECIFIC_ID	(2)
#define SYMBIOS_SCAM_USE_ORDER_GIVEN	(3)
		u_short status;
#define SYMBIOS_SCAM_UNKNOWN		(0)
#define SYMBIOS_SCAM_DEVICE_NOT_FOUND	(1)
#define SYMBIOS_SCAM_ID_NOT_SET		(2)
#define SYMBIOS_SCAM_ID_VALID		(3)
		u_char	target_id;
		u_char	rsvd;
	} scam[4];

	u_char	spare_devices[15*8];
	u_char	trailer[6];		/* 0xfe 0xfe 0x00 0x00 0x00 0x00 */
};
typedef struct Symbios_nvram	Symbios_nvram;
typedef struct Symbios_host	Symbios_host;
typedef struct Symbios_target	Symbios_target;
typedef struct Symbios_scam	Symbios_scam;

/*
**	Tekram NvRAM data format.
*/
#define TEKRAM_NVRAM_SIZE 64
#define TEKRAM_93C46_NVRAM_ADDRESS 0
#define TEKRAM_24C16_NVRAM_ADDRESS 0x40

struct Tekram_nvram {
	struct Tekram_target {
		u_char	flags;
#define	TEKRAM_PARITY_CHECK		(1)
#define TEKRAM_SYNC_NEGO		(1<<1)
#define TEKRAM_DISCONNECT_ENABLE	(1<<2)
#define	TEKRAM_START_CMD		(1<<3)
#define TEKRAM_TAGGED_COMMANDS		(1<<4)
#define TEKRAM_WIDE_NEGO		(1<<5)
		u_char	sync_index;
		u_short	word2;
	} target[16];
	u_char	host_id;
	u_char	flags;
#define TEKRAM_MORE_THAN_2_DRIVES	(1)
#define TEKRAM_DRIVES_SUP_1GB		(1<<1)
#define	TEKRAM_RESET_ON_POWER_ON	(1<<2)
#define TEKRAM_ACTIVE_NEGATION		(1<<3)
#define TEKRAM_IMMEDIATE_SEEK		(1<<4)
#define	TEKRAM_SCAN_LUNS		(1<<5)
#define	TEKRAM_REMOVABLE_FLAGS		(3<<6)	/* 0: disable; 1: boot device; 2:all */
	u_char	boot_delay_index;
	u_char	max_tags_index;
	u_short	flags1;
#define TEKRAM_F2_F6_ENABLED		(1)
	u_short	spare[29];
};
typedef struct Tekram_nvram	Tekram_nvram;
typedef struct Tekram_target	Tekram_target;

#endif /* SCSI_NCR_NVRAM_SUPPORT */

/**************** ORIGINAL CONTENT of ncrreg.h from FreeBSD ******************/

/*-----------------------------------------------------------------
**
**	The ncr 53c810 register structure.
**
**-----------------------------------------------------------------
*/

struct ncr_reg {
/*00*/  u_char    nc_scntl0;    /* full arb., ena parity, par->ATN  */

/*01*/  u_char    nc_scntl1;    /* no reset                         */
        #define   ISCON   0x10  /* connected to scsi		    */
        #define   CRST    0x08  /* force reset                      */
        #define   IARB    0x02  /* immediate arbitration            */

/*02*/  u_char    nc_scntl2;    /* no disconnect expected           */
	#define   SDU     0x80  /* cmd: disconnect will raise error */
	#define   CHM     0x40  /* sta: chained mode                */
	#define   WSS     0x08  /* sta: wide scsi send           [W]*/
	#define   WSR     0x01  /* sta: wide scsi received       [W]*/

/*03*/  u_char    nc_scntl3;    /* cnf system clock dependent       */
	#define   EWS     0x08  /* cmd: enable wide scsi         [W]*/
	#define   ULTRA   0x80  /* cmd: ULTRA enable                */
				/* bits 0-2, 7 rsvd for C1010       */

/*04*/  u_char    nc_scid;	/* cnf host adapter scsi address    */
	#define   RRE     0x40  /* r/w:e enable response to resel.  */
	#define   SRE     0x20  /* r/w:e enable response to select  */

/*05*/  u_char    nc_sxfer;	/* ### Sync speed and count         */
				/* bits 6-7 rsvd for C1010          */

/*06*/  u_char    nc_sdid;	/* ### Destination-ID               */

/*07*/  u_char    nc_gpreg;	/* ??? IO-Pins                      */

/*08*/  u_char    nc_sfbr;	/* ### First byte in phase          */

/*09*/  u_char    nc_socl;
	#define   CREQ	  0x80	/* r/w: SCSI-REQ                    */
	#define   CACK	  0x40	/* r/w: SCSI-ACK                    */
	#define   CBSY	  0x20	/* r/w: SCSI-BSY                    */
	#define   CSEL	  0x10	/* r/w: SCSI-SEL                    */
	#define   CATN	  0x08	/* r/w: SCSI-ATN                    */
	#define   CMSG	  0x04	/* r/w: SCSI-MSG                    */
	#define   CC_D	  0x02	/* r/w: SCSI-C_D                    */
	#define   CI_O	  0x01	/* r/w: SCSI-I_O                    */

/*0a*/  u_char    nc_ssid;

/*0b*/  u_char    nc_sbcl;

/*0c*/  u_char    nc_dstat;
        #define   DFE     0x80  /* sta: dma fifo empty              */
        #define   MDPE    0x40  /* int: master data parity error    */
        #define   BF      0x20  /* int: script: bus fault           */
        #define   ABRT    0x10  /* int: script: command aborted     */
        #define   SSI     0x08  /* int: script: single step         */
        #define   SIR     0x04  /* int: script: interrupt instruct. */
        #define   IID     0x01  /* int: script: illegal instruct.   */

/*0d*/  u_char    nc_sstat0;
        #define   ILF     0x80  /* sta: data in SIDL register lsb   */
        #define   ORF     0x40  /* sta: data in SODR register lsb   */
        #define   OLF     0x20  /* sta: data in SODL register lsb   */
        #define   AIP     0x10  /* sta: arbitration in progress     */
        #define   LOA     0x08  /* sta: arbitration lost            */
        #define   WOA     0x04  /* sta: arbitration won             */
        #define   IRST    0x02  /* sta: scsi reset signal           */
        #define   SDP     0x01  /* sta: scsi parity signal          */

/*0e*/  u_char    nc_sstat1;
	#define   FF3210  0xf0	/* sta: bytes in the scsi fifo      */

/*0f*/  u_char    nc_sstat2;
        #define   ILF1    0x80  /* sta: data in SIDL register msb[W]*/
        #define   ORF1    0x40  /* sta: data in SODR register msb[W]*/
        #define   OLF1    0x20  /* sta: data in SODL register msb[W]*/
        #define   DM      0x04  /* sta: DIFFSENS mismatch (895/6 only) */
        #define   LDSC    0x02  /* sta: disconnect & reconnect      */

/*10*/  u_char    nc_dsa;	/* --> Base page                    */
/*11*/  u_char    nc_dsa1;
/*12*/  u_char    nc_dsa2;
/*13*/  u_char    nc_dsa3;

/*14*/  u_char    nc_istat;	/* --> Main Command and status      */
        #define   CABRT   0x80  /* cmd: abort current operation     */
        #define   SRST    0x40  /* mod: reset chip                  */
        #define   SIGP    0x20  /* r/w: message from host to ncr    */
        #define   SEM     0x10  /* r/w: message between host + ncr  */
        #define   CON     0x08  /* sta: connected to scsi           */
        #define   INTF    0x04  /* sta: int on the fly (reset by wr)*/
        #define   SIP     0x02  /* sta: scsi-interrupt              */
        #define   DIP     0x01  /* sta: host/script interrupt       */

/*15*/  u_char    nc_istat1;	/* 896 and later cores only */
        #define   FLSH    0x04  /* sta: chip is flushing            */
        #define   SRUN    0x02  /* sta: scripts are running         */
        #define   SIRQD   0x01  /* r/w: disable INT pin             */

/*16*/  u_char    nc_mbox0;	/* 896 and later cores only */
/*17*/  u_char    nc_mbox1;	/* 896 and later cores only */

/*18*/	u_char	  nc_ctest0;
/*19*/  u_char    nc_ctest1;

/*1a*/  u_char    nc_ctest2;
	#define   CSIGP   0x40
				/* bits 0-2,7 rsvd for C1010        */

/*1b*/  u_char    nc_ctest3;
	#define   FLF     0x08  /* cmd: flush dma fifo              */
	#define   CLF	  0x04	/* cmd: clear dma fifo		    */
	#define   FM      0x02  /* mod: fetch pin mode              */
	#define   WRIE    0x01  /* mod: write and invalidate enable */
				/* bits 4-7 rsvd for C1010          */

/*1c*/  u_int32    nc_temp;	/* ### Temporary stack              */

/*20*/	u_char	  nc_dfifo;
/*21*/  u_char    nc_ctest4;
	#define   BDIS    0x80  /* mod: burst disable               */
	#define   MPEE    0x08  /* mod: master parity error enable  */

/*22*/  u_char    nc_ctest5;
	#define   DFS     0x20  /* mod: dma fifo size               */
				/* bits 0-1, 3-7 rsvd for C1010          */
/*23*/  u_char    nc_ctest6;

/*24*/  u_int32    nc_dbc;	/* ### Byte count and command       */
/*28*/  u_int32    nc_dnad;	/* ### Next command register        */
/*2c*/  u_int32    nc_dsp;	/* --> Script Pointer               */
/*30*/  u_int32    nc_dsps;	/* --> Script pointer save/opcode#2 */

/*34*/  u_char     nc_scratcha;  /* Temporary register a            */
/*35*/  u_char     nc_scratcha1;
/*36*/  u_char     nc_scratcha2;
/*37*/  u_char     nc_scratcha3;

/*38*/  u_char    nc_dmode;
	#define   BL_2    0x80  /* mod: burst length shift value +2 */
	#define   BL_1    0x40  /* mod: burst length shift value +1 */
	#define   ERL     0x08  /* mod: enable read line            */
	#define   ERMP    0x04  /* mod: enable read multiple        */
	#define   BOF     0x02  /* mod: burst op code fetch         */

/*39*/  u_char    nc_dien;
/*3a*/  u_char    nc_sbr;

/*3b*/  u_char    nc_dcntl;	/* --> Script execution control     */
	#define   CLSE    0x80  /* mod: cache line size enable      */
	#define   PFF     0x40  /* cmd: pre-fetch flush             */
	#define   PFEN    0x20  /* mod: pre-fetch enable            */
	#define   SSM     0x10  /* mod: single step mode            */
	#define   IRQM    0x08  /* mod: irq mode (1 = totem pole !) */
	#define   STD     0x04  /* cmd: start dma mode              */
	#define   IRQD    0x02  /* mod: irq disable                 */
 	#define	  NOCOM   0x01	/* cmd: protect sfbr while reselect */
				/* bits 0-1 rsvd for C1010          */

/*3c*/  u_int32    nc_adder;

/*40*/  u_short   nc_sien;	/* -->: interrupt enable            */
/*42*/  u_short   nc_sist;	/* <--: interrupt status            */
        #define   SBMC    0x1000/* sta: SCSI Bus Mode Change (895/6 only) */
        #define   STO     0x0400/* sta: timeout (select)            */
        #define   GEN     0x0200/* sta: timeout (general)           */
        #define   HTH     0x0100/* sta: timeout (handshake)         */
        #define   MA      0x80  /* sta: phase mismatch              */
        #define   CMP     0x40  /* sta: arbitration complete        */
        #define   SEL     0x20  /* sta: selected by another device  */
        #define   RSL     0x10  /* sta: reselected by another device*/
        #define   SGE     0x08  /* sta: gross error (over/underflow)*/
        #define   UDC     0x04  /* sta: unexpected disconnect       */
        #define   RST     0x02  /* sta: scsi bus reset detected     */
        #define   PAR     0x01  /* sta: scsi parity error           */

/*44*/  u_char    nc_slpar;
/*45*/  u_char    nc_swide;
/*46*/  u_char    nc_macntl;
/*47*/  u_char    nc_gpcntl;
/*48*/  u_char    nc_stime0;    /* cmd: timeout for select&handshake*/
/*49*/  u_char    nc_stime1;    /* cmd: timeout user defined        */
/*4a*/  u_short   nc_respid;    /* sta: Reselect-IDs                */

/*4c*/  u_char    nc_stest0;

/*4d*/  u_char    nc_stest1;
	#define   SCLK    0x80	/* Use the PCI clock as SCSI clock	*/
	#define   DBLEN   0x08	/* clock doubler running		*/
	#define   DBLSEL  0x04	/* clock doubler selected		*/
  

/*4e*/  u_char    nc_stest2;
	#define   ROF     0x40	/* reset scsi offset (after gross error!) */
	#define   EXT     0x02  /* extended filtering                     */

/*4f*/  u_char    nc_stest3;
	#define   TE     0x80	/* c: tolerAnt enable */
	#define   HSC    0x20	/* c: Halt SCSI Clock */
	#define   CSF    0x02	/* c: clear scsi fifo */

/*50*/  u_short   nc_sidl;	/* Lowlevel: latched from scsi data */
/*52*/  u_char    nc_stest4;
	#define   SMODE  0xc0	/* SCSI bus mode      (895/6 only) */
	#define    SMODE_HVD 0x40	/* High Voltage Differential       */
	#define    SMODE_SE  0x80	/* Single Ended                    */
	#define    SMODE_LVD 0xc0	/* Low Voltage Differential        */
	#define   LCKFRQ 0x20	/* Frequency Lock (895/6 only)     */
				/* bits 0-5 rsvd for C1010          */

/*53*/  u_char    nc_53_;
/*54*/  u_short   nc_sodl;	/* Lowlevel: data out to scsi data  */
/*56*/	u_char    nc_ccntl0;	/* Chip Control 0 (896)             */
	#define   ENPMJ  0x80	/* Enable Phase Mismatch Jump       */
	#define   PMJCTL 0x40	/* Phase Mismatch Jump Control      */
	#define   ENNDJ  0x20	/* Enable Non Data PM Jump          */
	#define   DISFC  0x10	/* Disable Auto FIFO Clear          */
	#define   DILS   0x02	/* Disable Internal Load/Store      */
	#define   DPR    0x01	/* Disable Pipe Req                 */

/*57*/	u_char    nc_ccntl1;	/* Chip Control 1 (896)             */
	#define   ZMOD   0x80	/* High Impedance Mode              */
	#define	  DIC	 0x10	/* Disable Internal Cycles	    */
	#define   DDAC   0x08	/* Disable Dual Address Cycle       */
	#define   XTIMOD 0x04	/* 64-bit Table Ind. Indexing Mode  */
	#define   EXTIBMV 0x02	/* Enable 64-bit Table Ind. BMOV    */
	#define   EXDBMV 0x01	/* Enable 64-bit Direct BMOV        */

/*58*/  u_short   nc_sbdl;	/* Lowlevel: data from scsi data    */
/*5a*/  u_short   nc_5a_;

/*5c*/  u_char    nc_scr0;	/* Working register B               */
/*5d*/  u_char    nc_scr1;	/*                                  */
/*5e*/  u_char    nc_scr2;	/*                                  */
/*5f*/  u_char    nc_scr3;	/*                                  */

/*60*/  u_char    nc_scrx[64];	/* Working register C-R             */
/*a0*/	u_int32   nc_mmrs;	/* Memory Move Read Selector        */
/*a4*/	u_int32   nc_mmws;	/* Memory Move Write Selector       */
/*a8*/	u_int32   nc_sfs;	/* Script Fetch Selector            */
/*ac*/	u_int32   nc_drs;	/* DSA Relative Selector            */
/*b0*/	u_int32   nc_sbms;	/* Static Block Move Selector       */
/*b4*/	u_int32   nc_dbms;	/* Dynamic Block Move Selector      */
/*b8*/	u_int32   nc_dnad64;	/* DMA Next Address 64              */
/*bc*/	u_short   nc_scntl4;    /* C1010 only                       */
	#define   U3EN   0x80	/* Enable Ultra 3                   */
	#define   AIPEN	 0x40   /* Allow check upper byte lanes     */
	#define   XCLKH_DT 0x08 /* Extra clock of data hold on DT
					transfer edge	            */
	#define   XCLKH_ST 0x04 /* Extra clock of data hold on ST
					transfer edge	            */

/*be*/  u_char   nc_aipcntl0;	/* Epat Control 1 C1010 only        */
/*bf*/  u_char   nc_aipcntl1;	/* AIP Control C1010_66 Only        */

/*c0*/	u_int32   nc_pmjad1;	/* Phase Mismatch Jump Address 1    */
/*c4*/	u_int32   nc_pmjad2;	/* Phase Mismatch Jump Address 2    */
/*c8*/	u_char    nc_rbc;	/* Remaining Byte Count             */
/*c9*/	u_char    nc_rbc1;	/*                                  */
/*ca*/	u_char    nc_rbc2;	/*                                  */
/*cb*/	u_char    nc_rbc3;	/*                                  */

/*cc*/	u_char    nc_ua;	/* Updated Address                  */
/*cd*/	u_char    nc_ua1;	/*                                  */
/*ce*/	u_char    nc_ua2;	/*                                  */
/*cf*/	u_char    nc_ua3;	/*                                  */
/*d0*/	u_int32   nc_esa;	/* Entry Storage Address            */
/*d4*/	u_char    nc_ia;	/* Instruction Address              */
/*d5*/	u_char    nc_ia1;
/*d6*/	u_char    nc_ia2;
/*d7*/	u_char    nc_ia3;
/*d8*/	u_int32   nc_sbc;	/* SCSI Byte Count (3 bytes only)   */
/*dc*/	u_int32   nc_csbc;	/* Cumulative SCSI Byte Count       */

                                /* Following for C1010 only         */
/*e0*/ u_short    nc_crcpad;    /* CRC Value                        */
/*e2*/ u_char     nc_crccntl0;  /* CRC control register             */
	#define   SNDCRC  0x10	/* Send CRC Request                 */
/*e3*/ u_char     nc_crccntl1;  /* CRC control register             */
/*e4*/ u_int32    nc_crcdata;   /* CRC data register                */ 
/*e8*/ u_int32	  nc_e8_;	/* rsvd 			    */
/*ec*/ u_int32	  nc_ec_;	/* rsvd 			    */
/*f0*/ u_short    nc_dfbc;      /* DMA FIFO byte count              */ 

};

/*-----------------------------------------------------------
**
**	Utility macros for the script.
**
**-----------------------------------------------------------
*/

#define REGJ(p,r) (offsetof(struct ncr_reg, p ## r))
#define REG(r) REGJ (nc_, r)

typedef u_int32 ncrcmd;

/*-----------------------------------------------------------
**
**	SCSI phases
**
**	DT phases illegal for ncr driver.
**
**-----------------------------------------------------------
*/

#define	SCR_DATA_OUT	0x00000000
#define	SCR_DATA_IN	0x01000000
#define	SCR_COMMAND	0x02000000
#define	SCR_STATUS	0x03000000
#define SCR_DT_DATA_OUT	0x04000000
#define SCR_DT_DATA_IN	0x05000000
#define SCR_MSG_OUT	0x06000000
#define SCR_MSG_IN      0x07000000

#define SCR_ILG_OUT	0x04000000
#define SCR_ILG_IN	0x05000000

/*-----------------------------------------------------------
**
**	Data transfer via SCSI.
**
**-----------------------------------------------------------
**
**	MOVE_ABS (LEN)
**	<<start address>>
**
**	MOVE_IND (LEN)
**	<<dnad_offset>>
**
**	MOVE_TBL
**	<<dnad_offset>>
**
**-----------------------------------------------------------
*/

#define OPC_MOVE          0x08000000

#define SCR_MOVE_ABS(l) ((0x00000000 | OPC_MOVE) | (l))
#define SCR_MOVE_IND(l) ((0x20000000 | OPC_MOVE) | (l))
#define SCR_MOVE_TBL     (0x10000000 | OPC_MOVE)

#define SCR_CHMOV_ABS(l) ((0x00000000) | (l))
#define SCR_CHMOV_IND(l) ((0x20000000) | (l))
#define SCR_CHMOV_TBL     (0x10000000)

struct scr_tblmove {
        u_int32  size;
        u_int32  addr;
};

/*-----------------------------------------------------------
**
**	Selection
**
**-----------------------------------------------------------
**
**	SEL_ABS | SCR_ID (0..15)    [ | REL_JMP]
**	<<alternate_address>>
**
**	SEL_TBL | << dnad_offset>>  [ | REL_JMP]
**	<<alternate_address>>
**
**-----------------------------------------------------------
*/

#define	SCR_SEL_ABS	0x40000000
#define	SCR_SEL_ABS_ATN	0x41000000
#define	SCR_SEL_TBL	0x42000000
#define	SCR_SEL_TBL_ATN	0x43000000

struct scr_tblsel {
        u_char  sel_scntl4;	
        u_char  sel_sxfer;
        u_char  sel_id;
        u_char  sel_scntl3;
};

#define SCR_JMP_REL     0x04000000
#define SCR_ID(id)	(((u_int32)(id)) << 16)

/*-----------------------------------------------------------
**
**	Waiting for Disconnect or Reselect
**
**-----------------------------------------------------------
**
**	WAIT_DISC
**	dummy: <<alternate_address>>
**
**	WAIT_RESEL
**	<<alternate_address>>
**
**-----------------------------------------------------------
*/

#define	SCR_WAIT_DISC	0x48000000
#define SCR_WAIT_RESEL  0x50000000

/*-----------------------------------------------------------
**
**	Bit Set / Reset
**
**-----------------------------------------------------------
**
**	SET (flags {|.. })
**
**	CLR (flags {|.. })
**
**-----------------------------------------------------------
*/

#define SCR_SET(f)     (0x58000000 | (f))
#define SCR_CLR(f)     (0x60000000 | (f))

#define	SCR_CARRY	0x00000400
#define	SCR_TRG		0x00000200
#define	SCR_ACK		0x00000040
#define	SCR_ATN		0x00000008




/*-----------------------------------------------------------
**
**	Memory to memory move
**
**-----------------------------------------------------------
**
**	COPY (bytecount)
**	<< source_address >>
**	<< destination_address >>
**
**	SCR_COPY   sets the NO FLUSH option by default.
**	SCR_COPY_F does not set this option.
**
**	For chips which do not support this option,
**	ncr_copy_and_bind() will remove this bit.
**-----------------------------------------------------------
*/

#define SCR_NO_FLUSH 0x01000000

#define SCR_COPY(n) (0xc0000000 | SCR_NO_FLUSH | (n))
#define SCR_COPY_F(n) (0xc0000000 | (n))

/*-----------------------------------------------------------
**
**	Register move and binary operations
**
**-----------------------------------------------------------
**
**	SFBR_REG (reg, op, data)        reg  = SFBR op data
**	<< 0 >>
**
**	REG_SFBR (reg, op, data)        SFBR = reg op data
**	<< 0 >>
**
**	REG_REG  (reg, op, data)        reg  = reg op data
**	<< 0 >>
**
**-----------------------------------------------------------
**	On 810A, 860, 825A, 875, 895 and 896 chips the content 
**	of SFBR register can be used as data (SCR_SFBR_DATA).
**	The 896 has additionnal IO registers starting at 
**	offset 0x80. Bit 7 of register offset is stored in 
**	bit 7 of the SCRIPTS instruction first DWORD.
**-----------------------------------------------------------
*/

#define SCR_REG_OFS(ofs) ((((ofs) & 0x7f) << 16ul) + ((ofs) & 0x80)) 

#define SCR_SFBR_REG(reg,op,data) \
        (0x68000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))

#define SCR_REG_SFBR(reg,op,data) \
        (0x70000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))

#define SCR_REG_REG(reg,op,data) \
        (0x78000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))


#define      SCR_LOAD   0x00000000
#define      SCR_SHL    0x01000000
#define      SCR_OR     0x02000000
#define      SCR_XOR    0x03000000
#define      SCR_AND    0x04000000
#define      SCR_SHR    0x05000000
#define      SCR_ADD    0x06000000
#define      SCR_ADDC   0x07000000

#define      SCR_SFBR_DATA   (0x00800000>>8ul)	/* Use SFBR as data */

/*-----------------------------------------------------------
**
**	FROM_REG (reg)		  SFBR = reg
**	<< 0 >>
**
**	TO_REG	 (reg)		  reg  = SFBR
**	<< 0 >>
**
**	LOAD_REG (reg, data)	  reg  = <data>
**	<< 0 >>
**
**	LOAD_SFBR(data) 	  SFBR = <data>
**	<< 0 >>
**
**-----------------------------------------------------------
*/

#define	SCR_FROM_REG(reg) \
	SCR_REG_SFBR(reg,SCR_OR,0)

#define	SCR_TO_REG(reg) \
	SCR_SFBR_REG(reg,SCR_OR,0)

#define	SCR_LOAD_REG(reg,data) \
	SCR_REG_REG(reg,SCR_LOAD,data)

#define SCR_LOAD_SFBR(data) \
        (SCR_REG_SFBR (gpreg, SCR_LOAD, data))

/*-----------------------------------------------------------
**
**	LOAD  from memory   to register.
**	STORE from register to memory.
**
**	Only supported by 810A, 860, 825A, 875, 895 and 896.
**
**-----------------------------------------------------------
**
**	LOAD_ABS (LEN)
**	<<start address>>
**
**	LOAD_REL (LEN)        (DSA relative)
**	<<dsa_offset>>
**
**-----------------------------------------------------------
*/

#define SCR_REG_OFS2(ofs) (((ofs) & 0xff) << 16ul)
#define SCR_NO_FLUSH2	0x02000000
#define SCR_DSA_REL2	0x10000000

#define SCR_LOAD_R(reg, how, n) \
        (0xe1000000 | how | (SCR_REG_OFS2(REG(reg))) | (n))

#define SCR_STORE_R(reg, how, n) \
        (0xe0000000 | how | (SCR_REG_OFS2(REG(reg))) | (n))

#define SCR_LOAD_ABS(reg, n)	SCR_LOAD_R(reg, SCR_NO_FLUSH2, n)
#define SCR_LOAD_REL(reg, n)	SCR_LOAD_R(reg, SCR_NO_FLUSH2|SCR_DSA_REL2, n)
#define SCR_LOAD_ABS_F(reg, n)	SCR_LOAD_R(reg, 0, n)
#define SCR_LOAD_REL_F(reg, n)	SCR_LOAD_R(reg, SCR_DSA_REL2, n)

#define SCR_STORE_ABS(reg, n)	SCR_STORE_R(reg, SCR_NO_FLUSH2, n)
#define SCR_STORE_REL(reg, n)	SCR_STORE_R(reg, SCR_NO_FLUSH2|SCR_DSA_REL2,n)
#define SCR_STORE_ABS_F(reg, n)	SCR_STORE_R(reg, 0, n)
#define SCR_STORE_REL_F(reg, n)	SCR_STORE_R(reg, SCR_DSA_REL2, n)


/*-----------------------------------------------------------
**
**	Waiting for Disconnect or Reselect
**
**-----------------------------------------------------------
**
**	JUMP            [ | IFTRUE/IFFALSE ( ... ) ]
**	<<address>>
**
**	JUMPR           [ | IFTRUE/IFFALSE ( ... ) ]
**	<<distance>>
**
**	CALL            [ | IFTRUE/IFFALSE ( ... ) ]
**	<<address>>
**
**	CALLR           [ | IFTRUE/IFFALSE ( ... ) ]
**	<<distance>>
**
**	RETURN          [ | IFTRUE/IFFALSE ( ... ) ]
**	<<dummy>>
**
**	INT             [ | IFTRUE/IFFALSE ( ... ) ]
**	<<ident>>
**
**	INT_FLY         [ | IFTRUE/IFFALSE ( ... ) ]
**	<<ident>>
**
**	Conditions:
**	     WHEN (phase)
**	     IF   (phase)
**	     CARRYSET
**	     DATA (data, mask)
**
**-----------------------------------------------------------
*/

#define SCR_NO_OP       0x80000000
#define SCR_JUMP        0x80080000
#define SCR_JUMP64      0x80480000
#define SCR_JUMPR       0x80880000
#define SCR_CALL        0x88080000
#define SCR_CALLR       0x88880000
#define SCR_RETURN      0x90080000
#define SCR_INT         0x98080000
#define SCR_INT_FLY     0x98180000

#define IFFALSE(arg)   (0x00080000 | (arg))
#define IFTRUE(arg)    (0x00000000 | (arg))

#define WHEN(phase)    (0x00030000 | (phase))
#define IF(phase)      (0x00020000 | (phase))

#define DATA(D)        (0x00040000 | ((D) & 0xff))
#define MASK(D,M)      (0x00040000 | (((M ^ 0xff) & 0xff) << 8ul)|((D) & 0xff))

#define CARRYSET       (0x00200000)

/*-----------------------------------------------------------
**
**	SCSI  constants.
**
**-----------------------------------------------------------
*/

/*
**	Messages
*/

#define	M_COMPLETE	(0x00)
#define	M_EXTENDED	(0x01)
#define	M_SAVE_DP	(0x02)
#define	M_RESTORE_DP	(0x03)
#define	M_DISCONNECT	(0x04)
#define	M_ID_ERROR	(0x05)
#define	M_ABORT		(0x06)
#define	M_REJECT	(0x07)
#define	M_NOOP		(0x08)
#define	M_PARITY	(0x09)
#define	M_LCOMPLETE	(0x0a)
#define	M_FCOMPLETE	(0x0b)
#define	M_RESET		(0x0c)
#define	M_ABORT_TAG	(0x0d)
#define	M_CLEAR_QUEUE	(0x0e)
#define	M_INIT_REC	(0x0f)
#define	M_REL_REC	(0x10)
#define	M_TERMINATE	(0x11)
#define	M_SIMPLE_TAG	(0x20)
#define	M_HEAD_TAG	(0x21)
#define	M_ORDERED_TAG	(0x22)
#define	M_IGN_RESIDUE	(0x23)
#define	M_IDENTIFY   	(0x80)

#define	M_X_MODIFY_DP	(0x00)
#define	M_X_SYNC_REQ	(0x01)
#define	M_X_WIDE_REQ	(0x03)
#define	M_X_PPR_REQ	(0x04)

/*
**	Status
*/

#define	S_GOOD		(0x00)
#define	S_CHECK_COND	(0x02)
#define	S_COND_MET	(0x04)
#define	S_BUSY		(0x08)
#define	S_INT		(0x10)
#define	S_INT_COND_MET	(0x14)
#define	S_CONFLICT	(0x18)
#define	S_TERMINATED	(0x20)
#define	S_QUEUE_FULL	(0x28)
#define	S_ILLEGAL	(0xff)
#define	S_SENSE		(0x80)

/*
 * End of ncrreg from FreeBSD
 */

#endif /* !defined HOSTS_C */

#endif /* defined SYM53C8XX_DEFS_H */
