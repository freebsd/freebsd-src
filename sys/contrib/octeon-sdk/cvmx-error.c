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
 * Interface to the Octeon extended error status.
 *
 * <hr>$Revision: 44252 $<hr>
 */
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-error.h>
#include <asm/octeon/cvmx-error-custom.h>
#include <asm/octeon/cvmx-pcie.h>
#include <asm/octeon/cvmx-srio.h>
#include <asm/octeon/cvmx-pexp-defs.h>
#else
#include "cvmx.h"
#include "cvmx-error.h"
#include "cvmx-error-custom.h"
#include "cvmx-pcie.h"
#include "cvmx-srio.h"
#include "cvmx-interrupt.h"
#endif

#define MAX_TABLE_SIZE 1024   /* Max number of error status bits we can support */

extern int cvmx_error_initialize_cn63xx(void);
extern int cvmx_error_initialize_cn63xxp1(void);
extern int cvmx_error_initialize_cn58xxp1(void);
extern int cvmx_error_initialize_cn58xx(void);
extern int cvmx_error_initialize_cn56xxp1(void);
extern int cvmx_error_initialize_cn56xx(void);
extern int cvmx_error_initialize_cn50xx(void);
extern int cvmx_error_initialize_cn52xxp1(void);
extern int cvmx_error_initialize_cn52xx(void);
extern int cvmx_error_initialize_cn38xxp2(void);
extern int cvmx_error_initialize_cn38xx(void);
extern int cvmx_error_initialize_cn31xx(void);
extern int cvmx_error_initialize_cn30xx(void);

/* Each entry in this array represents a status bit function or chain */
static CVMX_SHARED cvmx_error_info_t __cvmx_error_table[MAX_TABLE_SIZE];
static CVMX_SHARED int __cvmx_error_table_size = 0;
static CVMX_SHARED cvmx_error_flags_t __cvmx_error_flags;

#define REG_MATCH(h, reg_type, status_addr, status_mask) \
    ((h->reg_type == reg_type) && (h->status_addr == status_addr) && (h->status_mask == status_mask))

/**
 * @INTERNAL
 * Read a status or enable register from the hardware
 *
 * @param reg_type Register type to read
 * @param addr     Address to read
 *
 * @return Result of the read
 */
static uint64_t __cvmx_error_read_hw(cvmx_error_register_t reg_type, uint64_t addr)
{
    switch (reg_type)
    {
        case __CVMX_ERROR_REGISTER_NONE:
            return 0;
        case CVMX_ERROR_REGISTER_IO64:
            return cvmx_read_csr(addr);
        case CVMX_ERROR_REGISTER_IO32:
            return cvmx_read64_uint32(addr ^ 4);
        case CVMX_ERROR_REGISTER_PCICONFIG:
            return cvmx_pcie_cfgx_read(addr>>32, addr&0xffffffffull);
        case CVMX_ERROR_REGISTER_SRIOMAINT:
        {
            uint32_t r;
            if (cvmx_srio_config_read32(addr>>32, 0, -1, 0, 0, addr&0xffffffffull, &r))
                return 0;
            else
                return r;
        }
    }
    return 0;
}

/**
 * @INTERNAL
 * Write a status or enable register to the hardware
 *
 * @param reg_type Register type to write
 * @param addr     Address to write
 * @param value    Value to write
 */
static void __cvmx_error_write_hw(cvmx_error_register_t reg_type, uint64_t addr, uint64_t value)
{
    switch (reg_type)
    {
        case __CVMX_ERROR_REGISTER_NONE:
            return;
        case CVMX_ERROR_REGISTER_IO64:
            cvmx_write_csr(addr, value);
            return;
        case CVMX_ERROR_REGISTER_IO32:
            cvmx_write64_uint32(addr ^ 4, value);
            return;
        case CVMX_ERROR_REGISTER_PCICONFIG:
            cvmx_pcie_cfgx_write(addr>>32, addr&0xffffffffull, value);
            return;
        case CVMX_ERROR_REGISTER_SRIOMAINT:
        {
            cvmx_srio_config_write32(addr>>32, 0, -1, 0, 0, addr&0xffffffffull, value);
            return;
        }
    }
}

/**
 * @INTERNAL
 * Function for processing non leaf error status registers. This function
 * calls all handlers for this passed register and all children linked
 * to it.
 *
 * @param info   Error register to check
 *
 * @return Number of error status bits found or zero if no bits were set.
 */
