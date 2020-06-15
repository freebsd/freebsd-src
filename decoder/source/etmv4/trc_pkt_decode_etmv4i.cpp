/*
 * \file       trc_pkt_decode_etmv4i.cpp
 * \brief      OpenCSD : ETMv4 decoder
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

#include "opencsd/etmv4/trc_pkt_decode_etmv4i.h"

#include "common/trc_gen_elem.h"


#define DCD_NAME "DCD_ETMV4"

static const uint32_t ETMV4_SUPPORTED_DECODE_OP_FLAGS = OCSD_OPFLG_PKTDEC_COMMON;

TrcPktDecodeEtmV4I::TrcPktDecodeEtmV4I()
    : TrcPktDecodeBase(DCD_NAME)
{
    initDecoder();
}

TrcPktDecodeEtmV4I::TrcPktDecodeEtmV4I(int instIDNum)
    : TrcPktDecodeBase(DCD_NAME,instIDNum)
{
    initDecoder();
}

TrcPktDecodeEtmV4I::~TrcPktDecodeEtmV4I()   
{
}

/*********************** implementation packet decoding interface */

ocsd_datapath_resp_t TrcPktDecodeEtmV4I::processPacket()
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    ocsd_err_t err = OCSD_OK;
    bool bPktDone = false;

    while(!bPktDone)
    {
        switch (m_curr_state)
        {
        case NO_SYNC:
            // output the initial not synced packet to the sink
            err = m_out_elem.resetElemStack();
            if (!err)
                err = m_out_elem.addElemType(m_index_curr_pkt, OCSD_GEN_TRC_ELEM_NO_SYNC);
            if (!err)
            {
                outElem().setUnSyncEOTReason(m_unsync_eot_info);
                resp = m_out_elem.sendElements();
                m_curr_state = WAIT_SYNC;
            }
            else
                resp = OCSD_RESP_FATAL_SYS_ERR;

            // fall through to check if the current packet is the async we are waiting for.
            break;

        case WAIT_SYNC:
            if(m_curr_packet_in->getType() == ETM4_PKT_I_ASYNC)
                m_curr_state = WAIT_TINFO;
            bPktDone = true;
            break;

        case WAIT_TINFO:
            m_need_ctxt = true;
            m_need_addr = true;
            if(m_curr_packet_in->getType() == ETM4_PKT_I_TRACE_INFO)
            {
                doTraceInfoPacket();
                m_curr_state = DECODE_PKTS;
                m_return_stack.flush();
            }
            bPktDone = true;
            break;

        case DECODE_PKTS:
            // this may change the state to RESOLVE_ELEM if required;
            err = decodePacket();
            if (err)
            {
#ifdef OCSD_WARN_UNSUPPORTED
                if (err == OCSD_ERR_UNSUPP_DECODE_PKT)
                    resp = OCSD_RESP_WARN_CONT;
                else
#else
                resp = OCSD_RESP_FATAL_INVALID_DATA;
#endif

                bPktDone = true;
            }
            else if (m_curr_state != RESOLVE_ELEM)
                bPktDone = true;
            break;

        case RESOLVE_ELEM:
            // this will change the state to DECODE_PKTS once required elem resolved & 
            // needed generic packets output
            resp = resolveElements(); 
            if ((m_curr_state == DECODE_PKTS) || (!OCSD_DATA_RESP_IS_CONT(resp)))
                bPktDone = true;
            break;
        }
    }
    return resp;
}

ocsd_datapath_resp_t TrcPktDecodeEtmV4I::onEOT()
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    ocsd_err_t err;
    if ((err = commitElemOnEOT()) != OCSD_OK)
    {
        resp = OCSD_RESP_FATAL_INVALID_DATA;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR, err, "Error flushing element stack at end of trace data."));
    }
    else
        resp = m_out_elem.sendElements();
    return resp;
}

ocsd_datapath_resp_t TrcPktDecodeEtmV4I::onReset()
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    m_unsync_eot_info = UNSYNC_RESET_DECODER;
    resetDecoder();
    return resp;
}

ocsd_datapath_resp_t TrcPktDecodeEtmV4I::onFlush()
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;

    if (m_curr_state == RESOLVE_ELEM)
        resp = resolveElements();
    else
        resp = m_out_elem.sendElements();
    return resp;
}

ocsd_err_t TrcPktDecodeEtmV4I::onProtocolConfig()
{
    ocsd_err_t err = OCSD_OK;

    // set some static config elements
    m_CSID = m_config->getTraceID();
    m_max_spec_depth = m_config->MaxSpecDepth();

    // elements associated with data trace
#ifdef DATA_TRACE_SUPPORTED
    m_p0_key_max = m_config->P0_Key_Max();
    m_cond_key_max_incr = m_config->CondKeyMaxIncr();
#endif

    m_out_elem.initCSID(m_CSID);

    // set up static trace instruction decode elements
    m_instr_info.dsb_dmb_waypoints = 0;
    m_instr_info.wfi_wfe_branch = m_config->wfiwfeBranch() ? 1 : 0;
    m_instr_info.pe_type.arch = m_config->archVersion();
    m_instr_info.pe_type.profile = m_config->coreProfile();

    m_IASize64 = (m_config->iaSizeMax() == 64);

    if (m_config->enabledRetStack())
    {
        m_return_stack.set_active(true);
#ifdef TRC_RET_STACK_DEBUG
        m_return_stack.set_dbg_logger(this);
#endif
    }

    // check config compatible with current decoder support level.
    // at present no data trace, no spec depth, no return stack, no QE
    // Remove these checks as support is added.
    if(m_config->enabledDataTrace())
    {
        err = OCSD_ERR_HW_CFG_UNSUPP;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR,OCSD_ERR_HW_CFG_UNSUPP,"ETMv4 instruction decode : Data trace elements not supported"));
    }
    else if(m_config->enabledLSP0Trace())
    {
        err = OCSD_ERR_HW_CFG_UNSUPP;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR,OCSD_ERR_HW_CFG_UNSUPP,"ETMv4 instruction decode : LSP0 elements not supported."));
    }
    else if(m_config->enabledCondITrace() != EtmV4Config::COND_TR_DIS)
    {
        err = OCSD_ERR_HW_CFG_UNSUPP;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR,OCSD_ERR_HW_CFG_UNSUPP,"ETMv4 instruction decode : Trace on conditional non-branch elements not supported."));
    }
    return err;
}

/************* local decode methods */
void TrcPktDecodeEtmV4I::initDecoder()
{
    // set the operational modes supported.
    m_supported_op_flags = ETMV4_SUPPORTED_DECODE_OP_FLAGS;

    /* init elements that get set by config */
    m_max_spec_depth = 0;
    m_CSID = 0;
    m_IASize64 = false;

    // elements associated with data trace
#ifdef DATA_TRACE_SUPPORTED
    m_p0_key_max = 0;
    m_cond_key_max_incr = 0;
#endif

    // reset decoder state to unsynced
    m_unsync_eot_info = UNSYNC_INIT_DECODER;
    resetDecoder();
}

