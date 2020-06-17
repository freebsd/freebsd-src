/*
* \file       trc_raw_buffer.h
* \brief      OpenCSD : Trace raw data byte buffer
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

#ifndef ARM_TRC_RAW_BUFFER_H_INCLUDED
#define ARM_TRC_RAW_BUFFER_H_INCLUDED

#include <vector>

class TraceRawBuffer
{
public:
    TraceRawBuffer() : 
        m_bufSize(0), 
        m_bufProcessed(0), 
        m_pBuffer(0), 
        pkt(0)
    {};
    ~TraceRawBuffer() {};

    // init the buffer
    void init(const uint32_t size, const uint8_t *rawtrace, std::vector<uint8_t> *out_packet);
    void copyByteToPkt();   // move a byte to the packet buffer     
    uint8_t peekNextByte(); // value of next byte in buffer.

    bool empty() { return m_bufProcessed == m_bufSize; };
    // bytes processed.
    uint32_t processed() { return m_bufProcessed; };
    // buffer size;
    uint32_t size() { return m_bufSize; }

private:
    uint32_t m_bufSize;
    uint32_t m_bufProcessed;
    const uint8_t *m_pBuffer;
    std::vector<uint8_t> *pkt;

};

// init the buffer
inline void TraceRawBuffer::init(const uint32_t size, const uint8_t *rawtrace, std::vector<uint8_t> *out_packet)
{
    m_bufSize = size;
    m_bufProcessed = 0;
    m_pBuffer = rawtrace;
    pkt = out_packet;
}

inline void TraceRawBuffer::copyByteToPkt()
{
    if (!empty()) {
        pkt->push_back(m_pBuffer[m_bufProcessed]);
        m_bufProcessed++;
    }
}

inline uint8_t TraceRawBuffer::peekNextByte()
{
    uint8_t val = 0;
    if (!empty())
        val = m_pBuffer[m_bufProcessed];
    return val;
}

#endif // ARM_TRC_RAW_BUFFER_H_INCLUDED

