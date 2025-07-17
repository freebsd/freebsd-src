/*
 * \file       trc_gen_elem.cpp
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

#include "common/trc_gen_elem.h"

#include <string>
#include <sstream>
#include <iomanip>

static const char *s_elem_descs[][2] =  
{
    {"OCSD_GEN_TRC_ELEM_UNKNOWN","Unknown trace element - default value or indicate error in stream to client."},
    {"OCSD_GEN_TRC_ELEM_NO_SYNC","Waiting for sync - either at start of decode, or after overflow / bad packet"},
    {"OCSD_GEN_TRC_ELEM_TRACE_ON","Start of trace - beginning of elements or restart after discontinuity (overflow, trace filtering)."},
    {"OCSD_GEN_TRC_ELEM_EO_TRACE","End of the available trace in the buffer."},
    {"OCSD_GEN_TRC_ELEM_PE_CONTEXT","PE status update / change (arch, ctxtid, vmid etc)."},
    {"OCSD_GEN_TRC_ELEM_INSTR_RANGE","Traced N consecutive instructions from addr (no intervening events or data elements), may have data assoc key"},
    {"OCSD_GEN_TRC_ELEM_I_RANGE_NOPATH","Traced N instructions in a range, but incomplete information as to program execution path from start to end of range"},
    {"OCSD_GEN_TRC_ELEM_ADDR_NACC","Tracing in inaccessible memory area."},
    {"OCSD_GEN_TRC_ELEM_ADDR_UNKNOWN","Tracing unknown address area."},
    {"OCSD_GEN_TRC_ELEM_EXCEPTION","Exception"},
    {"OCSD_GEN_TRC_ELEM_EXCEPTION_RET","Exception return"},
    {"OCSD_GEN_TRC_ELEM_TIMESTAMP","Timestamp - preceding elements happeded before this time."},
    {"OCSD_GEN_TRC_ELEM_CYCLE_COUNT","Cycle count - cycles since last cycle count value - associated with a preceding instruction range."},
    {"OCSD_GEN_TRC_ELEM_EVENT","Event - numbered event or trigger"},
    {"OCSD_GEN_TRC_ELEM_SWTRACE","Software trace packet - may contain data payload. STM / ITM hardware trace with channel protocol."},
    {"OCSD_GEN_TRC_ELEM_SYNC_MARKER","Synchronisation marker - marks position in stream of an element that is output later."},
    {"OCSD_GEN_TRC_ELEM_MEMTRANS","Trace indication of transactional memory operations."},
    {"OCSD_GEN_TRC_ELEM_INSTRUMENTATION", "PE instrumentation trace - PE generated SW trace, application dependent protocol."},
    {"OCSD_GEN_TRC_ELEM_CUSTOM","Fully custom packet type."}
};

static const char *instr_type[] = {
    "--- ",
    "BR  ",
    "iBR ",
    "ISB ",
    "DSB.DMB",
    "WFI.WFE",
    "TSTART"
};

#define T_SIZE (sizeof(instr_type) / sizeof(const char *))

static const char *instr_sub_type[] = {
    "--- ",
    "b+link ",
    "A64:ret ",
    "A64:eret ",
    "V7:impl ret",
};

#define ST_SIZE (sizeof(instr_sub_type) / sizeof(const char *))

static const char *s_trace_on_reason[] = {
    "begin or filter",
    "overflow",
    "debug restart"
};


static const char *s_isa_str[] = {
   "A32",      /**< V7 ARM 32, V8 AArch32 */  
   "T32",          /**< Thumb2 -> 16/32 bit instructions */  
   "A64",      /**< V8 AArch64 */
   "TEE",          /**< Thumb EE - unsupported */  
   "Jaz",      /**< Jazelle - unsupported in trace */ 
   "Cst",      /**< ISA custom */
   "Unk"       /**< ISA not yet known */
};

