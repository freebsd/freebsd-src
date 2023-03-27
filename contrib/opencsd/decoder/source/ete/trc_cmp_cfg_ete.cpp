/*
* \file       trc_cmp_cfg_ete.cpp
* \brief      OpenCSD : ETE config class
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

#include "opencsd/ete/trc_cmp_cfg_ete.h"

ETEConfig::ETEConfig() : EtmV4Config()
{
    m_ete_cfg.reg_idr0 = 0x28000EA1;
    m_ete_cfg.reg_idr1 = 0x4100FFF3;
    m_ete_cfg.reg_idr2 = 0x00000488;
    m_ete_cfg.reg_idr8 = 0;
    m_ete_cfg.reg_configr = 0xC1;
    m_ete_cfg.reg_traceidr = 0;
    m_ete_cfg.arch_ver = ARCH_AA64;
    m_ete_cfg.core_prof = profile_CortexA;
    m_ete_cfg.reg_devarch = 0x47705A13;
    copyV4();
}

ETEConfig::ETEConfig(const ocsd_ete_cfg *cfg_regs) : EtmV4Config()
{
    m_ete_cfg = *cfg_regs;
    copyV4();
}

ETEConfig::~ETEConfig()
{

}

//! copy assignment operator for base structure into class.
ETEConfig & ETEConfig::operator=(const ocsd_ete_cfg *p_cfg)
{
    m_ete_cfg = *p_cfg;
    copyV4();
    return *this;
}

//! cast operator returning struct const reference
//operator const ocsd_ete_cfg &() const { return m_ete_cfg; };
//! cast operator returning struct const pointer
//operator const ocsd_ete_cfg *() const { return &m_ete_cfg; };

// ete superset of etmv4 - move info to underlying structure.
void ETEConfig::copyV4()
{
    // copy over 1:1 regs
    m_cfg.reg_idr0 = m_ete_cfg.reg_idr0;
    m_cfg.reg_idr1 = m_ete_cfg.reg_idr1;
    m_cfg.reg_idr2 = m_ete_cfg.reg_idr2;
    m_cfg.reg_idr8 = m_ete_cfg.reg_idr8;
    m_cfg.reg_idr9 = 0;
    m_cfg.reg_idr10 = 0;
    m_cfg.reg_idr11 = 0;
    m_cfg.reg_idr12 = 0;
    m_cfg.reg_idr13 = 0;
    m_cfg.reg_configr = m_ete_cfg.reg_configr;
    m_cfg.reg_traceidr = m_ete_cfg.reg_traceidr;
    m_cfg.core_prof = m_ete_cfg.core_prof;
    m_cfg.arch_ver = m_ete_cfg.arch_ver;

    // override major / minor version as part of devarch
    m_MajVer = (uint8_t)((m_ete_cfg.reg_devarch & 0xF000) >> 12);
    m_MinVer = (uint8_t)((m_ete_cfg.reg_devarch & 0xF0000) >> 16);
}

/* End of File trc_cmp_cfg_ete.cpp */
