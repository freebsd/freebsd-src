/*
 * \file       trc_mem_acc_mapper.cpp
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

#include "mem_acc/trc_mem_acc_mapper.h"
#include "mem_acc/trc_mem_acc_file.h"
#include "common/ocsd_error.h"

/************************************************************************************/
/* mappers base class */
/************************************************************************************/

#define USING_MEM_ACC_CACHE

TrcMemAccMapper::TrcMemAccMapper() :
    m_acc_curr(0),
    m_trace_id_curr(0),
    m_using_trace_id(false),
    m_err_log(0)
{
#ifdef USING_MEM_ACC_CACHE
    m_cache.enableCaching(true);
#endif
}

TrcMemAccMapper::TrcMemAccMapper(bool using_trace_id) : 
    m_acc_curr(0),
    m_trace_id_curr(0),
    m_using_trace_id(using_trace_id),
    m_err_log(0)
{
#ifdef USING_MEM_ACC_CACHE
    m_cache.enableCaching(true);
#endif
}

TrcMemAccMapper::~TrcMemAccMapper()
{
}

void TrcMemAccMapper::setErrorLog(ITraceErrorLog *err_log_i)
{ 
    m_err_log = err_log_i; 
    m_cache.setErrorLog(err_log_i);
}

// memory access interface
ocsd_err_t TrcMemAccMapper::ReadTargetMemory(const ocsd_vaddr_t address, const uint8_t cs_trace_id, const ocsd_mem_space_acc_t mem_space, uint32_t *num_bytes, uint8_t *p_buffer)
{
    bool bReadFromCurr = true;
    uint32_t readBytes = 0;
    ocsd_err_t err = OCSD_OK;

    /* see if the address is in any range we know */
    if (!readFromCurrent(address, mem_space, cs_trace_id))
    {
        bReadFromCurr = findAccessor(address, mem_space, cs_trace_id);

        // found a new accessor - invalidate any cache entries used by the previous one.
        if (m_cache.enabled() && bReadFromCurr)
            m_cache.invalidateAll();
    }

    /* if bReadFromCurr then we know m_acc_curr is set */
    if (bReadFromCurr)
    {
        // use cache if enabled and the amount fits into a cache page
        if (m_cache.enabled_for_size(*num_bytes))
        {
            // read from cache - or load a new cache page and read....
            readBytes = *num_bytes;
            err = m_cache.readBytesFromCache(m_acc_curr, address, mem_space, cs_trace_id, &readBytes, p_buffer);
            if (err != OCSD_OK)
                LogWarn(err, "Mem Acc: Cache access error");
        }
        else
        {
            readBytes = m_acc_curr->readBytes(address, mem_space, cs_trace_id, *num_bytes, p_buffer);
            // guard against bad accessor returns (e.g. callback not obeying the rules for return values)
            if (readBytes > *num_bytes)
            {
                err = OCSD_ERR_MEM_ACC_BAD_LEN;
                LogWarn(err,"Mem acc: bad return length");
            }
        }
    }

    *num_bytes = readBytes;  
    return err;
}

void TrcMemAccMapper::InvalidateMemAccCache(const uint8_t /* cs_trace_id */)
{
    // default mapper does not use cs_trace_id for cache invalidation.
    if (m_cache.enabled())
        m_cache.invalidateAll();
    m_acc_curr = 0;
}

void TrcMemAccMapper::RemoveAllAccessors()
{
    TrcMemAccessorBase *pAcc = 0;
    pAcc = getFirstAccessor();
    while(pAcc != 0)
    {
        TrcMemAccFactory::DestroyAccessor(pAcc);
        pAcc = getNextAccessor();
        if (m_cache.enabled())
            m_cache.invalidateAll();
    }
    clearAccessorList();
    if (m_cache.enabled())
        m_cache.logAndClearCounts();
}

ocsd_err_t TrcMemAccMapper::RemoveAccessorByAddress(const ocsd_vaddr_t st_address, const ocsd_mem_space_acc_t mem_space, const uint8_t cs_trace_id /* = 0 */)
{
    ocsd_err_t err = OCSD_OK;
    if(findAccessor(st_address,mem_space,cs_trace_id))
    {
        err = RemoveAccessor(m_acc_curr);
        m_acc_curr = 0;
        if (m_cache.enabled())
            m_cache.invalidateAll();
    }
    else
        err = OCSD_ERR_INVALID_PARAM_VAL;
    if (m_cache.enabled())
        m_cache.logAndClearCounts();
    return err;
}

void  TrcMemAccMapper::LogMessage(const std::string &msg)
{
    if(m_err_log)
        m_err_log->LogMessage(ITraceErrorLog::HANDLE_GEN_INFO,OCSD_ERR_SEV_INFO,msg);
}