static const char *s_unsync_reason[] = {
    "undefined",            // UNSYNC_UNKNOWN - unknown /undefined
    "init-decoder",         // UNSYNC_INIT_DECODER - decoder intialisation - start of trace.
    "reset-decoder",        // UNSYNC_RESET_DECODER - decoder reset.
    "overflow",             // UNSYNC_OVERFLOW - overflow packet - need to re-sync
    "discard",              // UNSYNC_DISCARD - specl trace discard - need to re-sync
    "bad-packet",           // UNSYNC_BAD_PACKET - bad packet at input - resync to restart.
    "end-of-trace",         // UNSYNC_EOT - end of trace info.
};
static const char *s_transaction_type[] = {
	"Init",
    "Start",
    "Commit",
    "Fail"
};

static const char *s_marker_t[] = {
  "Timestamp marker",  //  ELEM_MARKER_TS  
};

void OcsdTraceElement::toString(std::string &str) const
{
    std::ostringstream oss;
    int num_str = sizeof(s_elem_descs) / sizeof(s_elem_descs[0]);
    int typeIdx = (int)this->elem_type;
    if(typeIdx < num_str)
    {
        oss << s_elem_descs[typeIdx][0] << "(";
        switch(elem_type)
        {
        case OCSD_GEN_TRC_ELEM_INSTR_RANGE:
            oss << "exec range=0x" << std::hex << st_addr << ":[0x" << en_addr << "] ";
            oss << "num_i(" << std::dec << num_instr_range << ") ";
            oss << "last_sz(" << last_instr_sz << ") ";
            oss << "(ISA=" << s_isa_str[(int)isa] << ") ";
            oss << ((last_instr_exec == 1) ? "E " : "N ");
            if((int)last_i_type < T_SIZE)
                oss << instr_type[last_i_type];
            if((last_i_subtype != OCSD_S_INSTR_NONE) && ((int)last_i_subtype < ST_SIZE))
                oss << instr_sub_type[last_i_subtype];
            if (last_instr_cond)
                oss << " <cond>";
            break;

        case OCSD_GEN_TRC_ELEM_ADDR_NACC:
            oss << " 0x" << std::hex << st_addr << " ";
            break;

        case OCSD_GEN_TRC_ELEM_I_RANGE_NOPATH:
            oss << "first 0x" << std::hex << st_addr << ":[next 0x" << en_addr << "] ";
            oss << "num_i(" << std::dec << num_instr_range << ") ";
            break;

        case OCSD_GEN_TRC_ELEM_EXCEPTION:
            if (excep_ret_addr == 1)
            {
                oss << "pref ret addr:0x" << std::hex << en_addr; 
                if (excep_ret_addr_br_tgt)
                {
                    oss << " [addr also prev br tgt]";
                }
                oss << "; ";
            }
            oss << "excep num (0x" << std::setfill('0') << std::setw(2) << std::hex << exception_number << ") ";
            break;

        case OCSD_GEN_TRC_ELEM_PE_CONTEXT:
            oss << "(ISA=" << s_isa_str[(int)isa] << ") ";
            if((context.exception_level > ocsd_EL_unknown) && (context.el_valid))
            {
                oss << "EL" << std::dec << (int)(context.exception_level);
            }
            switch (context.security_level) 
            {
            case ocsd_sec_secure: oss << "S; "; break;
            case ocsd_sec_nonsecure: oss << "N; "; break;
            case ocsd_sec_root: oss << "Root; "; break;
            case ocsd_sec_realm: oss << "Realm; "; break;
            }
            oss  << (context.bits64 ? "64-bit; " : "32-bit; ");
            if(context.vmid_valid)
                oss << "VMID=0x" << std::hex << context.vmid << "; ";
            if(context.ctxt_id_valid)
                oss << "CTXTID=0x" << std::hex << context.context_id << "; ";
            break;

        case  OCSD_GEN_TRC_ELEM_TRACE_ON:
            oss << " [" << s_trace_on_reason[trace_on_reason] << "]";
            break;

        case OCSD_GEN_TRC_ELEM_TIMESTAMP:
            oss << " [ TS=0x" << std::setfill('0') << std::setw(12) << std::hex << timestamp << "]; "; 
            break;

        case OCSD_GEN_TRC_ELEM_SWTRACE:
            printSWInfoPkt(oss);
            break;

        case OCSD_GEN_TRC_ELEM_EVENT:
            if(trace_event.ev_type == EVENT_TRIGGER)
                oss << " Trigger; ";
            else if(trace_event.ev_type == EVENT_NUMBERED)
                oss << " Numbered:" << std::dec << trace_event.ev_number << "; ";
            break;

        case OCSD_GEN_TRC_ELEM_EO_TRACE:
        case OCSD_GEN_TRC_ELEM_NO_SYNC:
            if (unsync_eot_info <= UNSYNC_EOT)
                oss << " [" << s_unsync_reason[unsync_eot_info] << "]";
            break;

        case OCSD_GEN_TRC_ELEM_SYNC_MARKER:
            oss << " [" << s_marker_t[sync_marker.type] << "(0x" << std::setfill('0') << std::setw(8) << std::hex << sync_marker.value << ")]";
            break;

        case OCSD_GEN_TRC_ELEM_MEMTRANS:
            if (mem_trans <= OCSD_MEM_TRANS_FAIL)
                oss << s_transaction_type[mem_trans];
            break;

        case OCSD_GEN_TRC_ELEM_INSTRUMENTATION:
            oss << "EL" << std::dec << (int)sw_ite.el << "; 0x" << std::setfill('0') << std::setw(16) << std::hex << sw_ite.value;
            break;

        default: break;
        }
        if(has_cc)
            oss << std::dec << " [CC=" << cycle_count << "]; ";
        oss << ")";
    }
    else
    {
        oss << "OCSD_GEN_TRC_ELEM??: index out of range.";
    }
    str = oss.str();
}