void TrcPktDecodeEtmV4I::resetDecoder()
{
    m_curr_state = NO_SYNC;
    m_timestamp = 0;
    m_context_id = 0;              
    m_vmid_id = 0;                 
    m_is_secure = true;
    m_is_64bit = false;
    m_cc_threshold = 0;
    m_curr_spec_depth = 0;
    m_need_ctxt = true;
    m_need_addr = true;
    m_elem_pending_addr = false;
    m_prev_overflow = false;
    m_P0_stack.delete_all();
    m_out_elem.resetElemStack();
    m_last_IS = 0;
    clearElemRes();

    // elements associated with data trace
#ifdef DATA_TRACE_SUPPORTED
    m_p0_key = 0;
    m_cond_c_key = 0;
    m_cond_r_key = 0;
#endif
}

void TrcPktDecodeEtmV4I::onFirstInitOK()
{
    // once init, set the output element interface to the out elem list.
    m_out_elem.initSendIf(this->getTraceElemOutAttachPt());
}

// Changes a packet into stack of trace elements - these will be resolved and output later
ocsd_err_t TrcPktDecodeEtmV4I::decodePacket()
{
    ocsd_err_t err = OCSD_OK;
    bool bAllocErr = false;
    bool is_addr = false;

    switch(m_curr_packet_in->getType())
    {
    case ETM4_PKT_I_ASYNC: // nothing to do with this packet.
    case ETM4_PKT_I_IGNORE: // or this one.
        break;

    case ETM4_PKT_I_TRACE_INFO:
        // skip subsequent TInfo packets.
        m_return_stack.flush();
        break;

    case ETM4_PKT_I_TRACE_ON:
        {
            if (m_P0_stack.createParamElemNoParam(P0_TRC_ON, false, m_curr_packet_in->getType(), m_index_curr_pkt) == 0)
                bAllocErr = true;
        }
        break;

    case ETM4_PKT_I_ATOM_F1:
    case ETM4_PKT_I_ATOM_F2:
    case ETM4_PKT_I_ATOM_F3:
    case ETM4_PKT_I_ATOM_F4:
    case ETM4_PKT_I_ATOM_F5:
    case ETM4_PKT_I_ATOM_F6:
        {
            if (m_P0_stack.createAtomElem(m_curr_packet_in->getType(), m_index_curr_pkt, m_curr_packet_in->getAtom()) == 0)
                bAllocErr = true;
            else
                m_curr_spec_depth += m_curr_packet_in->getAtom().num;
        }
        break;

    case ETM4_PKT_I_CTXT:
        {
            if (m_P0_stack.createContextElem(m_curr_packet_in->getType(), m_index_curr_pkt, m_curr_packet_in->getContext(), m_last_IS) == 0)
                bAllocErr = true;
        }
        break;

    case ETM4_PKT_I_ADDR_MATCH:
        {
            etmv4_addr_val_t addr;

            addr.val = m_curr_packet_in->getAddrVal();
            addr.isa = m_last_IS = m_curr_packet_in->getAddrIS();

            if (m_P0_stack.createAddrElem(m_curr_packet_in->getType(), m_index_curr_pkt, addr) == 0)
                bAllocErr = true;
            is_addr = true;
        }
        break;

    case ETM4_PKT_I_ADDR_CTXT_L_64IS0:
    case ETM4_PKT_I_ADDR_CTXT_L_64IS1:
    case ETM4_PKT_I_ADDR_CTXT_L_32IS0:
    case ETM4_PKT_I_ADDR_CTXT_L_32IS1:    
        {
            m_last_IS = m_curr_packet_in->getAddrIS();
            if (m_P0_stack.createContextElem(m_curr_packet_in->getType(), m_index_curr_pkt, m_curr_packet_in->getContext(), m_last_IS) == 0)
                bAllocErr = true;
        }
    case ETM4_PKT_I_ADDR_L_32IS0:
    case ETM4_PKT_I_ADDR_L_32IS1:
    case ETM4_PKT_I_ADDR_L_64IS0:
    case ETM4_PKT_I_ADDR_L_64IS1:
    case ETM4_PKT_I_ADDR_S_IS0:
    case ETM4_PKT_I_ADDR_S_IS1:
        {
            etmv4_addr_val_t addr;

            addr.val = m_curr_packet_in->getAddrVal();
            addr.isa = m_last_IS = m_curr_packet_in->getAddrIS();

            if (m_P0_stack.createAddrElem(m_curr_packet_in->getType(), m_index_curr_pkt, addr) == 0)
                bAllocErr = true;
            is_addr = true;
        }
        break;

    // Exceptions
    case ETM4_PKT_I_EXCEPT:
         {
            if (m_P0_stack.createExceptElem(m_curr_packet_in->getType(), m_index_curr_pkt, 
                                            (m_curr_packet_in->exception_info.addr_interp == 0x2), 
                                            m_curr_packet_in->exception_info.exceptionType) == 0)
                bAllocErr = true;
            else
                m_elem_pending_addr = true;  // wait for following packets before marking for commit.
        }
        break;

    case ETM4_PKT_I_EXCEPT_RTN:
        {
            // P0 element if V7M profile.
            bool bV7MProfile = (m_config->archVersion() == ARCH_V7) && (m_config->coreProfile() == profile_CortexM);
            if (m_P0_stack.createParamElemNoParam(P0_EXCEP_RET, bV7MProfile, m_curr_packet_in->getType(), m_index_curr_pkt) == 0)
                bAllocErr = true;
            else if (bV7MProfile)
                m_curr_spec_depth++;
        }
        break;

    case ETM4_PKT_I_FUNC_RET:
        {
            // P0 element iff V8M profile, otherwise ignore
            if (OCSD_IS_V8_ARCH(m_config->archVersion()) && (m_config->coreProfile() == profile_CortexM))
            {
                if (m_P0_stack.createParamElemNoParam(P0_FUNC_RET, true, m_curr_packet_in->getType(), m_index_curr_pkt) == 0)
                    bAllocErr = true;
                else
                    m_curr_spec_depth++;
            }
        }
        break;

    // event trace
    case ETM4_PKT_I_EVENT:
        {
            std::vector<uint32_t> params = { 0 };
            params[0] = (uint32_t)m_curr_packet_in->event_val;
            if (m_P0_stack.createParamElem(P0_EVENT, false, m_curr_packet_in->getType(), m_index_curr_pkt, params) == 0)
                bAllocErr = true;

        }
        break;

    /* cycle count packets */
    case ETM4_PKT_I_CCNT_F1:
    case ETM4_PKT_I_CCNT_F2:
    case ETM4_PKT_I_CCNT_F3:
        {
            std::vector<uint32_t> params = { 0 };
            params[0] = m_curr_packet_in->getCC();
            if (m_P0_stack.createParamElem(P0_CC, false, m_curr_packet_in->getType(), m_index_curr_pkt, params) == 0)
                bAllocErr = true;

        }
        break;

    // timestamp
    case ETM4_PKT_I_TIMESTAMP:
        {
            bool bTSwithCC = m_config->enabledCCI();
            uint64_t ts = m_curr_packet_in->getTS();
            std::vector<uint32_t> params = { 0, 0, 0 };
            params[0] = (uint32_t)(ts & 0xFFFFFFFF);
            params[1] = (uint32_t)((ts >> 32) & 0xFFFFFFFF);
            if (bTSwithCC)
                params[2] = m_curr_packet_in->getCC();
            if (m_P0_stack.createParamElem(bTSwithCC ? P0_TS_CC : P0_TS, false, m_curr_packet_in->getType(), m_index_curr_pkt, params) == 0)
                bAllocErr = true;

        }
        break;

    case ETM4_PKT_I_BAD_SEQUENCE:
        err = handleBadPacket("Bad byte sequence in packet.");
        break;

    case ETM4_PKT_I_BAD_TRACEMODE:
        err = handleBadPacket("Invalid packet type for trace mode.");
        break;

    case ETM4_PKT_I_RESERVED:
        err = handleBadPacket("Reserved packet header");
        break;

    // speculation 
    case ETM4_PKT_I_MISPREDICT:
    case ETM4_PKT_I_CANCEL_F1_MISPRED:
    case ETM4_PKT_I_CANCEL_F2:
    case ETM4_PKT_I_CANCEL_F3:
        m_elem_res.mispredict = true;
        if (m_curr_packet_in->getNumAtoms())
        {
            if (m_P0_stack.createAtomElem(m_curr_packet_in->getType(), m_index_curr_pkt, m_curr_packet_in->getAtom()) == 0)
                bAllocErr = true;
            else
                m_curr_spec_depth += m_curr_packet_in->getNumAtoms();
        }

    case ETM4_PKT_I_CANCEL_F1:
        m_elem_res.P0_cancel = m_curr_packet_in->getCancelElem();
        break;
                                
    case ETM4_PKT_I_COMMIT:
        m_elem_res.P0_commit = m_curr_packet_in->getCommitElem();
        break;

    case ETM4_PKT_I_OVERFLOW:
        m_prev_overflow = true;
    case ETM4_PKT_I_DISCARD:
        m_curr_spec_depth = 0;
        m_elem_res.discard = true;
        break;

        /* Q packets */
    case ETM4_PKT_I_Q:
        {
            TrcStackQElem *pQElem = m_P0_stack.createQElem(m_curr_packet_in->getType(), m_index_curr_pkt, m_curr_packet_in->Q_pkt.q_count);
            if (pQElem)
            {
                if (m_curr_packet_in->Q_pkt.addr_present)
                {
                    etmv4_addr_val_t addr;

                    addr.val = m_curr_packet_in->getAddrVal();
                    addr.isa = m_curr_packet_in->getAddrIS();
                    pQElem->setAddr(addr);
                    m_curr_spec_depth++;
                }
                else
                    m_elem_pending_addr = true;
            }
            else
                bAllocErr = true;
        }
        break;

    /*** presently unsupported packets ***/
    /* conditional instruction tracing */
    case ETM4_PKT_I_COND_FLUSH:
    case ETM4_PKT_I_COND_I_F1:
    case ETM4_PKT_I_COND_I_F2:
    case ETM4_PKT_I_COND_I_F3:
    case ETM4_PKT_I_COND_RES_F1:
    case ETM4_PKT_I_COND_RES_F2:
    case ETM4_PKT_I_COND_RES_F3:
    case ETM4_PKT_I_COND_RES_F4:
    // data synchronisation markers
    case ETM4_PKT_I_NUM_DS_MKR:
    case ETM4_PKT_I_UNNUM_DS_MKR:
        // all currently unsupported
        {
        ocsd_err_severity_t sev = OCSD_ERR_SEV_ERROR;
#ifdef OCSD_WARN_UNSUPPORTED
        sev = OCSD_ERR_SEV_WARN;
        //resp = OCSD_RESP_WARN_CONT;
#else
        //resp = OCSD_RESP_FATAL_INVALID_DATA;
#endif
        err = OCSD_ERR_UNSUPP_DECODE_PKT;
        LogError(ocsdError(sev, err, "Data trace releated, unsupported packet type."));
        }
        break;

    default:
        // any other packet - bad packet error
        err = OCSD_ERR_BAD_DECODE_PKT;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR,err,"Unknown packet type."));
        break;
    }

    // we need to wait for following address after certain packets
    // - work out if we have seen enough here...
    if (is_addr && m_elem_pending_addr)
    {
        m_curr_spec_depth++;  // increase spec depth for element waiting on address.
        m_elem_pending_addr = false;  // can't be waiting on both
    }

    if(bAllocErr)
    {
        err = OCSD_ERR_MEM;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR,OCSD_ERR_MEM,"Memory allocation error."));       
    }
    else if(m_curr_spec_depth > m_max_spec_depth)
    {
        // auto commit anything above max spec depth 
        // (this will auto commit anything if spec depth not supported!)
        m_elem_res.P0_commit = m_curr_spec_depth - m_max_spec_depth;
    }

    if (!err && isElemForRes())
        m_curr_state = RESOLVE_ELEM;
    return err;
}