int __cvmx_error_decode(const cvmx_error_info_t *info)
{
    uint64_t status;
    uint64_t enable;
    int i;
    int handled = 0;

    /* Read the status and enable state */
    status = __cvmx_error_read_hw(info->reg_type, info->status_addr);
    if (info->enable_addr)
        enable = __cvmx_error_read_hw(info->reg_type, info->enable_addr);
    else
        enable = 0;

    for (i = 0; i < __cvmx_error_table_size; i++)
    {
        const cvmx_error_info_t *h = &__cvmx_error_table[i];
        uint64_t masked_status = status;

        /* If this is a child of the current register then recurse and process
            the child */
        if ((h->parent.reg_type == info->reg_type) &&
            (h->parent.status_addr == info->status_addr) &&
            (status & h->parent.status_mask))
            handled += __cvmx_error_decode(h);

        if ((h->reg_type != info->reg_type) || (h->status_addr != info->status_addr))
            continue;

        /* If the corresponding enable bit is not set then we have nothing to do */
        if (h->enable_addr && h->enable_mask)
        {
            if (!(enable & h->enable_mask))
                continue;
        }

        /* Apply the mask to eliminate irrelevant bits */
        if (h->status_mask)
            masked_status &= h->status_mask;

        /* Finally call the handler function unless it is this function */
        if (masked_status && h->func && (h->func != __cvmx_error_decode))
            handled += h->func(h);
    }
    /* Ths should be the total errors found */
    return handled;
}

/**
 * @INTERNAL
 * This error bit handler simply prints a message and clears the status bit
 *
 * @param info   Error register to check
 *
 * @return
 */
int __cvmx_error_display(const cvmx_error_info_t *info)
{
    const char *message = (const char *)(long)info->user_info;
    /* This assumes that all bits in the status register are RO or R/W1C */
    __cvmx_error_write_hw(info->reg_type, info->status_addr, info->status_mask);
    cvmx_safe_printf("%s", message);
    return 1;
}

/**
 * Initalize the error status system. This should be called once
 * before any other functions are called. This function adds default
 * handlers for most all error events but does not enable them. Later
 * calls to cvmx_error_enable() are needed.
 *
 * @param flags  Optional flags.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_error_initialize(cvmx_error_flags_t flags)
{
    __cvmx_error_flags = flags;
    if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS2_X))
    {
        if (cvmx_error_initialize_cn63xx())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X))
    {
        if (cvmx_error_initialize_cn63xxp1())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN58XX_PASS1_X))
    {
        if (cvmx_error_initialize_cn58xxp1())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        if (cvmx_error_initialize_cn58xx())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X))
    {
        if (cvmx_error_initialize_cn56xxp1())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        if (cvmx_error_initialize_cn56xx())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        if (cvmx_error_initialize_cn50xx())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X))
    {
        if (cvmx_error_initialize_cn52xxp1())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        if (cvmx_error_initialize_cn52xx())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2))
    {
        if (cvmx_error_initialize_cn38xxp2())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        if (cvmx_error_initialize_cn38xx())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        if (cvmx_error_initialize_cn31xx())
            return -1;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        if (cvmx_error_initialize_cn30xx())
            return -1;
    }
    else
    {
        cvmx_warn("cvmx_error_initialize() needs update for this Octeon model\n");
        return -1;
    }

    if (__cvmx_error_custom_initialize())
        return -1;

    /* Enable all of the purely internal error sources by default */
    cvmx_error_enable_group(CVMX_ERROR_GROUP_INTERNAL, 0);

    /* Enable DDR error reporting based on the memory controllers */
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        cvmx_l2c_cfg_t l2c_cfg;
        l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
        if (l2c_cfg.s.dpres0)
            cvmx_error_enable_group(CVMX_ERROR_GROUP_LMC, 0);
        if (l2c_cfg.s.dpres1)
            cvmx_error_enable_group(CVMX_ERROR_GROUP_LMC, 1);
    }
    else
        cvmx_error_enable_group(CVMX_ERROR_GROUP_LMC, 0);

    /* Old PCI parts don't have a common PCI init, so enable error
        reporting if the bootloader told us we are a PCI host. PCIe
        is handled when cvmx_pcie_rc_initialize is called */
    if (!octeon_has_feature(OCTEON_FEATURE_PCIE) &&
        (cvmx_sysinfo_get()->bootloader_config_flags & CVMX_BOOTINFO_CFG_FLAG_PCI_HOST))
        cvmx_error_enable_group(CVMX_ERROR_GROUP_PCI, 0);

    /* FIXME: Why is this needed for CN63XX? */
    if (OCTEON_IS_MODEL(OCTEON_CN63XX))
        cvmx_write_csr(CVMX_PEXP_SLI_INT_SUM, 1);

    return 0;
}