void TrcMemAccMapper::LogWarn(const ocsd_err_t err, const std::string &msg)
{
    if (m_err_log)
    {
        ocsdError err_ocsd(OCSD_ERR_SEV_WARN,err,msg);
        m_err_log->LogError(ITraceErrorLog::HANDLE_GEN_INFO, &err_ocsd);
    }
}


/************************************************************************************/
/* mappers global address space class - no differentiation in core trace IDs */
/************************************************************************************/
TrcMemAccMapGlobalSpace::TrcMemAccMapGlobalSpace() : TrcMemAccMapper()
{
}

TrcMemAccMapGlobalSpace::~TrcMemAccMapGlobalSpace()
{
}

ocsd_err_t TrcMemAccMapGlobalSpace::AddAccessor(TrcMemAccessorBase *p_accessor, const uint8_t /*cs_trace_id*/)
{
    ocsd_err_t err = OCSD_OK;
    bool bOverLap = false;

    if(!p_accessor->validateRange())
        return OCSD_ERR_MEM_ACC_RANGE_INVALID;

    std::vector<TrcMemAccessorBase *>::const_iterator it =  m_acc_global.begin();
    while((it != m_acc_global.end()) && !bOverLap)
    {
        // if overlap and memory space match
        if( ((*it)->overLapRange(p_accessor)) &&
            ((*it)->inMemSpace(p_accessor->getMemSpace()))
            )
        {
            bOverLap = true;
            err = OCSD_ERR_MEM_ACC_OVERLAP;
        }
        it++;
    }

    // no overlap - add to the list of ranges.
    if(!bOverLap)
        m_acc_global.push_back(p_accessor);

    return err;
}

bool TrcMemAccMapGlobalSpace::findAccessor(const ocsd_vaddr_t address, const ocsd_mem_space_acc_t mem_space, const uint8_t /*cs_trace_id*/)
{
    bool bFound = false;
    std::vector<TrcMemAccessorBase *>::const_iterator it =  m_acc_global.begin();
    while((it != m_acc_global.end()) && !bFound)
    {
        if( (*it)->addrInRange(address) &&
            (*it)->inMemSpace(mem_space))
        {
            bFound = true;
            m_acc_curr = *it;
        }
        it++;
    }
    return bFound;
}

bool TrcMemAccMapGlobalSpace::readFromCurrent(const ocsd_vaddr_t address, const ocsd_mem_space_acc_t mem_space, const uint8_t /*cs_trace_id*/)
{
    bool readFromCurr = false;
    if(m_acc_curr)
        readFromCurr = (m_acc_curr->addrInRange(address) && m_acc_curr->inMemSpace(mem_space));
    return readFromCurr;
}


TrcMemAccessorBase * TrcMemAccMapGlobalSpace::getFirstAccessor()
{
    TrcMemAccessorBase *p_acc = 0;
    m_acc_it = m_acc_global.begin();
    if(m_acc_it != m_acc_global.end())
    {
        p_acc = *m_acc_it;
    }
    return p_acc;
}

TrcMemAccessorBase *TrcMemAccMapGlobalSpace::getNextAccessor()
{
    TrcMemAccessorBase *p_acc = 0;
    m_acc_it++;
    if(m_acc_it != m_acc_global.end())
    {
        p_acc = *m_acc_it;
    }
    return p_acc;
}

void TrcMemAccMapGlobalSpace::clearAccessorList()
{
    m_acc_global.clear();
}

ocsd_err_t TrcMemAccMapGlobalSpace::RemoveAccessor(const TrcMemAccessorBase *p_accessor)
{
    bool bFound = false;
    TrcMemAccessorBase *p_acc = getFirstAccessor();
    while(p_acc != 0)
    {
        if(p_acc == p_accessor)
        {
            m_acc_global.erase(m_acc_it);
            TrcMemAccFactory::DestroyAccessor(p_acc);
            p_acc = 0;
            bFound = true;
        }
        else
            p_acc = getNextAccessor();
    }
    return bFound ? OCSD_OK : OCSD_ERR_INVALID_PARAM_VAL;
}


void TrcMemAccMapGlobalSpace::logMappedRanges()
{
    std::string accStr;
    TrcMemAccessorBase *pAccessor = getFirstAccessor();
    LogMessage("Mapped Memory Accessors\n");
    while(pAccessor != 0)
    {
        pAccessor->getMemAccString(accStr);
        accStr += "\n";
        LogMessage(accStr);
        pAccessor = getNextAccessor();
    }
    LogMessage("========================\n");
}

/* End of File trc_mem_acc_mapper.cpp */