void TrcPktDecodeEtmV4I::doTraceInfoPacket()
{
    m_trace_info = m_curr_packet_in->getTraceInfo();
    m_cc_threshold = m_curr_packet_in->getCCThreshold();
    m_curr_spec_depth = m_curr_packet_in->getCurrSpecDepth();

    // elements associated with data trace
#ifdef DATA_TRACE_SUPPORTED
    m_p0_key = m_curr_packet_in->getP0Key();
#endif
}

/* Element resolution
 * Commit or cancel elements as required
 * Send any buffered output packets.
 */
ocsd_datapath_resp_t TrcPktDecodeEtmV4I::resolveElements()
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    bool Complete = false;

    while (!Complete)
    {
        if (m_out_elem.numElemToSend())
            resp = m_out_elem.sendElements();
        else if (isElemForRes())
        {
            ocsd_err_t err = OCSD_OK;
            if (m_elem_res.P0_commit)
                err = commitElements();

            if (!err && m_elem_res.P0_cancel)
                err = cancelElements();

            if (!err && m_elem_res.mispredict)
                err = mispredictAtom();
            
            if (!err && m_elem_res.discard)
                err = discardElements();

            if (err != OCSD_OK)
                resp = OCSD_RESP_FATAL_INVALID_DATA;
        }
        
        // break out on error or wait request.
        if (!OCSD_DATA_RESP_IS_CONT(resp))
            break;

        // completion is nothing to send and nothing to commit
        Complete = !m_out_elem.numElemToSend() && !isElemForRes();

        // done all elements - need more packets.
        if (Complete) {
            // if we are still in resolve, the goto decode.
            if (m_curr_state == RESOLVE_ELEM)
                m_curr_state = DECODE_PKTS;
        }        
    }
    return resp;
}

