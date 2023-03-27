/*
 * \file       trc_pkt_decode_etmv4i.h
 * \brief      OpenCSD : ETMv4 instruction decoder
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

#ifndef ARM_TRC_PKT_DECODE_ETMV4I_H_INCLUDED
#define ARM_TRC_PKT_DECODE_ETMV4I_H_INCLUDED

#include "common/trc_pkt_decode_base.h"
#include "opencsd/etmv4/trc_pkt_elem_etmv4i.h"
#include "opencsd/etmv4/trc_cmp_cfg_etmv4.h"
#include "common/trc_gen_elem.h"
#include "common/trc_ret_stack.h"
#include "common/ocsd_gen_elem_stack.h"
#include "opencsd/etmv4/trc_etmv4_stack_elem.h"

class TrcStackElem;
class TrcStackElemParam;
class TrcStackElemCtxt;

class TrcPktDecodeEtmV4I : public TrcPktDecodeBase<EtmV4ITrcPacket, EtmV4Config>
{
public:
    TrcPktDecodeEtmV4I();
    TrcPktDecodeEtmV4I(int instIDNum);
    virtual ~TrcPktDecodeEtmV4I();

protected:
    /* implementation packet decoding interface */
    virtual ocsd_datapath_resp_t processPacket();
    virtual ocsd_datapath_resp_t onEOT();
    virtual ocsd_datapath_resp_t onReset();
    virtual ocsd_datapath_resp_t onFlush();
    virtual ocsd_err_t onProtocolConfig();
    virtual const uint8_t getCoreSightTraceID() { return m_CSID; };

    /* local decode methods */
    void initDecoder();      // initial state on creation (zeros all config)
    void resetDecoder();     // reset state to start of decode. (moves state, retains config)
    virtual void onFirstInitOK(); // override to set init related info.

    ocsd_err_t decodePacket();    // decode packet into trace elements. return true to indicate decode complete - can change FSM to commit state - return is false.
    ocsd_datapath_resp_t resolveElements();   // commit/cancel trace elements generated from latest / prior packets & send to output - may get wait response, or flag completion.
    ocsd_err_t commitElements(); // commit elements - process element stack to generate output packets.
    ocsd_err_t commitElemOnEOT();
    ocsd_err_t cancelElements();    // cancel elements. These not output  
    ocsd_err_t mispredictAtom();    // mispredict an atom
    ocsd_err_t discardElements();   // discard elements and flush

    void doTraceInfoPacket();
    void updateContext(TrcStackElemCtxt *pCtxtElem, OcsdTraceElement &elem);
    
    // process atom will create instruction trace, or no memory access trace output elements. 
    ocsd_err_t processAtom(const ocsd_atm_val atom);

    // process an exception element - output instruction trace + exception generic type.
    ocsd_err_t processException(); 

    // process Q element
    ocsd_err_t processQElement();

    // process a source address element
    ocsd_err_t processSourceAddress();

    // process an element that cannot be cancelled / discarded
    ocsd_err_t processTS_CC_EventElem(TrcStackElem *pElem); 

    // process marker elements
    ocsd_err_t processMarkerElem(TrcStackElem *pElem);

    // process a transaction element
    ocsd_err_t processTransElem(TrcStackElem *pElem);

    // process an Instrumentation element
    ocsd_err_t processITEElem(TrcStackElem *pElem);

    // process a bad packet
    ocsd_err_t handleBadPacket(const char *reason, ocsd_trc_index_t index = OCSD_BAD_TRC_INDEX);

    // sequencing error on packet processing - optionally continue
    ocsd_err_t handlePacketSeqErr(ocsd_err_t err, ocsd_trc_index_t index, const char *reason);

    // common packet error routine
    ocsd_err_t handlePacketErr(ocsd_err_t err, ocsd_err_severity_t sev, ocsd_trc_index_t index, const char *reason);

    ocsd_err_t addElemCC(TrcStackElemParam *pParamElem);
    ocsd_err_t addElemTS(TrcStackElemParam *pParamElem, bool withCC);
    ocsd_err_t addElemEvent(TrcStackElemParam *pParamElem);
     
