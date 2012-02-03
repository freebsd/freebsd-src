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
#ifndef __CVMX_ERROR_H__
#define __CVMX_ERROR_H__

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * There are generally many error status bits associated with a
 * single logical group. The enumeration below is used to
 * communicate high level groups to the error infastructure so
 * error status bits can be enable or disabled in large groups.
 */
typedef enum
{
    CVMX_ERROR_GROUP_INTERNAL,  /* All internal blocks that can always be enabled */
    CVMX_ERROR_GROUP_ETHERNET,  /* All errors related to network traffic that should be enabled when a port is up. Indexed by IPD number */
    CVMX_ERROR_GROUP_MGMT_PORT, /* All errors related to the management ethernet ports that should be enabled when a port is up. Indexed by port number (0-1) */
    CVMX_ERROR_GROUP_PCI,       /* All errors related to PCI/PCIe when the bus is up. Index by port number (0-1) */
    CVMX_ERROR_GROUP_SRIO,      /* All errors related to SRIO when the bus is up. Index by port number (0-1) */
    CVMX_ERROR_GROUP_USB,       /* All errors related to USB when the port is enabled. Index by port number (0-1) */
    CVMX_ERROR_GROUP_LMC,       /* All errors related to LMC when the controller is enabled. Index by controller number (0-1) */
} cvmx_error_group_t;

/**
 * When registering for interest in an error status register, the
 * type of the register needs to be known by cvmx-error. Most
 * registers are either IO64 or IO32, but some blocks contain
 * registers that can't be directly accessed. A good example of
 * would be PCIe extended error state stored in config space.
 */
typedef enum
{
    __CVMX_ERROR_REGISTER_NONE,     /* Used internally */
    CVMX_ERROR_REGISTER_IO64,       /* Status and enable registers are Octeon 64bit CSRs */
    CVMX_ERROR_REGISTER_IO32,       /* Status and enable registers are Octeon 32bit CSRs */
    CVMX_ERROR_REGISTER_PCICONFIG,  /* Status and enable registers are in PCI config space */
    CVMX_ERROR_REGISTER_SRIOMAINT,  /* Status and enable registers are in SRIO maintenance space */
} cvmx_error_register_t;

/**
 * Flags representing special handling for some error registers.
 * These flags are passed to cvmx_error_initialize() to control
 * the handling of bits where the same flags were passed to the
 * added cvmx_error_info_t.
 */
typedef enum
{
    CVMX_ERROR_FLAGS_ECC_SINGLE_BIT = 1<<0, /* This is a ECC single bit error. Normally these can be ignored */
    CVMX_ERROR_FLAGS_CORRECTABLE = 1<<1,    /* Some blocks have errors that can be silently corrected. This flags reports these */
    CVMX_ERROR_FLAGS_DISABLED = 1<<2,       /* Flag used to signal a register should not be enable as part of the groups */
} cvmx_error_flags_t;

struct cvmx_error_info;
/**
 * Error handling functions must have the following prototype.
 */
typedef int (*cvmx_error_func_t)(const struct cvmx_error_info *info);

/**
 * This structure is passed to all error handling functions.
 */
typedef struct cvmx_error_info
{
    cvmx_error_register_t   reg_type;       /* Type of registers used for the error */
    uint64_t                status_addr;    /* The address of the status register */
    uint64_t                status_mask;    /* Mask to apply to status register to detect asserted error */
    uint64_t                enable_addr;    /* The address of the enable register */
    uint64_t                enable_mask;    /* Mask to apply to enable register to allow error detection */
    cvmx_error_flags_t      flags;          /* Flags indicating special handling of this error */
    cvmx_error_group_t      group;          /* Group to associate this error register with */
    int                     group_index;    /* Group index to associate this error register with */
    cvmx_error_func_t       func;           /* Function to call when the error is detected */
    uint64_t                user_info;      /* User supplied information for the error handler */
    struct
    {
        cvmx_error_register_t reg_type;     /* Type of parent's register */
        uint64_t            status_addr;    /* The address of the parent's register */
        uint64_t            status_mask;    /* Mask to apply to parent's register */
    } parent;
} cvmx_error_info_t;

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
int cvmx_error_initialize(cvmx_error_flags_t flags);

/**
 * Poll the error status registers and call the appropriate error
 * handlers. This should be called in the RSL interrupt handler
 * for your application or operating system.
 *
 * @return Number of error handlers called. Zero means this call
 *         found no errors and was spurious.
 */
int cvmx_error_poll(void);

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
int cvmx_error_add(const cvmx_error_info_t *new_info);

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
                        cvmx_error_info_t *old_info);

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
                        cvmx_error_func_t *old_func, uint64_t *old_user_info);

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
#ifndef CVMX_BUILD_FOR_UBOOT
int cvmx_error_enable_group(cvmx_error_group_t group, int group_index);
#else
/* Rather than conditionalize the calls throughout the executive to not enable
    interrupts in Uboot, simply make the enable function do nothing */
static inline int cvmx_error_enable_group(cvmx_error_group_t group, int group_index)
{
    return 0;
}
#endif

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
#ifndef CVMX_BUILD_FOR_UBOOT
int cvmx_error_disable_group(cvmx_error_group_t group, int group_index);
#else
/* Rather than conditionalize the calls throughout the executive to not disable
    interrupts in Uboot, simply make the enable function do nothing */
static inline int cvmx_error_disable_group(cvmx_error_group_t group, int group_index)
{
    return 0;
}
#endif

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
                        uint64_t status_addr, uint64_t status_mask);

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
                        uint64_t status_addr, uint64_t status_mask);

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
int __cvmx_error_decode(const cvmx_error_info_t *info);

/**
 * @INTERNAL
 * This error bit handler simply prints a message and clears the status bit
 *
 * @param info   Error register to check
 *
 * @return
 */
int __cvmx_error_display(const cvmx_error_info_t *info);

#ifdef	__cplusplus
}
#endif

#endif