/*
 * Walks through the element stack, processing from oldest element to the newest, 
   according to the number of P0 elements that need committing.
   Build a stack of output elements in the process.
 */
ocsd_err_t TrcPktDecodeEtmV4I::commitElements()
{
    ocsd_err_t err = OCSD_OK;
    bool bPopElem = true;       // do we remove the element from the stack (multi atom elements may need to stay!)
    int num_commit_req = m_elem_res.P0_commit;
    ocsd_trc_index_t err_idx = 0;
    TrcStackElem *pElem = 0;    // stacked element pointer

    err = m_out_elem.resetElemStack();

    while(m_elem_res.P0_commit && !err)
    {
        if (m_P0_stack.size() > 0)
        {
            pElem = m_P0_stack.back();  // get oldest element
            err_idx = pElem->getRootIndex(); // save index in case of error.

            switch (pElem->getP0Type())
            {
            // indicates a trace restart - beginning of trace or discontinuiuty
            case P0_TRC_ON:
                err = m_out_elem.addElemType(pElem->getRootIndex(), OCSD_GEN_TRC_ELEM_TRACE_ON);
                if (!err)
                {
                    m_out_elem.getCurrElem().trace_on_reason = m_prev_overflow ? TRACE_ON_OVERFLOW : TRACE_ON_NORMAL;
                    m_prev_overflow = false;
                    m_return_stack.flush();
                }
                break;

            case P0_ADDR:
                {
                TrcStackElemAddr *pAddrElem = dynamic_cast<TrcStackElemAddr *>(pElem);
                m_return_stack.clear_pop_pending(); // address removes the need to pop the indirect address target from the stack
                if(pAddrElem)
                {
                    SetInstrInfoInAddrISA(pAddrElem->getAddr().val, pAddrElem->getAddr().isa);
                    m_need_addr = false;
                }
                }
                break;

            case P0_CTXT:
                {
                TrcStackElemCtxt *pCtxtElem = dynamic_cast<TrcStackElemCtxt *>(pElem);
                if(pCtxtElem)
                {
                    etmv4_context_t ctxt = pCtxtElem->getContext();
                    // check this is an updated context
                    if(ctxt.updated)
                    {                        
                        err = m_out_elem.addElem(pElem->getRootIndex());
                        if (!err)
                            updateContext(pCtxtElem, outElem());
                    }
                }
                }
                break;

            case P0_EVENT:
            case P0_TS:
            case P0_CC:
            case P0_TS_CC:
                err = processTS_CC_EventElem(pElem);
                break;

            case P0_ATOM:
                {
                TrcStackElemAtom *pAtomElem = dynamic_cast<TrcStackElemAtom *>(pElem);

                if(pAtomElem)
                {
                    while(!pAtomElem->isEmpty() && m_elem_res.P0_commit && !err)
                    {
                        ocsd_atm_val atom = pAtomElem->commitOldest();

                        // check if prev atom left us an indirect address target on the return stack
                        if ((err = returnStackPop()) != OCSD_OK)
                            break;

                        // if address and context do instruction trace follower.
                        // otherwise skip atom and reduce committed elements
                        if(!m_need_ctxt && !m_need_addr)
                        {
                            err = processAtom(atom);
                        }
                        m_elem_res.P0_commit--; // mark committed 
                    }
                    if(!pAtomElem->isEmpty())   
                        bPopElem = false;   // don't remove if still atoms to process.
                }
                }
                break;

            case P0_EXCEP:
                // check if prev atom left us an indirect address target on the return stack
                if ((err = returnStackPop()) != OCSD_OK)
                    break;

                err = processException();  // output trace + exception elements.
                m_elem_res.P0_commit--;
                break;

            case P0_EXCEP_RET:
                err = m_out_elem.addElemType(pElem->getRootIndex(), OCSD_GEN_TRC_ELEM_EXCEPTION_RET);
                if (!err)
                {
                    if (pElem->isP0()) // are we on a core that counts ERET as P0?
                        m_elem_res.P0_commit--;
                }
                break;

            case P0_FUNC_RET:
                // func ret is V8M - data trace only - hint that data has been popped off the stack.
                // at this point nothing to do till the decoder starts handling data trace.
                if (pElem->isP0()) 
                    m_elem_res.P0_commit--;
                break;

            case P0_Q:
                err = processQElement();
                m_elem_res.P0_commit--;
                break;
            }

            if(bPopElem)
                m_P0_stack.delete_back();  // remove element from stack;
        }
        else
        {
            // too few elements for commit operation - decode error.
            err = OCSD_ERR_COMMIT_PKT_OVERRUN;
            LogError(ocsdError(OCSD_ERR_SEV_ERROR,OCSD_ERR_COMMIT_PKT_OVERRUN,err_idx,m_CSID,"Not enough elements to commit"));
        }
    }

    // reduce the spec depth by number of comitted elements
    m_curr_spec_depth -= (num_commit_req-m_elem_res.P0_commit);
    return err;
}

ocsd_err_t TrcPktDecodeEtmV4I::returnStackPop()
{
    ocsd_err_t err = OCSD_OK;
    ocsd_isa nextISA;
       
    if (m_return_stack.pop_pending())
    {
        ocsd_vaddr_t popAddr = m_return_stack.pop(nextISA);
        if (m_return_stack.overflow())
        {
            err = OCSD_ERR_RET_STACK_OVERFLOW;
            LogError(ocsdError(OCSD_ERR_SEV_ERROR, err, "Trace Return Stack Overflow."));
        }
        else
        {
            m_instr_info.instr_addr = popAddr;
            m_instr_info.isa = nextISA;
            m_need_addr = false;
        }
    }
    return err;
}