private:
    void SetInstrInfoInAddrISA(const ocsd_vaddr_t addr_val, const uint8_t isa); 
    const ocsd_isa calcISA(const bool SF, const uint8_t IS) const
    {
        if (SF)
            return ocsd_isa_aarch64;
        return (IS == 0) ? ocsd_isa_arm : ocsd_isa_thumb2;
    }
    typedef enum {
        WP_NOT_FOUND,
        WP_FOUND,
        WP_NACC
    } WP_res_t;

    typedef struct {
        ocsd_vaddr_t st_addr;
        ocsd_vaddr_t en_addr;
        uint32_t num_instr;
    } instr_range_t;

    //!< follow instructions from the current address to a WP. true if good, false if memory cannot be accessed.
    ocsd_err_t traceInstrToWP(instr_range_t &instr_range, WP_res_t &WPRes, const bool traceToAddrNext = false, const ocsd_vaddr_t nextAddrMatch = 0);

    inline const bool WPFound(WP_res_t res) const { return (res == WP_FOUND); };
    inline const bool WPNacc(WP_res_t res) const { return (res == WP_NACC); };
        
    ocsd_err_t returnStackPop();  // pop return stack and update instruction address.

    void setElemTraceRange(OcsdTraceElement &elemIn, const instr_range_t &addr_range, const bool executed, ocsd_trc_index_t index);
    void setElemTraceRangeInstr(OcsdTraceElement &elemIn, const instr_range_t &addr_range, 
                                const bool executed, ocsd_trc_index_t index, ocsd_instr_info &instr);

    // true if we are ETE configured.
    inline bool isETEConfig() {
        return (m_config->MajVersion() >= ETE_ARCH_VERSION);
    }

    ocsd_mem_space_acc_t getCurrMemSpace();

//** intra packet state (see ETMv4 spec 6.2.1);

    // timestamping
    uint64_t m_timestamp;   // last broadcast global Timestamp.
    bool m_ete_first_ts_marker; 

    // state and context 
    uint32_t m_context_id;              // most recent context ID
    uint32_t m_vmid_id;                 // most recent VMID
    bool m_is_secure;                   // true if Secure
    bool m_is_64bit;                    // true if 64 bit
    uint8_t m_last_IS;                  // last instruction set value from address packet.

    // cycle counts 
    int m_cc_threshold;

    // speculative trace 
    int m_curr_spec_depth;                
    int m_max_spec_depth;   // nax depth - from ID reg, beyond which auto-commit occurs 
    int m_unseen_spec_elem; // speculative elements at decode start

/** Remove elements that are associated with data trace */
#ifdef DATA_TRACE_SUPPORTED
    // data trace associative elements (unsupported at present in the decoder).
    int m_p0_key;
    int m_p0_key_max;

    // conditional non-branch trace - when data trace active (unsupported at present in the decoder)
    int m_cond_c_key;
    int m_cond_r_key;
    int m_cond_key_max_incr;
#endif

    uint8_t m_CSID; //!< Coresight trace ID for this decoder.

    bool m_IASize64;    //!< True if 64 bit instruction addresses supported.

//** Other processor state;

    // trace decode FSM
    typedef enum {
        NO_SYNC,        //!< pre start trace - init state or after reset or overflow, loss of sync.
        WAIT_SYNC,      //!< waiting for sync packet.
        WAIT_TINFO,     //!< waiting for trace info packet.
        DECODE_PKTS,    //!< processing packets - creating decode elements on stack
        RESOLVE_ELEM,   //!< analyze / resolve decode elements - create generic trace elements and pass on.
    } processor_state_t;

    processor_state_t m_curr_state;
    unsync_info_t m_unsync_eot_info;   //!< addition info when / why unsync / eot

//** P0 element stack
    EtmV4P0Stack m_P0_stack;    //!< P0 decode element stack

    // element resolution
    struct {
        int P0_commit;    //!< number of elements to commit
        int P0_cancel;    //!< elements to cancel
        bool mispredict;  //!< mispredict latest atom
        bool discard;     //!< discard elements 
    } m_elem_res;

    //! true if any of the element resolution fields are non-zero
    const bool isElemForRes() const {
        return (m_elem_res.P0_commit || m_elem_res.P0_cancel || 
            m_elem_res.mispredict || m_elem_res.discard);
    }

    void clearElemRes() {
        m_elem_res.P0_commit = 0;
        m_elem_res.P0_cancel = 0;
        m_elem_res.mispredict = false;
        m_elem_res.discard = false;
    }

    // packet decode state
    bool m_need_ctxt;   //!< need context to continue
    bool m_need_addr;   //!< need an address to continue
    bool m_elem_pending_addr;    //!< next address packet is needed for prev element.

    ocsd_instr_info m_instr_info;  //!< instruction info for code follower - in address is the next to be decoded.

    etmv4_trace_info_t m_trace_info; //!< trace info for this trace run.

    bool m_prev_overflow;

    TrcAddrReturnStack m_return_stack;  //!< the address return stack.

//** output element handling
    OcsdGenElemStack m_out_elem;  //!< output element stack.
    OcsdTraceElement &outElem() { return m_out_elem.getCurrElem(); };   //!< current  out element
};

#endif // ARM_TRC_PKT_DECODE_ETMV4I_H_INCLUDED

/* End of File trc_pkt_decode_etmv4i.h */
