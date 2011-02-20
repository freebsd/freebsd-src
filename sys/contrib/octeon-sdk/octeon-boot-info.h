/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/



/**
 * @file
 *
 * Interface to Octeon boot structure
 *
 * <hr>$Revision:  $<hr>
 */

#ifndef __OCTEON_BOOT_INFO_H__
#define __OCTEON_BOOT_INFO_H__

#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <linux/types.h>
#endif

#ifndef __ASSEMBLY__

/*
 * This structure is access by bootloader, Linux kernel and the Linux
 * user space utility "bootoct".

 * In the bootloader, this structure is accessed by assembly code in start.S,
 * so any changes to content or size must be reflected there as well.

 * This is placed at a fixed address in DRAM, so that cores can access it
 * when they come out of reset.  It is used to setup the minimal bootloader
 * runtime environment (stack, but no heap, global data ptr) that is needed
 * by the non-boot cores to setup the environment for the applications.
 * The boot_info_addr is the address of a boot_info_block_t structure
 * which contains more core-specific information.
 *
 * The Linux kernel and the Linux bootoct utility access this structure for
 * implementing CPU hotplug functionality and booting of idle cores with SE
 * apps respectively.
 *
 */
typedef struct
{
    /* First stage address - in ram instead of flash */
    uint64_t code_addr;
    /* Setup code for application, NOT application entry point */
    uint32_t app_start_func_addr;
    /* k0 is used for global data - needs to be passed to other cores */
    uint32_t k0_val;
    /* Address of boot info block structure */
    uint64_t boot_info_addr;
    uint32_t flags;         /* flags */
    uint32_t pad;
} boot_init_vector_t;

/*
 * Definition of a data structure setup by the bootloader to enable Linux to
 * launch SE apps on idle cores.
 */

struct linux_app_boot_info
{
    uint32_t labi_signature;
    uint32_t start_core0_addr;
    uint32_t avail_coremask;
    uint32_t pci_console_active;
    uint32_t icache_prefetch_disable;
    uint64_t InitTLBStart_addr;
    uint32_t start_app_addr;
    uint32_t cur_exception_base;
    uint32_t no_mark_private_data;
    uint32_t compact_flash_common_base_addr;
    uint32_t compact_flash_attribute_base_addr;
    uint32_t led_display_base_addr;
#ifndef __OCTEON_NEWLIB__
#if defined(__U_BOOT__) || !defined(__KERNEL__)
    gd_t gd;
#endif
#endif
};
typedef struct linux_app_boot_info linux_app_boot_info_t;

#endif

/* If not to copy a lot of bootloader's structures
   here is only offset of requested member */
#define AVAIL_COREMASK_OFFSET_IN_LINUX_APP_BOOT_BLOCK    0x765c

/* hardcoded in bootloader */
#define LABI_ADDR_IN_BOOTLOADER                         0x700

#define LINUX_APP_BOOT_BLOCK_NAME "linux-app-boot"

#define LABI_SIGNATURE 0xAABBCC01

/*  from uboot-headers/octeon_mem_map.h */
#if defined(CVMX_BUILD_FOR_LINUX_KERNEL) || defined(__OCTEON_NEWLIB__)
#define EXCEPTION_BASE_INCR     (4 * 1024)
#endif

#define OCTEON_NUM_CORES    16
/* Increment size for exception base addresses (4k minimum) */
#define EXCEPTION_BASE_BASE     0
#define BOOTLOADER_PRIV_DATA_BASE        (EXCEPTION_BASE_BASE + 0x800)
#define BOOTLOADER_BOOT_VECTOR           (BOOTLOADER_PRIV_DATA_BASE)
#define BOOTLOADER_DEBUG_TRAMPOLINE      (BOOTLOADER_BOOT_VECTOR + BOOT_VECTOR_SIZE)   /* WORD */
#define BOOTLOADER_DEBUG_TRAMPOLINE_CORE (BOOTLOADER_DEBUG_TRAMPOLINE + 4)   /* WORD */

#define OCTEON_EXCEPTION_VECTOR_BLOCK_SIZE  (OCTEON_NUM_CORES*EXCEPTION_BASE_INCR) /* 16 4k blocks */
#define BOOTLOADER_DEBUG_REG_SAVE_BASE  (EXCEPTION_BASE_BASE + OCTEON_EXCEPTION_VECTOR_BLOCK_SIZE)

#define BOOT_VECTOR_NUM_WORDS           (8)
#define BOOT_VECTOR_SIZE                ((OCTEON_NUM_CORES*4)*BOOT_VECTOR_NUM_WORDS)


#endif /* __OCTEON_BOOT_INFO_H__ */
