/*!
 * \file       opencsd/trc_gen_elem_types.h
 * \brief      OpenCSD : Decoder Output Generic Element types.
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

#ifndef ARM_TRC_GEN_ELEM_TYPES_H_INCLUDED
#define ARM_TRC_GEN_ELEM_TYPES_H_INCLUDED

/** @defgroup gen_trc_elem  OpenCSD Library : Generic Trace Elements
  * @brief Generic trace elements output by the PE trace decode and SW stim decode stages.
  *
  * 
@{*/

#include "opencsd/ocsd_if_types.h"

/**  Enum for generic element types */
typedef enum _ocsd_gen_trc_elem_t 
{  
    OCSD_GEN_TRC_ELEM_UNKNOWN = 0,     /*!< Unknown trace element - default value or indicate error in stream to client */
    OCSD_GEN_TRC_ELEM_NO_SYNC,         /*!< Waiting for sync - either at start of decode, or after overflow / bad packet */
    OCSD_GEN_TRC_ELEM_TRACE_ON,        /*!< Start of trace - beginning of elements or restart after discontinuity (overflow, trace filtering). */
    OCSD_GEN_TRC_ELEM_EO_TRACE,        /*!< end of the available trace in the buffer.  */
    OCSD_GEN_TRC_ELEM_PE_CONTEXT,      /*!< PE status update / change (arch, ctxtid, vmid etc).  */
    OCSD_GEN_TRC_ELEM_INSTR_RANGE,     /*!< traced N consecutive instructions from addr (no intervening events or data elements), may have data assoc key  */
    OCSD_GEN_TRC_ELEM_I_RANGE_NOPATH,  /*!< traced N instructions in a range, but incomplete information as to program execution path from start to end of range */
    OCSD_GEN_TRC_ELEM_ADDR_NACC,       /*!< tracing in inaccessible memory area  */ 
    OCSD_GEN_TRC_ELEM_ADDR_UNKNOWN,    /*!< address currently unknown - need address packet update */
    OCSD_GEN_TRC_ELEM_EXCEPTION,       /*!< exception - start address may be exception target, end address may be preferred ret addr. */
    OCSD_GEN_TRC_ELEM_EXCEPTION_RET,   /*!< expection return */
    OCSD_GEN_TRC_ELEM_TIMESTAMP,       /*!< Timestamp - preceding elements happeded before this time. */
    OCSD_GEN_TRC_ELEM_CYCLE_COUNT,     /*!< Cycle count - cycles since last cycle count value - associated with a preceding instruction range. */
    OCSD_GEN_TRC_ELEM_EVENT,           /*!< Event - trigger or numbered event  */   
    OCSD_GEN_TRC_ELEM_SWTRACE,         /*!< Software trace packet - may contain data payload. STM / ITM hardware trace with channel protocol */
    OCSD_GEN_TRC_ELEM_SYNC_MARKER,     /*!< Synchronisation marker - marks position in stream of an element that is output later. */
    OCSD_GEN_TRC_ELEM_MEMTRANS,        /*!< Trace indication of transactional memory operations. */
    OCSD_GEN_TRC_ELEM_INSTRUMENTATION, /*!< PE instrumentation trace - PE generated SW trace, application dependent protocol. */
    OCSD_GEN_TRC_ELEM_CUSTOM,          /*!< Fully custom packet type - used by none-ARM architecture decoders */
} ocsd_gen_trc_elem_t;


typedef enum _trace_on_reason_t {
    TRACE_ON_NORMAL = 0,    /**< Trace on at start of trace or filtering discontinuity */
    TRACE_ON_OVERFLOW,      /**< Trace on due to prior trace overflow discontinuity */
    TRACE_ON_EX_DEBUG,      /**< Trace restarted due to debug exit */
} trace_on_reason_t;

typedef struct _trace_event_t {
    uint16_t ev_type;          /**< event type - unknown (0) trigger (1), numbered event (2)*/
    uint16_t ev_number;        /**< event number if numbered event type */
} trace_event_t;

typedef enum _unsync_info_t {
    UNSYNC_UNKNOWN,         /**< unknown /undefined */
    UNSYNC_INIT_DECODER,    /**< decoder intialisation - start of trace. */
    UNSYNC_RESET_DECODER,   /**< decoder reset. */
    UNSYNC_OVERFLOW,        /**< overflow packet - need to re-sync / end of trace after overflow. */
    UNSYNC_DISCARD,         /**< specl trace discard - need to re-sync. */
    UNSYNC_BAD_PACKET,      /**< bad packet at input - resync to restart. */
    UNSYNC_EOT,             /**< end of trace - no additional info */
} unsync_info_t;