ocsd_err_t TrcPktDecodeEtmV4I::commitElemOnEOT()
{
    ocsd_err_t err = OCSD_OK;
    TrcStackElem *pElem = 0;

    // nothing outstanding - reset the stack before we add more
    if (!m_out_elem.numElemToSend())
        m_out_elem.resetElemStack();

    while((m_P0_stack.size() > 0) && !err)
    {
        // scan for outstanding events, TS and CC, that appear before any outstanding
        // uncommited P0 element.
        pElem = m_P0_stack.back();
            
            switch(pElem->getP0Type())
            {
                // clear stack and stop
            case P0_UNKNOWN:
            case P0_ATOM:
            case P0_TRC_ON:
            case P0_EXCEP:
            case P0_EXCEP_RET:
            case P0_OVERFLOW:
            case P0_Q:
                m_P0_stack.delete_all();
                break;

            //skip
        case P0_ADDR:
        case P0_CTXT:
            break;

            // output
        case P0_EVENT:
        case P0_TS:
        case P0_CC:
        case P0_TS_CC:
            err = processTS_CC_EventElem(pElem);
            break;
        }
        m_P0_stack.delete_back();
    }

    if(!err)
    {
        err = m_out_elem.addElemType(m_index_curr_pkt, OCSD_GEN_TRC_ELEM_EO_TRACE);
        outElem().setUnSyncEOTReason(m_prev_overflow ? UNSYNC_OVERFLOW : UNSYNC_EOT);
    }
    return err;
}

// cancel elements. These not output  
ocsd_err_t TrcPktDecodeEtmV4I::cancelElements()
{
    ocsd_err_t err = OCSD_OK;
    bool P0StackDone = false;  // checked all P0 elements on the stack
    TrcStackElem *pElem = 0;   // stacked element pointer
    EtmV4P0Stack temp;
    int num_cancel_req = m_elem_res.P0_cancel;
    
    while (m_elem_res.P0_cancel)
    {
        //search the stack for the newest elements 
        if (!P0StackDone)
        {
            if (m_P0_stack.size() == 0)
                P0StackDone = true;
            else
            {
                // get the newest element
                pElem = m_P0_stack.front();
                if (pElem->isP0()) {
                    if (pElem->getP0Type() == P0_ATOM)
                    { 
                        TrcStackElemAtom *pAtomElem = (TrcStackElemAtom *)pElem;
                        // atom - cancel N atoms
                        m_elem_res.P0_cancel -= pAtomElem->cancelNewest(m_elem_res.P0_cancel);
                        if (pAtomElem->isEmpty())
                            m_P0_stack.delete_front();  // remove the element
                    }
                    else
                    {
                        m_elem_res.P0_cancel--;
                        m_P0_stack.delete_front();  // remove the element
                    }
                } else {
                // not P0, make a keep / remove decision
                    switch (pElem->getP0Type())
                    {
                    // keep these 
                    case P0_EVENT:
                    case P0_TS:
                    case P0_CC:
                    case P0_TS_CC:
                        m_P0_stack.pop_front(false);
                        temp.push_back(pElem);
                        break;

                    default:
                        m_P0_stack.delete_front();
                        break;
                    }
                }
            }
        }
        // may have some unseen elements
        else if (m_unseen_spec_elem)
        {
            m_unseen_spec_elem--;
            m_elem_res.P0_cancel--;
        }
        // otherwise we have some sort of overrun
        else
        {
            // too few elements for commit operation - decode error.
            err = OCSD_ERR_COMMIT_PKT_OVERRUN;
            LogError(ocsdError(OCSD_ERR_SEV_ERROR, err, m_index_curr_pkt, m_CSID, "Not enough elements to cancel"));
            m_elem_res.P0_cancel = 0;
            break;
        }
       
        if (temp.size())
        {
            while (temp.size())
            {
                pElem = temp.back();
                m_P0_stack.push_front(pElem);
                temp.pop_back(false);
            }
        }
    }
    m_curr_spec_depth -= num_cancel_req - m_elem_res.P0_cancel;
    return err;
}

// mispredict an atom
ocsd_err_t TrcPktDecodeEtmV4I::mispredictAtom()
{
    ocsd_err_t err = OCSD_OK;
    bool bFoundAtom = false, bDone = false;
    TrcStackElem *pElem = 0;
       
    m_P0_stack.from_front_init();   // init iterator at front.
    while (!bDone)
    {
        pElem = m_P0_stack.from_front_next();
        if (pElem)
        {
            if (pElem->getP0Type() == P0_ATOM)
            {
                TrcStackElemAtom *pAtomElem = dynamic_cast<TrcStackElemAtom *>(pElem);
                if (pAtomElem)
                {
                    pAtomElem->mispredictNewest();
                    bFoundAtom = true;
                }
                bDone = true;
            }
            else if (pElem->getP0Type() == P0_ADDR)
            {
                // need to disregard any addresses that appear between mispredict and the atom in question
                m_P0_stack.erase_curr_from_front();
            }
        }
        else
            bDone = true;
    }
   
    // if missed atom then either overrun error or mispredict on unseen element
    if (!bFoundAtom && !m_unseen_spec_elem)
    {
        err = OCSD_ERR_COMMIT_PKT_OVERRUN;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR, err, m_index_curr_pkt, m_CSID, "Not found mispredict atom"));
    }
    m_elem_res.mispredict = false;
    return err;
}

// discard elements and flush
ocsd_err_t TrcPktDecodeEtmV4I::discardElements()
{
    ocsd_err_t err = OCSD_OK;
    TrcStackElem *pElem = 0;   // stacked element pointer

    // dump P0, elemnts - output remaining CC / TS
    while ((m_P0_stack.size() > 0) && !err)
    {
        pElem = m_P0_stack.back();
        err = processTS_CC_EventElem(pElem);
        m_P0_stack.delete_back();
    }

    // clear all speculation info
    clearElemRes(); 
    m_curr_spec_depth = 0;

    // set decode state
    m_curr_state = NO_SYNC;
    m_unsync_eot_info = m_prev_overflow ? UNSYNC_OVERFLOW : UNSYNC_DISCARD;

    // unsync so need context & address.
    m_need_ctxt = true;
    m_need_addr = true;
    m_elem_pending_addr = false;
    return err;
}

ocsd_err_t TrcPktDecodeEtmV4I::processTS_CC_EventElem(TrcStackElem *pElem)
{
    ocsd_err_t err = OCSD_OK;

    switch (pElem->getP0Type())
    {
        case P0_EVENT:
        {
            TrcStackElemParam *pParamElem = dynamic_cast<TrcStackElemParam *>(pElem);
            if (pParamElem)
                err = addElemEvent(pParamElem);
        }
        break;

        case P0_TS:
        {
            TrcStackElemParam *pParamElem = dynamic_cast<TrcStackElemParam *>(pElem);
            if (pParamElem)
                err = addElemTS(pParamElem, false);
        }
        break;

        case P0_CC:
        {
            TrcStackElemParam *pParamElem = dynamic_cast<TrcStackElemParam *>(pElem);
            if (pParamElem)
                err = addElemCC(pParamElem);
        }
        break;

        case P0_TS_CC:
        {
            TrcStackElemParam *pParamElem = dynamic_cast<TrcStackElemParam *>(pElem);
            if (pParamElem)
                err = addElemTS(pParamElem, true);
        }
        break;
    }
    return err;

}

