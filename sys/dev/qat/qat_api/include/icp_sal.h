/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 ***************************************************************************
 * @file icp_sal.h
 *
 * @ingroup SalCommon
 *
 * Functions for both user space and kernel space.
 *
 ***************************************************************************/

#ifndef ICP_SAL_H
#define ICP_SAL_H

/*
 * icp_sal_get_dc_error
 *
 * @description:
 *  This function returns the occurrences of compression errors specified
 *  in the input parameter
 *
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 * @param[in] dcError                DC Error Type
 *
 * returns                           Number of failing requests of type dcError
 */
Cpa64U icp_sal_get_dc_error(Cpa8S dcError);

#endif