/**
 * Poll the error status registers and call the appropriate error
 * handlers. This should be called in the RSL interrupt handler
 * for your application or operating system.
 *
 * @return Number of error handlers called. Zero means this call
 *         found no errors and was spurious.
 */
int cvmx_error_poll(void)
{
    int i;
    int count = 0;
    /* Call all handlers that don't have a parent */
    for (i = 0; i < __cvmx_error_table_size; i++)
        if (__cvmx_error_table[i].parent.reg_type == __CVMX_ERROR_REGISTER_NONE)
            count += __cvmx_error_decode(&__cvmx_error_table[i]);
    return count;
}

/**
 * Register to be called when an error status bit is set. Most users
 * will not need to call this function as cvmx_error_initialize()
 * registers default handlers for most error conditions. This function
 * is normally used to add more handlers without changing the existing
 * handlers.
 *
 * @param new_info Information about the handler for a error register. The
 *                 structure passed is copied and can be destroyed after the
 *                 call. All members of the structure must be populated, even the
 *                 parent information.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_error_add(const cvmx_error_info_t *new_info)
{
    if (__cvmx_error_table_size >= MAX_TABLE_SIZE)
    {
        cvmx_warn("cvmx-error table full\n");
        return -1;
    }
    __cvmx_error_table[__cvmx_error_table_size] = *new_info;
    __cvmx_error_table_size++;
    return 0;
}

/**
 * Remove all handlers for a status register and mask. Normally
 * this function should not be called. Instead a new handler should be
 * installed to replace the existing handler. In the even that all
 * reporting of a error bit should be removed, then use this
 * function.
 *
 * @param reg_type Type of the status register to remove
 * @param status_addr
 *                 Status register to remove.
 * @param status_mask
 *                 All handlers for this status register with this mask will be
 *                 removed.
 * @param old_info If not NULL, this is filled with information about the handler
 *                 that was removed.
 *
 * @return Zero on success, negative on failure (not found).
 */
int cvmx_error_remove(cvmx_error_register_t reg_type,
                        uint64_t status_addr, uint64_t status_mask,
                        cvmx_error_info_t *old_info)
{
    int found = 0;
    int i;
    for (i = 0; i < __cvmx_error_table_size; i++)
    {
        cvmx_error_info_t *h = &__cvmx_error_table[i];
        if (!REG_MATCH(h, reg_type, status_addr, status_mask))
            continue;
        if (old_info)
            *old_info = *h;
        memset(h, 0, sizeof(*h));
        found = 1;
    }
    if (found)
        return 0;
    else
    {
        cvmx_warn("cvmx-error remove couldn't find requested register\n");
        return -1;
    }
}

/**
 * Change the function and user_info for an existing error status
 * register. This function should be used to replace the default
 * handler with an application specific version as needed.
 *
 * @param reg_type Type of the status register to change
 * @param status_addr
 *                 Status register to change.
 * @param status_mask
 *                 All handlers for this status register with this mask will be
 *                 changed.
 * @param new_func New function to use to handle the error status
 * @param new_user_info
 *                 New user info parameter for the function
 * @param old_func If not NULL, the old function is returned. Useful for restoring
 *                 the old handler.
 * @param old_user_info
 *                 If not NULL, the old user info parameter.
 *
 * @return Zero on success, negative on failure
 */
int cvmx_error_change_handler(cvmx_error_register_t reg_type,
                        uint64_t status_addr, uint64_t status_mask,
                        cvmx_error_func_t new_func, uint64_t new_user_info,
                        cvmx_error_func_t *old_func, uint64_t *old_user_info)
{
    int found = 0;
    int i;
    for (i = 0; i < __cvmx_error_table_size; i++)
    {
        cvmx_error_info_t *h = &__cvmx_error_table[i];
        if (!REG_MATCH(h, reg_type, status_addr, status_mask))
            continue;
        if (old_func)
            *old_func = h->func;
        if (old_user_info)
            *old_user_info = h->user_info;
        h->func = new_func;
        h->user_info = new_user_info;
        found = 1;
    }
    if (found)
        return 0;
    else
    {
        cvmx_warn("cvmx-error change couldn't find requested register\n");
        return -1;
    }
}

