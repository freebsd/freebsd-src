/*
 * \file       trc_pkt_proc_etmv3.cpp
 * \brief      OpenCSD : 
 * 
 * \copyright  Copyright (c) 2015, ARM Limited. All Rights Reserved.
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

#include "opencsd/etmv3/trc_pkt_proc_etmv3.h"
#include "trc_pkt_proc_etmv3_impl.h"
#include "common/ocsd_error.h"

#ifdef __GNUC__
// G++ doesn't like the ## pasting
#define ETMV3_PKTS_NAME "PKTP_ETMV3"
#else
#define ETMV3_PKTS_NAME OCSD_CMPNAME_PREFIX_PKTPROC##"_"##OCSD_BUILTIN_DCD_ETMV3
#endif 

static const uint32_t ETMV3_SUPPORTED_OP_FLAGS = OCSD_OPFLG_PKTPROC_COMMON |
    ETMV3_OPFLG_UNFORMATTED_SOURCE;
    
TrcPktProcEtmV3::TrcPktProcEtmV3() : TrcPktProcBase(ETMV3_PKTS_NAME), 
    m_pProcessor(0)
{
    m_supported_op_flags = ETMV3_SUPPORTED_OP_FLAGS;
}

TrcPktProcEtmV3::TrcPktProcEtmV3(int instIDNum) : TrcPktProcBase(ETMV3_PKTS_NAME, instIDNum),
    m_pProcessor(0)
{
    m_supported_op_flags = ETMV3_SUPPORTED_OP_FLAGS;
}

TrcPktProcEtmV3::~TrcPktProcEtmV3()
{
    if(m_pProcessor)
        delete m_pProcessor;
    m_pProcessor = 0;
}

ocsd_err_t TrcPktProcEtmV3::onProtocolConfig()
{
    if(m_pProcessor == 0)
    {
        m_pProcessor = new (std::nothrow) EtmV3PktProcImpl();
        if(m_pProcessor == 0)
        {           
            LogError(ocsdError(OCSD_ERR_SEV_ERROR,OCSD_ERR_MEM));
            return OCSD_ERR_MEM;
        }
        m_pProcessor->Initialise(this);
    }
    return m_pProcessor->Configure(m_config);
}

ocsd_datapath_resp_t TrcPktProcEtmV3::processData(  const ocsd_trc_index_t index,
                                                const uint32_t dataBlockSize,
                                                const uint8_t *pDataBlock,
                                                uint32_t *numBytesProcessed)
{
    if(m_pProcessor)
        return m_pProcessor->processData(index,dataBlockSize,pDataBlock,numBytesProcessed);
    return OCSD_RESP_FATAL_NOT_INIT;
}

ocsd_datapath_resp_t TrcPktProcEtmV3::onEOT()
{
    if(m_pProcessor)
        return m_pProcessor->onEOT();
    return OCSD_RESP_FATAL_NOT_INIT;
}

ocsd_datapath_resp_t TrcPktProcEtmV3::onReset()
{
    if(m_pProcessor)
        return m_pProcessor->onReset();
    return OCSD_RESP_FATAL_NOT_INIT;
}

ocsd_datapath_resp_t TrcPktProcEtmV3::onFlush()
{
    if(m_pProcessor)
        return m_pProcessor->onFlush();
    return OCSD_RESP_FATAL_NOT_INIT;
}

const bool TrcPktProcEtmV3::isBadPacket() const 
{
    if(m_pProcessor)
        return m_pProcessor->isBadPacket();
    return false;
}

/* End of File trc_pkt_proc_etmv3.cpp */