OcsdTraceElement &OcsdTraceElement::operator =(const ocsd_generic_trace_elem* p_elem)
{
    *dynamic_cast<ocsd_generic_trace_elem*>(this) = *p_elem;
    return *this;
}


void OcsdTraceElement::printSWInfoPkt(std::ostringstream & oss) const
{
    if (!sw_trace_info.swt_global_err)
    {
        if (sw_trace_info.swt_id_valid)
        {
            oss << " (Ma:0x" << std::setfill('0') << std::setw(2) << std::hex << sw_trace_info.swt_master_id << "; ";
            oss << "Ch:0x" << std::setfill('0') << std::setw(2) << std::hex << sw_trace_info.swt_channel_id << ") ";
        }
        else
            oss << "(Ma:0x??; Ch:0x??" << ") ";

        if (sw_trace_info.swt_payload_pkt_bitsize > 0)
        {
            oss << "0x" << std::setfill('0') << std::hex;
            if (sw_trace_info.swt_payload_pkt_bitsize == 4)
            {
                oss << std::setw(1);
                oss << (uint16_t)(((uint8_t *)ptr_extended_data)[0] & 0xF);
            }
            else
            {
                switch (sw_trace_info.swt_payload_pkt_bitsize)
                {
                case 8:
                    // force uint8 to uint16 so oss 'sees' them as something to be stringised, rather than absolute char values
                    oss << std::setw(2) << (uint16_t)((uint8_t *)ptr_extended_data)[0];
                    break;
                case 16:
                    oss << std::setw(4) << ((uint16_t *)ptr_extended_data)[0];
                    break;
                case 32:
                    oss << std::setw(8) << ((uint32_t *)ptr_extended_data)[0];
                    break;
                case 64:
                    oss << std::setw(16) << ((uint64_t *)ptr_extended_data)[0];
                    break;
                default:
                    oss << "{Data Error : unsupported bit width.}";
                    break;
                }
            }
            oss << "; ";
        }
        if (sw_trace_info.swt_marker_packet)
            oss << "+Mrk ";
        if (sw_trace_info.swt_trigger_event)
            oss << "Trig ";
        if (sw_trace_info.swt_has_timestamp)
            oss << " [ TS=0x" << std::setfill('0') << std::setw(12) << std::hex << timestamp << "]; ";
        if (sw_trace_info.swt_frequency)
            oss << "Freq";
        if (sw_trace_info.swt_master_err)
            oss << "{Master Error.}";
    }
    else
    {
        oss << "{Global Error.}";
    }
}

/*
void OcsdTraceElement::toString(const ocsd_generic_trace_elem *p_elem, std::string &str)
{
    OcsdTraceElement elem;
    elem = p_elem;
    elem.toString(str);
}
*/
/* End of File trc_gen_elem.cpp */
