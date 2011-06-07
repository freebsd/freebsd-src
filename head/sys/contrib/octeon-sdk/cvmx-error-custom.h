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
 * Prototypes for custom error handler function not handled by the default
 * message display error function.
 *
 * <hr>$Revision: 44252 $<hr>
 */
#ifndef __CVMX_ERROR_CUSTOM_H__
#define __CVMX_ERROR_CUSTOM_H__

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * @INTERNAL
 * Some errors require more complicated error handing functions
 * than the automatically generated functions in cvmx-error-init-*.c.
 * This function replaces these handers with hand coded functions
 * for these special cases.
 *
 * @return Zero on success, negative on failure.
 */
int __cvmx_error_custom_initialize(void);

int __cvmx_error_handle_dfa_err_cp2dbe(const struct cvmx_error_info *info);
int __cvmx_error_handle_dfa_err_cp2perr(const struct cvmx_error_info *info);
int __cvmx_error_handle_dfa_err_cp2sbe(const struct cvmx_error_info *info);
int __cvmx_error_handle_dfa_err_dblovf(const struct cvmx_error_info *info);
int __cvmx_error_handle_dfa_err_dtedbe(const struct cvmx_error_info *info);
int __cvmx_error_handle_dfa_err_dteperr(const struct cvmx_error_info *info);
int __cvmx_error_handle_dfa_err_dtesbe(const struct cvmx_error_info *info);
int __cvmx_error_handle_l2d_err_ded_err(const struct cvmx_error_info *info);
int __cvmx_error_handle_l2d_err_sec_err(const struct cvmx_error_info *info);
int __cvmx_error_handle_l2t_err_ded_err(const struct cvmx_error_info *info);
int __cvmx_error_handle_l2t_err_lckerr2(const struct cvmx_error_info *info);
int __cvmx_error_handle_l2t_err_lckerr(const struct cvmx_error_info *info);
int __cvmx_error_handle_l2t_err_sec_err(const struct cvmx_error_info *info);
int __cvmx_error_handle_lmcx_mem_cfg0_ded_err(const struct cvmx_error_info *info);
int __cvmx_error_handle_lmcx_mem_cfg0_sec_err(const struct cvmx_error_info *info);
int __cvmx_error_handle_pow_ecc_err_dbe(const struct cvmx_error_info *info);
int __cvmx_error_handle_pow_ecc_err_iop(const struct cvmx_error_info *info);
int __cvmx_error_handle_pow_ecc_err_rpe(const struct cvmx_error_info *info);
int __cvmx_error_handle_pow_ecc_err_sbe(const struct cvmx_error_info *info);

#ifdef	__cplusplus
}
#endif

#endif
