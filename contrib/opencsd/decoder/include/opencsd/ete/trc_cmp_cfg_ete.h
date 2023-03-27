/*
* \file       trc_cmp_cfg_ete.h
* \brief      OpenCSD : ETE configuration 
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

#ifndef ARM_TRC_CMP_CFG_ETE_H_INCLUDED
#define ARM_TRC_CMP_CFG_ETE_H_INCLUDED

#include "trc_pkt_types_ete.h"
#include "opencsd/etmv4/trc_cmp_cfg_etmv4.h"

/** @addtogroup ocsd_protocol_cfg
@{*/

/** @name ETE configuration
@{*/

/*!
 * @class ETEConfig
 * @brief Interpreter class for ETE config structure
 *
 * ETE trace and config are a superset of ETMv4 trace and config - hence 
 * use the EtmV4Config class as a base.
 */
class ETEConfig : public EtmV4Config
{
public:
    ETEConfig();
    ETEConfig(const ocsd_ete_cfg *cfg_regs);
    ~ETEConfig();

    //! copy assignment operator for base structure into class.
    ETEConfig & operator=(const ocsd_ete_cfg *p_cfg);

    //! cast operator returning struct const reference
    operator const ocsd_ete_cfg &() const { return m_ete_cfg; };
    //! cast operator returning struct const pointer
    operator const ocsd_ete_cfg *() const { return &m_ete_cfg; };

private:
    void copyV4();  // copy relevent config to underlying structure.

    ocsd_ete_cfg m_ete_cfg;
};


/** @}*/
/** @}*/

#endif // ARM_TRC_CMP_CFG_ETE_H_INCLUDED

/* End of File trc_cmp_cfg_ete.h */