typedef enum _trace_sync_marker_t {
    ELEM_MARKER_TS,        /**< Marker for timestamp element */
} trace_sync_marker_t;

typedef struct _trace_marker_payload_t {
    trace_sync_marker_t type;   /**< type of sync marker */
    uint32_t value;             /**< sync marker value - usage depends on type */
} trace_marker_payload_t;

typedef enum _memtrans_t {
    OCSD_MEM_TRANS_TRACE_INIT,/**< Trace started while PE in transactional state */
    OCSD_MEM_TRANS_START,     /**< Trace after this packet is part of a transactional memory sequence */
    OCSD_MEM_TRANS_COMMIT,    /**< Transactional memory sequence valid. */
    OCSD_MEM_TRANS_FAIL,      /**< Transactional memory sequence failed - operations since start of transaction have been unwound. */  
} trace_memtrans_t;

typedef struct _sw_ite_t {
    uint8_t el;             /**< exception level for PE sw instrumentation instruction */
    uint64_t value;         /**< payload for PE sw instrumentation instruction */
} trace_sw_ite_t;

typedef struct _ocsd_generic_trace_elem {
    ocsd_gen_trc_elem_t elem_type;   /**< Element type - remaining data interpreted according to this value */
    ocsd_isa           isa;          /**< instruction set for executed instructions */
    ocsd_vaddr_t       st_addr;      /**< start address for instruction execution range / inaccessible code address / data address */
    ocsd_vaddr_t       en_addr;        /**< end address (exclusive) for instruction execution range. */
    ocsd_pe_context    context;        /**< PE Context */
    uint64_t           timestamp;      /**< timestamp value for TS element type */
    uint32_t           cycle_count;    /**< cycle count for explicit cycle count element, or count for element with associated cycle count */
    ocsd_instr_type    last_i_type;    /**< Last instruction type if instruction execution range */
    ocsd_instr_subtype last_i_subtype; /**< sub type for last instruction in range */
 
    //! per element flags
    union {
        struct {
            uint32_t last_instr_exec:1;     /**< 1 if last instruction in range was executed; */
            uint32_t last_instr_sz:3;       /**< size of last instruction in bytes (2/4) */
            uint32_t has_cc:1;              /**< 1 if this packet has a valid cycle count included (e.g. cycle count included as part of instruction range packet, always 1 for pure cycle count packet.*/
            uint32_t cpu_freq_change:1;     /**< 1 if this packet indicates a change in CPU frequency */
            uint32_t excep_ret_addr:1;      /**< 1 if en_addr is the preferred exception return address on exception packet type */
            uint32_t excep_data_marker:1;   /**< 1 if the exception entry packet is a data push marker only, with no address information (used typically in v7M trace for marking data pushed onto stack) */
            uint32_t extended_data:1;       /**< 1 if the packet extended data pointer is valid. Allows packet extensions for custom decoders, or additional data payloads for data trace.  */
            uint32_t has_ts:1;              /**< 1 if the packet has an associated timestamp - e.g. SW/STM trace TS+Payload as a single packet */
            uint32_t last_instr_cond:1;     /**< 1 if the last instruction was conditional */
            uint32_t excep_ret_addr_br_tgt:1;   /**< 1 if exception return address (en_addr) is also the target of a taken branch addr from the previous range. */
        };
        uint32_t flag_bits;
    };

    //! packet specific payloads
    union {  
        uint32_t exception_number;          /**< exception number for exception type packets */
        trace_event_t  trace_event;         /**< Trace event - trigger etc      */
        trace_on_reason_t trace_on_reason;  /**< reason for the trace on packet */
        ocsd_swt_info_t sw_trace_info;      /**< software trace packet info    */
		uint32_t num_instr_range;	        /**< number of instructions covered by range packet (for T32 this cannot be calculated from en-st/i_size) */
        unsync_info_t unsync_eot_info;      /**< additional information for unsync / end-of-trace packets. */
        trace_marker_payload_t sync_marker; /**< marker element - sync later element to position in stream */
        trace_memtrans_t mem_trans;         /**< memory transaction packet - transaction event */
        trace_sw_ite_t sw_ite;              /**< PE sw instrumentation using FEAT_ITE */
    };

    const void *ptr_extended_data;        /**< pointer to extended data buffer (data trace, sw trace payload) / custom structure */

} ocsd_generic_trace_elem;


typedef enum _event_t {
    EVENT_UNKNOWN = 0,
    EVENT_TRIGGER,
    EVENT_NUMBERED
} event_t;


/** @}*/
#endif // ARM_TRC_GEN_ELEM_TYPES_H_INCLUDED

/* End of File opencsd/trc_gen_elem_types.h */
