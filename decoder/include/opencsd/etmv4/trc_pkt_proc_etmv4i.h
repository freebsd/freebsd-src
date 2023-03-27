/*
 * \file       trc_pkt_proc_etmv4i.h
 * \brief      OpenCSD : Implementation of ETMv4 packet processing
 * 
 * \copyright  Copyright (c) 2015, 2019 ARM Limited. All Rights Reserved.
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

#ifndef ARM_TRC_PKT_PROC_ETMV4I_IMPL_H_INCLUDED
#define ARM_TRC_PKT_PROC_ETMV4I_IMPL_H_INCLUDED

#include "trc_pkt_types_etmv4.h"
#include "opencsd/etmv4/trc_pkt_proc_etmv4.h"
#include "opencsd/etmv4/trc_cmp_cfg_etmv4.h"
#include "opencsd/etmv4/trc_pkt_elem_etmv4i.h"
#include "common/trc_raw_buffer.h"
#include "common/trc_pkt_proc_base.h"

class EtmV4ITrcPacket;
class EtmV4Config;

/** @addtogroup ocsd_pkt_proc
@{*/

class TrcPktProcEtmV4I : public TrcPktProcBase< EtmV4ITrcPacket, ocsd_etmv4_i_pkt_type, EtmV4Config>
{
public:
    TrcPktProcEtmV4I();
    TrcPktProcEtmV4I(int instIDNum);
    virtual ~TrcPktProcEtmV4I();

protected:
    /* implementation packet processing interface */
    virtual ocsd_datapath_resp_t processData(  const ocsd_trc_index_t index,
                                        const uint32_t dataBlockSize,
                                        const uint8_t *pDataBlock,
                                        uint32_t *numBytesProcessed);
    virtual ocsd_datapath_resp_t onEOT();
    virtual ocsd_datapath_resp_t onReset();
    virtual ocsd_datapath_resp_t onFlush();
    virtual ocsd_err_t onProtocolConfig();
    virtual const bool isBadPacket() const;

protected:
    typedef enum _process_state {
        PROC_HDR,
        PROC_DATA,
        SEND_PKT,
        SEND_UNSYNCED,
        PROC_ERR,
    } process_state;
    
    process_state m_process_state;

    void InitPacketState();      // clear current packet state.
    void InitProcessorState();   // clear all previous process state

    /** packet processor configuration **/
    bool m_isInit;

    // etmv4 hardware configuration
    EtmV4Config m_config;

    /** packet data **/
    TraceRawBuffer m_trcIn;    // trace data in buffer
    std::vector<uint8_t> m_currPacketData;  // raw data packet
    int m_currPktIdx;   // index into raw packet when expanding
    EtmV4ITrcPacket m_curr_packet;  // expanded packet
    ocsd_trc_index_t m_packet_index;   // index of the start of the current packet
    ocsd_trc_index_t m_blockIndex;     // index at the start of the current data block being processed

    // searching for sync
    bool m_is_sync;             //!< seen first sync packet
    bool m_first_trace_info;    //!< seen first trace info packet after sync
    bool m_sent_notsync_packet; //!< send one not sync packet if we see any unsynced data on the channel 
    unsigned m_dump_unsynced_bytes;  //!< number of unsynced bytes to send
    ocsd_trc_index_t m_update_on_unsync_packet_index;


private:
    // current processing state data - counts and flags to determine if a packet is complete.

    // TraceInfo Packet
    // flags to indicate processing progress for these sections is complete.
    struct _t_info_pkt_prog {
        uint8_t sectFlags;
        uint8_t ctrlBytes;                 
    }  m_tinfo_sections;

    #define TINFO_INFO_SECT 0x01
    #define TINFO_KEY_SECT  0x02 
    #define TINFO_SPEC_SECT 0x04
    #define TINFO_CYCT_SECT 0x08
    #define TINFO_WNDW_SECT 0x10
    #define TINFO_CTRL      0x20
    #define TINFO_ALL_SECT  0x1F
    #define TINFO_ALL       0x3F