ocsd_err_t TrcPktDecodeEtmV4I::addElemCC(TrcStackElemParam *pParamElem)
{
    ocsd_err_t err = OCSD_OK;

    err = m_out_elem.addElemType(pParamElem->getRootIndex(), OCSD_GEN_TRC_ELEM_CYCLE_COUNT);
    if (!err)
        outElem().setCycleCount(pParamElem->getParam(0));
    return err;
}

ocsd_err_t TrcPktDecodeEtmV4I::addElemTS(TrcStackElemParam *pParamElem, bool withCC)
{
    ocsd_err_t err = OCSD_OK;

    err = m_out_elem.addElemType(pParamElem->getRootIndex(), OCSD_GEN_TRC_ELEM_TIMESTAMP);
    if (!err)
    {
        outElem().timestamp = (uint64_t)(pParamElem->getParam(0)) | (((uint64_t)pParamElem->getParam(1)) << 32);
        if (withCC)
            outElem().setCycleCount(pParamElem->getParam(2));
    }
    return err;
}

ocsd_err_t TrcPktDecodeEtmV4I::addElemEvent(TrcStackElemParam *pParamElem)
{
    ocsd_err_t err = OCSD_OK;

    err = m_out_elem.addElemType(pParamElem->getRootIndex(), OCSD_GEN_TRC_ELEM_EVENT);
    if (!err)
    {
        outElem().trace_event.ev_type = EVENT_NUMBERED;
        outElem().trace_event.ev_number = pParamElem->getParam(0);
    }
    return err;
}

void TrcPktDecodeEtmV4I::setElemTraceRange(OcsdTraceElement &elemIn, const instr_range_t &addr_range, 
                                           const bool executed, ocsd_trc_index_t index)
{
    elemIn.setType(OCSD_GEN_TRC_ELEM_INSTR_RANGE);
    elemIn.setLastInstrInfo(executed, m_instr_info.type, m_instr_info.sub_type, m_instr_info.instr_size);
    elemIn.setISA(m_instr_info.isa);
    elemIn.setLastInstrCond(m_instr_info.is_conditional);
    elemIn.setAddrRange(addr_range.st_addr, addr_range.en_addr, addr_range.num_instr);
    if (executed)
        m_instr_info.isa = m_instr_info.next_isa;
}

ocsd_err_t TrcPktDecodeEtmV4I::processAtom(const ocsd_atm_val atom)
{
    ocsd_err_t err;
    TrcStackElem *pElem = m_P0_stack.back();  // get the atom element
    WP_res_t WPRes;
    instr_range_t addr_range;

    // new element for this processed atom
    if ((err = m_out_elem.addElem(pElem->getRootIndex())) != OCSD_OK)
        return err;

    err = traceInstrToWP(addr_range, WPRes);
    if(err != OCSD_OK)
    {
        if(err == OCSD_ERR_UNSUPPORTED_ISA)
        {
             m_need_addr = true;
             m_need_ctxt = true;
             LogError(ocsdError(OCSD_ERR_SEV_WARN,err,pElem->getRootIndex(),m_CSID,"Warning: unsupported instruction set processing atom packet."));  
             // wait for next context
             return OCSD_OK;
        }
        else
        {
            LogError(ocsdError(OCSD_ERR_SEV_ERROR,err,pElem->getRootIndex(),m_CSID,"Error processing atom packet."));  
            return err;
        }
    }

    if(WPFound(WPRes))
    {
        //  save recorded next instuction address
        ocsd_vaddr_t nextAddr = m_instr_info.instr_addr;

        // action according to waypoint type and atom value
        switch(m_instr_info.type)
        {
        case OCSD_INSTR_BR:
            if (atom == ATOM_E)
            {
                m_instr_info.instr_addr = m_instr_info.branch_addr;
                if (m_instr_info.is_link)
                    m_return_stack.push(nextAddr, m_instr_info.isa);

            }
            break;

        case OCSD_INSTR_BR_INDIRECT:
            if (atom == ATOM_E)
            {
                m_need_addr = true; // indirect branch taken - need new address.
                if (m_instr_info.is_link)
                    m_return_stack.push(nextAddr,m_instr_info.isa);
                m_return_stack.set_pop_pending();  // need to know next packet before we know what is to happen
            }
            break;
        }
        setElemTraceRange(outElem(), addr_range, (atom == ATOM_E), pElem->getRootIndex());
    }
    else
    {
        // no waypoint - likely inaccessible memory range.
        m_need_addr = true; // need an address update 

        if(addr_range.st_addr != addr_range.en_addr)
        {
            // some trace before we were out of memory access range
            setElemTraceRange(outElem(), addr_range, true, pElem->getRootIndex());

            // another element for the nacc...
            if (WPNacc(WPRes))
                err = m_out_elem.addElem(pElem->getRootIndex());
        }

        if(WPNacc(WPRes) && !err)
        {
            outElem().setType(OCSD_GEN_TRC_ELEM_ADDR_NACC);
            outElem().st_addr = m_instr_info.instr_addr;
        }
    }
    return err;
}

