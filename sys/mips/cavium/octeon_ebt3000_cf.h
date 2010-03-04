/***********************license start***************
 *  Copyright (c) 2003-2008 Cavium Networks (support@cavium.com). All rights
 *  reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Cavium Networks nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 *  TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 *  REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 *  DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 *  PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 *  POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 *  OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 *  For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/

/* $FreeBSD$ */

/*
 *  octeon_ebt3000_cf.h
 *
 */


#ifndef  __OCTEON_EBT3000_H__
#define  __OCTEON_EBT3000_H__



#define OCTEON_CF_COMMON_BASE_ADDR		(0x1d000000 | (1 << 11))
#define OCTEON_MIO_BOOT_REG_CFGX(offset)	(0x8001180000000000ull + ((offset) * 8))


typedef union
{   
    uint64_t	word64;
    struct
    {
        uint64_t reserved                : 27;      /**< Reserved */
        uint64_t sam                     : 1;       /**< Region 0 SAM */
        uint64_t we_ext                  : 2;       /**< Region 0 write enable count extension */
        uint64_t oe_ext                  : 2;       /**< Region 0 output enable count extension */
        uint64_t en                      : 1;       /**< Region 0 enable */
        uint64_t orbit                   : 1;       /**< No function for region 0 */
        uint64_t ale                     : 1;       /**< Region 0 ALE mode */
        uint64_t width                   : 1;       /**< Region 0 bus width */
        uint64_t size                    : 12;      /**< Region 0 size */
        uint64_t base                    : 16;      /**< Region 0 base address */
    } bits;
} octeon_mio_boot_reg_cfgx_t;


#endif  /* __OCTEON_EBT3000_H__ */
