/*
 * \file       trc_pkt_types_ete.h
 * \brief      OpenCSD : ETE types 
 *
 * \copyright  Copyright (c) 2019, ARM Limited. All Rights Reserved.
 */

/*
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ARM_TRC_PKT_TYPES_ETE_H_INCLUDED
#define ARM_TRC_PKT_TYPES_ETE_H_INCLUDED

#include "opencsd/trc_pkt_types.h"
#include "opencsd/etmv4/trc_pkt_types_etmv4.h"
 /** @addtogroup trc_pkts
 @{*/

 /** @name ETE config Types
 @{*/


typedef struct _ocsd_ete_cfg
{
    uint32_t                reg_idr0;       /**< ID0 register */
    uint32_t                reg_idr1;       /**< ID1 register */
    uint32_t                reg_idr2;       /**< ID2 register */
    uint32_t                reg_idr8;       /**< ID8 - maxspec */
    uint32_t                reg_devarch;    /**< DevArch register */
    uint32_t                reg_configr;    /**< Config Register */
    uint32_t                reg_traceidr;   /**< Trace Stream ID register */
    ocsd_arch_version_t    arch_ver;        /**< Architecture version */
    ocsd_core_profile_t    core_prof;       /**< Core Profile */
} ocsd_ete_cfg;


/** @}*/
/** @}*/

#endif  // ARM_TRC_PKT_TYPES_ETE_H_INCLUDED

/* End of File trc_pkt_types_ete.h */