// Exception processor
ocsd_err_t TrcPktDecodeEtmV4I::processException()
{
    ocsd_err_t err;
    TrcStackElem *pElem = 0;
    TrcStackElemExcept *pExceptElem = 0;
    TrcStackElemAddr *pAddressElem = 0;
    TrcStackElemCtxt *pCtxtElem = 0;
    bool branch_target = false;    // exception address implies prior branch target address
    ocsd_vaddr_t excep_ret_addr;
    ocsd_trc_index_t excep_pkt_index;
    WP_res_t WPRes = WP_NOT_FOUND;

    // grab the exception element off the stack
    pExceptElem = dynamic_cast<TrcStackElemExcept *>(m_P0_stack.back());  // get the exception element
    excep_pkt_index = pExceptElem->getRootIndex();
    branch_target = pExceptElem->getPrevSame();
    m_P0_stack.pop_back(); // remove the exception element

    pElem = m_P0_stack.back();  // look at next element.
    if(pElem->getP0Type() == P0_CTXT)
    {
        pCtxtElem = dynamic_cast<TrcStackElemCtxt *>(pElem);
        m_P0_stack.pop_back(); // remove the context element
        pElem = m_P0_stack.back();  // next one should be an address element
    }
   
   if(pElem->getP0Type() != P0_ADDR)
   {
       // no following address element - indicate processing error.      
       LogError(ocsdError(OCSD_ERR_SEV_ERROR,OCSD_ERR_BAD_PACKET_SEQ, excep_pkt_index,m_CSID,"Address missing in exception packet."));
       return OCSD_ERR_BAD_PACKET_SEQ;
   }
   else
   {
        // extract address
        pAddressElem = static_cast<TrcStackElemAddr *>(pElem);
        excep_ret_addr = pAddressElem->getAddr().val;

        // see if there is an address + optional context element implied 
        // prior to the exception.
        if (branch_target)
        {
            // this was a branch target address - update current setting
            bool b64bit = m_instr_info.isa == ocsd_isa_aarch64;
            if (pCtxtElem) {
                b64bit = pCtxtElem->getContext().SF;
            }

            // as the exception address was also a branch target address then update the 
            // current maintained address value. This also means that there is no range to
            // output before the exception packet.
            m_instr_info.instr_addr = excep_ret_addr; 
            m_instr_info.isa = (pAddressElem->getAddr().isa == 0) ?
                    (b64bit ? ocsd_isa_aarch64 : ocsd_isa_arm) : ocsd_isa_thumb2;
            m_need_addr = false;
        }
    }   

    // need to output something - set up an element
    if ((err = m_out_elem.addElem(excep_pkt_index)))
        return err;  

    // output a context element if present
    if (pCtxtElem)
    {
        updateContext(pCtxtElem, outElem());

        // used the element - need another for later stages
        if ((err = m_out_elem.addElem(excep_pkt_index)))
            return err;
    }

    // if the preferred return address is not the end of the last output range...
    if (m_instr_info.instr_addr != excep_ret_addr)
    {        
        bool range_out = false;
        instr_range_t addr_range;

        // look for match to return address.
        err = traceInstrToWP(addr_range, WPRes, true, excep_ret_addr);

        if(err != OCSD_OK)
        {
            if(err == OCSD_ERR_UNSUPPORTED_ISA)
            {
                m_need_addr = true;
                m_need_ctxt = true;
                LogError(ocsdError(OCSD_ERR_SEV_WARN,err, excep_pkt_index,m_CSID,"Warning: unsupported instruction set processing exception packet."));
            }
            else
            {
                LogError(ocsdError(OCSD_ERR_SEV_ERROR,err, excep_pkt_index,m_CSID,"Error processing exception packet."));
            }
            return err;
        }

        if(WPFound(WPRes))
        {
            // waypoint address found - output range
            setElemTraceRange(outElem(), addr_range, true, excep_pkt_index);
            range_out = true;
        }
        else
        {
            // no waypoint - likely inaccessible memory range.
            m_need_addr = true; // need an address update 
            
            if(addr_range.st_addr != addr_range.en_addr)
            {
                // some trace before we were out of memory access range
                setElemTraceRange(outElem(), addr_range, true, excep_pkt_index);
                range_out = true;
            }
        }

        // used the element need another for NACC or EXCEP.
        if (range_out)
        {
            if ((err = m_out_elem.addElem(excep_pkt_index)))
                return err;
        }
    }
   
    // watchpoint walk resulted in inaccessible memory call...
    if (WPNacc(WPRes))
    {
        
        outElem().setType(OCSD_GEN_TRC_ELEM_ADDR_NACC);
        outElem().st_addr = m_instr_info.instr_addr;

        // used the element - need another for the final exception packet.
        if ((err = m_out_elem.addElem(excep_pkt_index)))
            return err;
    }
    
    // output exception element.
    outElem().setType(OCSD_GEN_TRC_ELEM_EXCEPTION);

    // add end address as preferred return address to end addr in element
    outElem().en_addr = excep_ret_addr;
    outElem().excep_ret_addr = 1;
    outElem().excep_ret_addr_br_tgt = branch_target;
    outElem().exception_number = pExceptElem->getExcepNum();

    m_P0_stack.delete_popped();     // clear the used elements from the stack
    return err;
}

ocsd_err_t TrcPktDecodeEtmV4I::processQElement()
{
    ocsd_err_t err = OCSD_OK;
    TrcStackQElem *pQElem;
    etmv4_addr_val_t QAddr; // address where trace restarts 
    int iCount = 0;

    pQElem = dynamic_cast<TrcStackQElem *>(m_P0_stack.back());  // get the exception element
    m_P0_stack.pop_back(); // remove the Q element.

    if (!pQElem->hasAddr())  // no address - it must be next on the stack....
    {
        TrcStackElemAddr *pAddressElem = 0;
        TrcStackElemCtxt *pCtxtElem = 0;
        TrcStackElem *pElem = 0;

        pElem = m_P0_stack.back();  // look at next element.
        if (pElem->getP0Type() == P0_CTXT)
        {
            pCtxtElem = dynamic_cast<TrcStackElemCtxt *>(pElem);
            m_P0_stack.pop_back(); // remove the context element
            pElem = m_P0_stack.back();  // next one should be an address element
        }

        if (pElem->getP0Type() != P0_ADDR)
        {
            // no following address element - indicate processing error.
            err = OCSD_ERR_BAD_PACKET_SEQ;
            LogError(ocsdError(OCSD_ERR_SEV_ERROR, err, pQElem->getRootIndex(), m_CSID, "Address missing in Q packet."));
            m_P0_stack.delete_popped();
            return err;
        }
        pAddressElem = dynamic_cast<TrcStackElemAddr *>(pElem);
        QAddr = pAddressElem->getAddr();
        m_P0_stack.pop_back();  // remove the address element
        m_P0_stack.delete_popped(); // clear used elements

        // return the context element for processing next time.
        if (pCtxtElem)
        {
            // need a new copy at the back - old one will be deleted as popped.
            m_P0_stack.createContextElem(pCtxtElem->getRootPkt(), pCtxtElem->getRootIndex(), pCtxtElem->getContext(),true);
        }
    }
    else
        QAddr = pQElem->getAddr();

    // process the Q element with address. 
    iCount = pQElem->getInstrCount();

    bool isBranch = false;

    // need to output something - set up an element
    if ((err = m_out_elem.addElem(pQElem->getRootIndex())))
        return err;

    instr_range_t addr_range;
    addr_range.st_addr = addr_range.en_addr = m_instr_info.instr_addr;
    addr_range.num_instr = 0;

    // walk iCount instructions
    for (int i = 0; i < iCount; i++)
    {
        uint32_t opcode;
        uint32_t bytesReq = 4;

        err = accessMemory(m_instr_info.instr_addr, getCurrMemSpace(), &bytesReq, (uint8_t *)&opcode);
        if (err != OCSD_OK) break;

        if (bytesReq == 4) // got data back
        {
            m_instr_info.opcode = opcode;
            err = instrDecode(&m_instr_info);
            if (err != OCSD_OK) break;

            // increment address - may be adjusted by direct branch value later
            m_instr_info.instr_addr += m_instr_info.instr_size;
            addr_range.num_instr++;

            isBranch = (m_instr_info.type == OCSD_INSTR_BR) ||
                (m_instr_info.type == OCSD_INSTR_BR_INDIRECT);

            // on a branch no way of knowing if taken - bail out
            if (isBranch)
                break;
        }
        else
            break;  // missing memory

    }

    if (err == OCSD_OK)
    {
        bool inCompleteRange = true;
        if (iCount && (addr_range.num_instr == (unsigned)iCount))
        {
            if ((m_instr_info.instr_addr == QAddr.val) ||    // complete range
                (isBranch)) // or ends on branch - only way we know if branch taken.
            {
                // output a range and continue
                inCompleteRange = false;
                // update the range decoded address in the output packet.
                addr_range.en_addr = m_instr_info.instr_addr;
                setElemTraceRange(outElem(), addr_range, true, pQElem->getRootIndex());
            }
        }

        if (inCompleteRange)
        {   
            // unknown instructions executed.
            addr_range.en_addr = QAddr.val;
            addr_range.num_instr = iCount;

            outElem().setType(OCSD_GEN_TRC_ELEM_I_RANGE_NOPATH);
            outElem().setAddrRange(addr_range.st_addr, addr_range.en_addr, addr_range.num_instr);
            outElem().setISA(calcISA(m_is_64bit, QAddr.isa));
        }

        // after the Q element, tracing resumes at the address supplied
        SetInstrInfoInAddrISA(QAddr.val, QAddr.isa);
        m_need_addr = false;
    }
    else
    {
        // output error and halt decode.
        LogError(ocsdError(OCSD_ERR_SEV_ERROR, err, pQElem->getRootIndex(), m_CSID, "Error processing Q packet"));
    }
    m_P0_stack.delete_popped();
    return err;
}

