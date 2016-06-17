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

#ifndef SYM53C8XX_H
#define SYM53C8XX_H

#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif
#include <linux/config.h>

/*
 *  Compatibility with ncr53c8xx and sym53c8xx configuration options.
 */
#ifndef	CONFIG_SCSI_SYM53C8XX_IOMAPPED
#ifdef	CONFIG_SCSI_NCR53C8XX_IOMAPPED
#define	CONFIG_SCSI_SYM53C8XX_IOMAPPED		CONFIG_SCSI_NCR53C8XX_IOMAPPED
#endif
#endif

#ifndef	CONFIG_SCSI_SYM53C8XX_DEFAULT_TAGS
#ifdef	CONFIG_SCSI_NCR53C8XX_DEFAULT_TAGS
#define	CONFIG_SCSI_SYM53C8XX_DEFAULT_TAGS	CONFIG_SCSI_NCR53C8XX_DEFAULT_TAGS
#endif
#endif

#ifndef	CONFIG_SCSI_SYM53C8XX_MAX_TAGS
#ifdef	CONFIG_SCSI_NCR53C8XX_MAX_TAGS
#define	CONFIG_SCSI_SYM53C8XX_MAX_TAGS		CONFIG_SCSI_NCR53C8XX_MAX_TAGS
#endif
#endif

int sym53c8xx_detect(Scsi_Host_Template *tpnt);
const char *sym53c8xx_info(struct Scsi_Host *host);

int sym53c8xx_queue_command(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));

int sym53c8xx_eh_abort_handler(Scsi_Cmnd *);
int sym53c8xx_eh_device_reset_handler(Scsi_Cmnd *);
int sym53c8xx_eh_bus_reset_handler(Scsi_Cmnd *);
int sym53c8xx_eh_host_reset_handler(Scsi_Cmnd *);

#ifdef MODULE
int sym53c8xx_release(struct Scsi_Host *);
#else
#define sym53c8xx_release NULL
#endif


/*
 *  Host template defintion
 */
#if (LINUX_VERSION_CODE >= 0x020400) || defined(HOSTS_C) || defined(MODULE)

#include <scsi/scsicam.h>

#define SYM53C8XX {							\
	name:			"sym53c8xx",				\
	detect:			sym53c8xx_detect,			\
	release:		sym53c8xx_release,			\
	info:			sym53c8xx_info, 			\
	queuecommand:		sym53c8xx_queue_command,		\
	use_new_eh_code:	1,					\
	eh_abort_handler:	sym53c8xx_eh_abort_handler,		\
	eh_device_reset_handler:sym53c8xx_eh_device_reset_handler,	\
	eh_bus_reset_handler:	sym53c8xx_eh_bus_reset_handler,		\
	eh_host_reset_handler:	sym53c8xx_eh_host_reset_handler,	\
	bios_param:		scsicam_bios_param,			\
	can_queue:		0,					\
	this_id:		7,					\
	sg_tablesize:		0,					\
	cmd_per_lun:		0,					\
	use_clustering:		DISABLE_CLUSTERING,			\
	highmem_io:		1}

#endif /* defined(HOSTS_C) || defined(MODULE) */ 

/*
 *  Translate kernel configuration parameters
 *  into corresponding driver parameters.
 */
#if !defined(HOSTS_C)

/*
 *  Use normal IO if configured.
 *  Normal IO forced for alpha.
 *  Forced to MMIO for sparc.
 */
#if defined(__alpha__)
#define	SYM_CONF_IOMAPPED
#elif defined(__sparc__)
#undef SYM_CONF_IOMAPPED
/* #elif defined(__powerpc__) */
/* #define	SYM_CONF_IOMAPPED */
/* #define SYM_OPT_NO_BUS_MEMORY_MAPPING */
#elif defined(CONFIG_SCSI_SYM53C8XX_IOMAPPED)
#define	SYM_CONF_IOMAPPED
#endif

/*
 *  DMA addressing mode.
 *
 *  0 : 32 bit addressing for all chips.
 *  1 : 40 bit addressing when supported by chip.
 *  2 : 64 bit addressing when supported by chip,
 *      limited to 16 segments of 4 GB -> 64 GB max.
 */