/**
 * Enable all error registers for a logical group. This should be
 * called whenever a logical group is brought online.
 *
 * @param group  Logical group to enable
 * @param group_index
 *               Index for the group as defined in the cvmx_error_group_t
 *               comments.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_error_enable_group(cvmx_error_group_t group, int group_index)
{
    int i;
    uint64_t enable;

    if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_SIM)
        return 0;

    for (i = 0; i < __cvmx_error_table_size; i++)
    {
        const cvmx_error_info_t *h = &__cvmx_error_table[i];
        /* Skip entries that have a different group or group index. We
            also skip entries that don't have an enable */
        if ((h->group != group) || (h->group_index != group_index) || (!h->enable_addr))
            continue;
        /* Skip entries that have flags that don't match the user's
            selected flags */
        if (h->flags && (h->flags != (h->flags & __cvmx_error_flags)))
            continue;
        /* Update the enables for this entry */
        enable = __cvmx_error_read_hw(h->reg_type, h->enable_addr);
        if (h->reg_type == CVMX_ERROR_REGISTER_PCICONFIG)
            enable &= ~h->enable_mask; /* PCI bits have reversed polarity */
        else
            enable |= h->enable_mask;
        __cvmx_error_write_hw(h->reg_type, h->enable_addr, enable);
    }
    return 0;
}

/**
 * Disable all error registers for a logical group. This should be
 * called whenever a logical group is brought offline. Many blocks
 * will report spurious errors when offline unless this function
 * is called.
 *
 * @param group  Logical group to disable
 * @param group_index
 *               Index for the group as defined in the cvmx_error_group_t
 *               comments.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_error_disable_group(cvmx_error_group_t group, int group_index)
{
    int i;
    uint64_t enable;

    if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_SIM)
        return 0;

    for (i = 0; i < __cvmx_error_table_size; i++)
    {
        const cvmx_error_info_t *h = &__cvmx_error_table[i];
        /* Skip entries that have a different group or group index. We
            also skip entries that don't have an enable */
        if ((h->group != group) || (h->group_index != group_index) || (!h->enable_addr))
            continue;
        /* Update the enables for this entry */
        enable = __cvmx_error_read_hw(h->reg_type, h->enable_addr);
        if (h->reg_type == CVMX_ERROR_REGISTER_PCICONFIG)
            enable |= h->enable_mask; /* PCI bits have reversed polarity */
        else
            enable &= ~h->enable_mask;
        __cvmx_error_write_hw(h->reg_type, h->enable_addr, enable);
    }
    return 0;
}

/**
 * Enable all handlers for a specific status register mask.
 *
 * @param reg_type Type of the status register
 * @param status_addr
 *                 Status register address
 * @param status_mask
 *                 All handlers for this status register with this mask will be
 *                 enabled.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_error_enable(cvmx_error_register_t reg_type,
                        uint64_t status_addr, uint64_t status_mask)
{
    int found = 0;
    int i;
    uint64_t enable;
    for (i = 0; i < __cvmx_error_table_size; i++)
    {
        cvmx_error_info_t *h = &__cvmx_error_table[i];
        if (!REG_MATCH(h, reg_type, status_addr, status_mask) || !h->enable_addr)
            continue;
        enable = __cvmx_error_read_hw(h->reg_type, h->enable_addr);
        if (h->reg_type == CVMX_ERROR_REGISTER_PCICONFIG)
            enable &= ~h->enable_mask; /* PCI bits have reversed polarity */
        else
            enable |= h->enable_mask;
        __cvmx_error_write_hw(h->reg_type, h->enable_addr, enable);
        h->flags &= ~CVMX_ERROR_FLAGS_DISABLED;
        found = 1;
    }
    if (found)
        return 0;
    else
    {
        cvmx_warn("cvmx-error enable couldn't find requested register\n");
        return -1;
    }
}

/**
 * Disable all handlers for a specific status register and mask.
 *
 * @param reg_type Type of the status register
 * @param status_addr
 *                 Status register address
 * @param status_mask
 *                 All handlers for this status register with this mask will be
 *                 disabled.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_error_disable(cvmx_error_register_t reg_type,
                        uint64_t status_addr, uint64_t status_mask)
{
    int found = 0;
    int i;
    uint64_t enable;
    for (i = 0; i < __cvmx_error_table_size; i++)
    {
        cvmx_error_info_t *h = &__cvmx_error_table[i];
        if (!REG_MATCH(h, reg_type, status_addr, status_mask) || !h->enable_addr)
            continue;
        enable = __cvmx_error_read_hw(h->reg_type, h->enable_addr);
        if (h->reg_type == CVMX_ERROR_REGISTER_PCICONFIG)
            enable |= h->enable_mask; /* PCI bits have reversed polarity */
        else
            enable &= ~h->enable_mask;
        __cvmx_error_write_hw(h->reg_type, h->enable_addr, enable);
        h->flags |= CVMX_ERROR_FLAGS_DISABLED;
        found = 1;
    }
    if (found)
        return 0;
    else
    {
        cvmx_warn("cvmx-error disable couldn't find requested register\n");
        return -1;
    }
}