void TrcPktDecodeEtmV4I::SetInstrInfoInAddrISA(const ocsd_vaddr_t addr_val, const uint8_t isa)
{
    m_instr_info.instr_addr = addr_val;
    m_instr_info.isa = calcISA(m_is_64bit, isa);
}

// trace an instruction range to a waypoint - and set next address to restart from.
ocsd_err_t TrcPktDecodeEtmV4I::traceInstrToWP(instr_range_t &range, WP_res_t &WPRes, const bool traceToAddrNext /*= false*/, const ocsd_vaddr_t nextAddrMatch /*= 0*/)
{
    uint32_t opcode;
    uint32_t bytesReq;
    ocsd_err_t err = OCSD_OK;

    range.st_addr = range.en_addr = m_instr_info.instr_addr;
    range.num_instr = 0;

    WPRes = WP_NOT_FOUND;

    while(WPRes == WP_NOT_FOUND)
    {
        // start off by reading next opcode;
        bytesReq = 4;
        err = accessMemory(m_instr_info.instr_addr, getCurrMemSpace(),&bytesReq,(uint8_t *)&opcode);
        if(err != OCSD_OK) break;

        if(bytesReq == 4) // got data back
        {
            m_instr_info.opcode = opcode;
            err = instrDecode(&m_instr_info);
            if(err != OCSD_OK) break;

            // increment address - may be adjusted by direct branch value later
            m_instr_info.instr_addr += m_instr_info.instr_size;
            range.num_instr++;

            // either walking to match the next instruction address or a real watchpoint
            if (traceToAddrNext)
            {
                if (m_instr_info.instr_addr == nextAddrMatch) 
                    WPRes = WP_FOUND;
            }
            else if (m_instr_info.type != OCSD_INSTR_OTHER)
                WPRes = WP_FOUND;
        }
        else
        {
            // not enough memory accessible.
            WPRes = WP_NACC;
        }
    }
    // update the range decoded address in the output packet.
    range.en_addr = m_instr_info.instr_addr;
    return err;
}

void TrcPktDecodeEtmV4I::updateContext(TrcStackElemCtxt *pCtxtElem, OcsdTraceElement &elem)
{
    etmv4_context_t ctxt = pCtxtElem->getContext();
    
    elem.setType(OCSD_GEN_TRC_ELEM_PE_CONTEXT);

    // map to output element and local saved state.
    m_is_64bit = (ctxt.SF != 0);
    elem.context.bits64 = ctxt.SF;
    m_is_secure = (ctxt.NS == 0);
    elem.context.security_level = ctxt.NS ? ocsd_sec_nonsecure : ocsd_sec_secure;
    elem.context.exception_level = (ocsd_ex_level)ctxt.EL;
    elem.context.el_valid = 1;
    if(ctxt.updated_c)
    {
        elem.context.ctxt_id_valid = 1;
        m_context_id = elem.context.context_id = ctxt.ctxtID;
    }
    if(ctxt.updated_v)
    {
        elem.context.vmid_valid = 1;
        m_vmid_id = elem.context.vmid = ctxt.VMID;
    }

    // need to update ISA in case context follows address.
    elem.isa = m_instr_info.isa = calcISA(m_is_64bit, pCtxtElem->getIS());
    m_need_ctxt = false;
}

ocsd_err_t TrcPktDecodeEtmV4I::handleBadPacket(const char *reason)
{
    ocsd_err_t err = OCSD_OK;

    if(getComponentOpMode() & OCSD_OPFLG_PKTDEC_ERROR_BAD_PKTS)
    {
        // error out - stop decoding
        err = OCSD_ERR_BAD_DECODE_PKT;
        LogError(ocsdError(OCSD_ERR_SEV_ERROR,err,reason));
    }
    else
    {
        LogError(ocsdError(OCSD_ERR_SEV_WARN, OCSD_ERR_BAD_DECODE_PKT, reason));
        // switch to unsync - clear decode state
        resetDecoder();
        m_curr_state = NO_SYNC;
        m_unsync_eot_info = UNSYNC_BAD_PACKET;
    }
    return err;
}

inline ocsd_mem_space_acc_t TrcPktDecodeEtmV4I::getCurrMemSpace()
{
    static ocsd_mem_space_acc_t SMemSpace[] = {
        OCSD_MEM_SPACE_EL1S,
        OCSD_MEM_SPACE_EL1S,
        OCSD_MEM_SPACE_EL2S,
        OCSD_MEM_SPACE_EL3
    };

    static ocsd_mem_space_acc_t NSMemSpace[] = {
        OCSD_MEM_SPACE_EL1N,
        OCSD_MEM_SPACE_EL1N,
        OCSD_MEM_SPACE_EL2,
        OCSD_MEM_SPACE_EL3
    };

    /* if no valid EL value - just use S/NS */
    if (!outElem().context.el_valid)
        return  m_is_secure ? OCSD_MEM_SPACE_S : OCSD_MEM_SPACE_N;
    
    /* mem space according to EL + S/NS */
    int el = (int)(outElem().context.exception_level) & 0x3;
    return m_is_secure ? SMemSpace[el] : NSMemSpace[el];
}
/* End of File trc_pkt_decode_etmv4i.cpp */