    // address and context packets 
    int m_addrBytes;
    uint8_t m_addrIS;
    bool m_bAddr64bit;
    int m_vmidBytes;    // bytes still to find
    int m_ctxtidBytes;  // bytes still to find
    bool m_bCtxtInfoDone;
    bool m_addr_done;

    // timestamp
    bool m_ccount_done; // done or not needed 
    bool m_ts_done;
    int m_ts_bytes;

    // exception
    int m_excep_size;

    // cycle count
    bool m_has_count;
    bool m_count_done;
    bool m_commit_done;

    // cond result
    bool m_F1P1_done;  // F1 payload 1 done
    bool m_F1P2_done;  // F1 payload 2 done
    bool m_F1has_P2;   // F1 has a payload 2

    // Q packets (use some from above too)
    bool m_has_addr;
    bool m_addr_short;
    bool m_addr_match;
    uint8_t m_Q_type;
    uint8_t m_QE;

    ocsd_datapath_resp_t outputPacket();
    ocsd_datapath_resp_t outputUnsyncedRawPacket(); 

    void iNotSync(const uint8_t lastByte);      // not synced yet
    void iPktNoPayload(const uint8_t lastByte); // process a single byte packet
    void iPktReserved(const uint8_t lastByte);  // deal with reserved header value
    void iPktExtension(const uint8_t lastByte);
    void iPktASync(const uint8_t lastByte);
    void iPktTraceInfo(const uint8_t lastByte);
    void iPktTimestamp(const uint8_t lastByte);
    void iPktException(const uint8_t lastByte);
    void iPktCycleCntF123(const uint8_t lastByte);
    void iPktSpeclRes(const uint8_t lastByte);
    void iPktCondInstr(const uint8_t lastByte);
    void iPktCondResult(const uint8_t lastByte);
    void iPktContext(const uint8_t lastByte);
    void iPktAddrCtxt(const uint8_t lastByte);
    void iPktShortAddr(const uint8_t lastByte);
    void iPktLongAddr(const uint8_t lastByte);
    void iPktQ(const uint8_t lastByte);
    void iAtom(const uint8_t lastByte);
    void iPktInvalidCfg(const uint8_t lastByte);  // packet invalid in current config.
    void iPktITE(const uint8_t lastByte);

    unsigned extractContField(const std::vector<uint8_t> &buffer, const unsigned st_idx, uint32_t &value, const unsigned byte_limit = 5);
    unsigned extractTSField64(const std::vector<uint8_t> &buffer, const unsigned st_idx, uint64_t &value);
    unsigned extractCondResult(const std::vector<uint8_t> &buffer, const unsigned st_idx, uint32_t& key, uint8_t &result);
    void extractAndSetContextInfo(const std::vector<uint8_t> &buffer, const int st_idx);
    int extract64BitLongAddr(const std::vector<uint8_t> &buffer, const int st_idx, const uint8_t IS, uint64_t &value);
    int extract32BitLongAddr(const std::vector<uint8_t> &buffer, const int st_idx, const uint8_t IS, uint32_t &value);
    int extractShortAddr(const std::vector<uint8_t> &buffer, const int st_idx, const uint8_t IS, uint32_t &value, int &bits);

    // packet processing is table driven.    
    typedef void (TrcPktProcEtmV4I::*PPKTFN)(uint8_t);
    PPKTFN m_pIPktFn;

    struct _pkt_i_table_t {
        ocsd_etmv4_i_pkt_type pkt_type;
        PPKTFN pptkFn;
    } m_i_table[256];

    void BuildIPacketTable();

    void throwBadSequenceError(const char *pszExtMsg);
};


inline const bool TrcPktProcEtmV4I::isBadPacket() const
{
    return m_curr_packet.isBadPacket();
}

/** @}*/

#endif // ARM_TRC_PKT_PROC_ETMV4I_IMPL_H_INCLUDED

/* End of File trc_pkt_proc_etmv4i_impl.h */
