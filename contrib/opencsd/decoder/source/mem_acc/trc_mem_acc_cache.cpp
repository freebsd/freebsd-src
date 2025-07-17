/*!
* \file       trc_mem_acc_cache.cpp
* \brief      OpenCSD : Memory accessor cache.
*
* \copyright  Copyright (c) 2018, ARM Limited. All Rights Reserved.
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

#include <cstring>
#include <sstream>
#include <iomanip>
#include "mem_acc/trc_mem_acc_cache.h"
#include "mem_acc/trc_mem_acc_base.h"
#include "interfaces/trc_error_log_i.h"

#ifdef LOG_CACHE_STATS
#define INC_HITS_RL(idx) m_hits++; m_hit_rl[m_mru_idx]++;
#define INC_MISS() m_misses++;
#define INC_PAGES() m_pages++;
#define SET_MAX_RL(idx)                         \
    {                                           \
        if (m_hit_rl_max[idx] < m_hit_rl[idx])  \
            m_hit_rl_max[idx] = m_hit_rl[idx];  \
        m_hit_rl[idx] = 0;                      \
    }
#define INC_RL(idx) m_hit_rl[m_mru_idx]++;
#else
#define INC_HITS_RL(idx)
#define INC_MISS() 
#define INC_PAGES() 
#define SET_MAX_RL(idx)
#define INC_RL(idx)  
#endif

// uncomment to log cache ops
//#define LOG_CACHE_OPS

ocsd_err_t TrcMemAccCache::readBytesFromCache(TrcMemAccessorBase *p_accessor, const ocsd_vaddr_t address, const ocsd_mem_space_acc_t mem_space, const uint8_t trcID, uint32_t *numBytes, uint8_t *byteBuffer)
{
    uint32_t bytesRead = 0, reqBytes = *numBytes;
    ocsd_err_t err = OCSD_OK;

#ifdef LOG_CACHE_OPS
    std::ostringstream oss;
#endif

    if (m_bCacheEnabled)
    {
        if (blockInCache(address, reqBytes))
        {
            bytesRead = reqBytes;
            memcpy(byteBuffer, &m_mru[m_mru_idx].data[address - m_mru[m_mru_idx].st_addr], reqBytes);
#ifdef LOG_CACHE_OPS
            oss << "TrcMemAccCache:: hit [page: " << std::dec << m_mru_idx << "[addr:0x" << std::hex << address << ", bytes: " << std::dec << reqBytes << "]\n";
            logMsg(oss.str());
#endif
            INC_HITS_RL(m_mru_idx);
        }
        else
        {
            INC_MISS();
#ifdef LOG_CACHE_OPS
            oss << "TrcMemAccCache:: miss [addr:0x" << std::hex << address << ", bytes: " << std::dec << reqBytes << "]\n";
            logMsg(oss.str());
#endif
            /* need a new cache page - check the underlying accessor for the data */
            m_mru_idx = m_mru_next_new;             
            m_mru[m_mru_idx].valid_len = p_accessor->readBytes(address, mem_space, trcID, MEM_ACC_CACHE_PAGE_SIZE, &m_mru[m_mru_idx].data[0]);
            
            /* check return length valid - v bad if return length more than request */
            if (m_mru[m_mru_idx].valid_len > MEM_ACC_CACHE_PAGE_SIZE)
            {
                m_mru[m_mru_idx].valid_len = 0; // set to nothing returned.
                err = OCSD_ERR_MEM_ACC_BAD_LEN;
            }
            
            if (m_mru[m_mru_idx].valid_len > 0)
            {
                // got some data - so save the 
                m_mru[m_mru_idx].st_addr = address;

                // log the run length hit counts
                SET_MAX_RL(m_mru_idx);
                
#ifdef LOG_CACHE_OPS
                oss.str("");
                oss << "TrcMemAccCache:: load [page: " << std::dec << m_mru_idx << "[addr:0x" << std::hex << address << ", bytes: " << std::dec << m_mru[m_mru_idx].valid_len << "]\n";
                logMsg(oss.str());
#endif
                INC_PAGES();
                
                // increment the next new page counter.
                m_mru_next_new++;
                if (m_mru_next_new == MEM_ACC_CACHE_MRU_SIZE)
                    m_mru_next_new = 0;

                if (blockInPage(address, reqBytes)) /* check we got the data we needed */
                {
                    bytesRead = reqBytes;
                    memcpy(byteBuffer, &m_mru[m_mru_idx].data[address - m_mru[m_mru_idx].st_addr], reqBytes);
                    INC_RL(m_mru_idx);
                }
                else
                {
#ifdef LOG_CACHE_OPS
                    oss.str("");
                    oss << "TrcMemAccCache:: miss-after-load [page: " << std::dec << m_mru_idx << "[addr:0x" << std::hex << address << ", bytes: " << std::dec << m_mru[m_mru_idx].valid_len << "]\n";
                    logMsg(oss.str());
#endif
                    INC_MISS();
                }
            }
        }
    }
    *numBytes = bytesRead;
    return err;
}

void TrcMemAccCache::logMsg(const std::string &szMsg)
{
    if (m_err_log)
        m_err_log->LogMessage(ITraceErrorLog::HANDLE_GEN_INFO, OCSD_ERR_SEV_INFO, szMsg);
}

void TrcMemAccCache::setErrorLog(ITraceErrorLog *log)
{
    m_err_log = log;
}

void TrcMemAccCache::logAndClearCounts()
{
#ifdef LOG_CACHE_STATS
    std::ostringstream oss;

    oss << "TrcMemAccCache:: cache performance: hits(" << std::dec << m_hits << "), miss(" << m_misses << "), pages(" << m_pages << ")\n";
    logMsg(oss.str());
    for (int i = 0; i < MEM_ACC_CACHE_MRU_SIZE; i++)
    {
        if (m_hit_rl_max[i] < m_hit_rl[i])
            m_hit_rl_max[i] = m_hit_rl[i];
        oss.str("");
        oss << "Run length max page " << std::dec << i << ": " << m_hit_rl_max[i] << "\n";
        logMsg(oss.str());
    }
    m_hits = m_misses = m_pages = 0;
#endif
}


/* End of File trc_mem_acc_cache.cpp */