#ifdef	CONFIG_SCSI_SYM53C8XX_DMA_ADDRESSING_MODE
#define	SYM_CONF_DMA_ADDRESSING_MODE CONFIG_SCSI_SYM53C8XX_DMA_ADDRESSING_MODE
#endif

/*
 *  NCR PQS/PDS special device support.
 */
#if 1
#define SYM_CONF_PQS_PDS_SUPPORT
#endif

/*
 *  NVRAM support.
 */
#if 1
#define SYM_CONF_NVRAM_SUPPORT		(1)
#define SYM_SETUP_SYMBIOS_NVRAM		(1)
#define SYM_SETUP_TEKRAM_NVRAM		(1)
#endif

/*
 *  These options are not tunable from 'make config'
 */
#if 1
#define	SYM_LINUX_PROC_INFO_SUPPORT
#define SYM_LINUX_BOOT_COMMAND_LINE_SUPPORT
#define SYM_LINUX_USER_COMMAND_SUPPORT
#define SYM_LINUX_USER_INFO_SUPPORT
#define SYM_LINUX_DEBUG_CONTROL_SUPPORT
#endif

/*
 *  Also handle old NCR chips if not (0).
 */
#define SYM_CONF_GENERIC_SUPPORT	(1)

/*
 *  Allow tags from 2 to 256, default 8
 */
#ifndef CONFIG_SCSI_SYM53C8XX_MAX_TAGS
#define CONFIG_SCSI_SYM53C8XX_MAX_TAGS	(8)
#endif

#if	CONFIG_SCSI_SYM53C8XX_MAX_TAGS < 2
#define SYM_CONF_MAX_TAG	(2)
#elif	CONFIG_SCSI_SYM53C8XX_MAX_TAGS > 256
#define SYM_CONF_MAX_TAG	(256)
#else
#define	SYM_CONF_MAX_TAG	CONFIG_SCSI_SYM53C8XX_MAX_TAGS
#endif

#ifndef	CONFIG_SCSI_SYM53C8XX_DEFAULT_TAGS
#define	CONFIG_SCSI_SYM53C8XX_DEFAULT_TAGS	SYM_CONF_MAX_TAG
#endif

/*
 *  Anyway, we configure the driver for at least 64 tags per LUN. :)
 */
#if	SYM_CONF_MAX_TAG <= 64
#define SYM_CONF_MAX_TAG_ORDER	(6)
#elif	SYM_CONF_MAX_TAG <= 128
#define SYM_CONF_MAX_TAG_ORDER	(7)
#else
#define SYM_CONF_MAX_TAG_ORDER	(8)
#endif

/*
 *  Sync transfer frequency at startup.
 *  Allow up to ULTRA-160. The driver will scale the value 
 *  according to controller capabilities.
 */
#define	CONFIG_SCSI_SYM53C8XX_DEFAULT_SYNC (9)

/*
 *  Max number of SG entries.
 */
#define SYM_CONF_MAX_SG		(96)

/*
 *  Max number of LUNs per target.
 */
#if 1 /* defined CONFIG_SCSI_MULTI_LUN */
#define	CONFIG_SCSI_SYM53C8XX_MAX_LUN	(16)
#else
#define	CONFIG_SCSI_SYM53C8XX_MAX_LUN	(1)
#endif

/*
 *  Driver setup structure.
 *
 *  This structure is initialized from linux config options.
 *  It can be overridden at boot-up by the boot command line.
 */
struct sym_driver_setup {
	u_char	pci_parity;
	u_char	scsi_parity;
	u_short	max_tag;
	u_char	min_sync;
	u_char	burst_order;
	u_char	scsi_led;
	u_char	max_wide;
	u_char	scsi_diff;
	u_char	irq_mode;
	u_char	scsi_bus_check;
	u_char	host_id;
	u_char	max_offs;
	u_char	max_lun;
	u_char	pci_fix_up;

	u_char	reverse_probe;
	u_char	verbose;
	u_short	debug;
	u_char	settle_delay;
	u_char	use_nvram;
	u_long	excludes[8];
	char	tag_ctrl[100];
};

