/*!
* \file       trc_mem_acc_cache.h
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

#ifndef ARM_TRC_MEM_ACC_CACHE_H_INCLUDED
#define ARM_TRC_MEM_ACC_CACHE_H_INCLUDED

#include <string>
#include "opencsd/ocsd_if_types.h"

#define MEM_ACC_CACHE_PAGE_SIZE 256
#define MEM_ACC_CACHE_MRU_SIZE 12

class TrcMemAccessorBase;
class ITraceErrorLog;

typedef struct cache_block {
    ocsd_vaddr_t st_addr;
    uint32_t valid_len;
    uint8_t data[MEM_ACC_CACHE_PAGE_SIZE];
} cache_block_t;

// enable define to collect stats for debugging / cache performance tests
//#define LOG_CACHE_STATS


/** class TrcMemAccCache - cache small amounts of data from accessors to speed up decode. */
class TrcMemAccCache
{
public:
    TrcMemAccCache();
    ~TrcMemAccCache() {};

    void enableCaching(bool bEnable) { m_bCacheEnabled = bEnable; };
    void invalidateAll();
    const bool enabled() const { return m_bCacheEnabled; };
    const bool enabled_for_size(const uint32_t reqSize) const
    {
        return (m_bCacheEnabled && (reqSize <= MEM_ACC_CACHE_PAGE_SIZE));
    }
        
    
    /** read bytes from cache if possible - load new page if needed, bail out if data not available */
    ocsd_err_t readBytesFromCache(TrcMemAccessorBase *p_accessor, const ocsd_vaddr_t address, const ocsd_mem_space_acc_t mem_space, const uint8_t trcID, uint32_t *numBytes, uint8_t *byteBuffer);

    void setErrorLog(ITraceErrorLog *log);
    void logAndClearCounts();

private:
    bool blockInCache(const ocsd_vaddr_t address, const uint32_t reqBytes); // run through each page to look for data.
    bool blockInPage(const ocsd_vaddr_t address, const uint32_t reqBytes);
    void logMsg(const std::string &szMsg);

    cache_block_t m_mru[MEM_ACC_CACHE_MRU_SIZE];
    int m_mru_idx = 0;  // in use index
    int m_mru_next_new = 0; // next new page at this index.
    bool m_bCacheEnabled = false;

#ifdef LOG_CACHE_STATS    
    uint32_t m_hits = 0;
    uint32_t m_misses = 0;
    uint32_t m_pages = 0;
    uint32_t m_hit_rl[MEM_ACC_CACHE_MRU_SIZE];
    uint32_t m_hit_rl_max[MEM_ACC_CACHE_MRU_SIZE];
#endif
    
    ITraceErrorLog *m_err_log = 0;
};

inline TrcMemAccCache::TrcMemAccCache()
{
    for (int i = 0; i < MEM_ACC_CACHE_MRU_SIZE; i++)
    {
        m_mru[i].st_addr = 0;
        m_mru[i].valid_len = 0;
#ifdef LOG_CACHE_STATS
        m_hit_rl[i] = 0;
        m_hit_rl_max[i] = 0;
#endif
    }
}

inline bool TrcMemAccCache::blockInPage(const ocsd_vaddr_t address, const uint32_t reqBytes)
{
    if ((m_mru[m_mru_idx].st_addr <= address) &&
        m_mru[m_mru_idx].st_addr + m_mru[m_mru_idx].valid_len >= (address + reqBytes))
        return true;
    return false;
}

inline bool TrcMemAccCache::blockInCache(const ocsd_vaddr_t address, const uint32_t reqBytes)
{
    int tests = MEM_ACC_CACHE_MRU_SIZE;
    while (tests)
    {        
        if (blockInPage(address, reqBytes))
            return true; // found address in page
        tests--;
        m_mru_idx++;
        if (m_mru_idx == MEM_ACC_CACHE_MRU_SIZE)
            m_mru_idx = 0;
    }
    return false;
}

inline void TrcMemAccCache::invalidateAll()
{
    for (int i = 0; i < MEM_ACC_CACHE_MRU_SIZE; i++)
    {
        m_mru[i].valid_len = 0;
        m_mru[i].st_addr = 0;
    }
    m_mru_idx = 0;
    m_mru_next_new = 0;
}

#endif // ARM_TRC_MEM_ACC_CACHE_H_INCLUDED

/* End of File trc_mem_acc_cache.h */