#define SYM_SETUP_PCI_PARITY		sym_driver_setup.pci_parity
#define SYM_SETUP_SCSI_PARITY		sym_driver_setup.scsi_parity
#define SYM_SETUP_MAX_TAG		sym_driver_setup.max_tag
#define SYM_SETUP_MIN_SYNC		sym_driver_setup.min_sync
#define SYM_SETUP_BURST_ORDER		sym_driver_setup.burst_order
#define SYM_SETUP_SCSI_LED		sym_driver_setup.scsi_led
#define SYM_SETUP_MAX_WIDE		sym_driver_setup.max_wide
#define SYM_SETUP_SCSI_DIFF		sym_driver_setup.scsi_diff
#define SYM_SETUP_IRQ_MODE		sym_driver_setup.irq_mode
#define SYM_SETUP_SCSI_BUS_CHECK	sym_driver_setup.scsi_bus_check
#define SYM_SETUP_HOST_ID		sym_driver_setup.host_id
#define SYM_SETUP_MAX_OFFS		sym_driver_setup.max_offs
#define SYM_SETUP_MAX_LUN		sym_driver_setup.max_lun
#define SYM_SETUP_PCI_FIX_UP		sym_driver_setup.pci_fix_up

/*
 *  Initial setup.
 *
 *  Can be overriden at startup by a command line.
 */
#define SYM_LINUX_DRIVER_SETUP			\
{						\
	1,	/* pci_parity */		\
	1,	/* scsi_parity */		\
	CONFIG_SCSI_SYM53C8XX_DEFAULT_TAGS,	\
	CONFIG_SCSI_SYM53C8XX_DEFAULT_SYNC,	\
	7,	/* burst_order */		\
	1,	/* scsi_led */			\
	1,	/* max_wide */			\
	1,	/* scsi_diff */			\
	0,	/* irq_mode */			\
	1,	/* scsi_bus_check */		\
	7,	/* host_id */			\
	62,	/* max_offs */			\
	CONFIG_SCSI_SYM53C8XX_MAX_LUN,		\
	3,	/* pci_fix_up */		\
	0,	/* reverse_probe */		\
	0,	/* verbose */			\
	0,	/* debug */			\
	3,	/* settle_delay */		\
	1,	/* use_nvram */			\
}

/*
 *  Boot fail safe setup.
 *
 *  Override initial setup from boot command line:
 *    sym53c8xx=safe:y
 */
#define SYM_LINUX_DRIVER_SAFE_SETUP		\
{						\
	0,	/* pci_parity */		\
	0,	/* scsi_parity */		\
	0,	/* max_tag */			\
	50,	/* min_sync */			\
	0,	/* burst_order */		\
	0,	/* scsi_led */			\
	1,	/* max_wide */			\
	1,	/* scsi_diff */			\
	0,	/* irq_mode */			\
	2,	/* scsi_bus_check */		\
	7,	/* host_id */			\
	15,	/* max_offs */			\
	1,	/* max_lun */			\
	0,	/* pci_fix_up */		\
	0,	/* reverse_probe */		\
	2,	/* verbose */			\
	0,	/* debug */			\
	10,	/* settle_delay */		\
	1,	/* use_nvram */			\
}

/*
 *  This structure is initialized from linux config options.
 *  It can be overridden at boot-up by the boot command line.
 */
#ifdef SYM_GLUE_C
struct sym_driver_setup
	sym_driver_setup = SYM_LINUX_DRIVER_SETUP;
#ifdef SYM_LINUX_DEBUG_CONTROL_SUPPORT
u_int	sym_debug_flags = 0;
#endif
#else
extern struct sym_driver_setup sym_driver_setup;
#ifdef SYM_LINUX_DEBUG_CONTROL_SUPPORT
extern u_int sym_debug_flags;
#endif
#endif /* SYM_GLUE_C */

#ifdef SYM_LINUX_DEBUG_CONTROL_SUPPORT
#define DEBUG_FLAGS	sym_debug_flags
#endif
#define boot_verbose	sym_driver_setup.verbose

#endif /* !defined(HOSTS_C) */ 

#endif /* SYM53C8XX_H */
