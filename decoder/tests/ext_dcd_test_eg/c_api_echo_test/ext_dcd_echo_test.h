/*
* \file       ext_dcd_echo_test_fact.h
* \brief      OpenCSD : Echo test custom decoder
*
* \copyright  Copyright (c) 2016, ARM Limited. All Rights Reserved.
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
#ifndef ARM_EXT_DCD_ECHO_TEST_H_INCLUDED
#define ARM_EXT_DCD_ECHO_TEST_H_INCLUDED

#include <inttypes.h>
#include "opencsd/c_api/ocsd_c_api_custom.h"

/*
 Echo test decoder designed  to test the external decoder C - API infrastructure.
 Echo decoders can be attached to any CS byte stream and will mainly echo the data
 back with a simple "protocol" decoded - not based on real protocol.

 Will test callback infrastructure and provide an implemntation example for a real external decoder.

*/

/*** decoder types ***/

typedef struct _echo_dcd_cfg {
    unsigned char cs_id;
} echo_dcd_cfg_t;

typedef struct _echo_dcd_pkt {
    uint8_t header;
    uint32_t data;
} echo_dcd_pkt_t;

typedef enum _echo_dcd_state {
    DCD_INIT,
    DCD_WAIT_SYNC,
    DCD_PROC_PACKETS
} echo_dcd_stat_t;

/** 'packet' size is 5 bytes - 1 header + 4 data. */
#define ECHO_DCD_PKT_SIZE 5     

/** main decoder structure */
typedef struct _echo_decoder {
    uint8_t data_in[ECHO_DCD_PKT_SIZE]; /** input buffer for current incoming packet data bytes */
    int data_in_count;                  /** count up to the 5 bytes per packet to know when complete packet RX */
    echo_dcd_pkt_t curr_pkt;            /** current packet  */
    ocsd_trc_index_t curr_pkt_idx;      /** buffer index for the current packet */
    echo_dcd_cfg_t reg_config;          /** Decoder config "registers" - in this case just the CSID value */
    ocsd_extern_dcd_cb_fns lib_fns;     /** Library Callback functions */
    int createFlags;                    /** creation / operational options -> packet only / full decode */
    echo_dcd_stat_t state;              /** current state of the decoder. */
    ocsd_generic_trace_elem out_pkt;    /** generic output packet */
} echo_decoder_t;



/*** internal decoder API ***/

/** decoder will contain packet printing logic */
void echo_dcd_pkt_tostr(echo_dcd_pkt_t *pkt, char *buffer, const int buflen);

/** init decoder on creation, along with library instance structure */
void echo_dcd_init(echo_decoder_t *decoder, ocsd_extern_dcd_inst_t *p_decoder_inst, const echo_dcd_cfg_t *p_config, const ocsd_extern_dcd_cb_fns *p_lib_callbacks);

#endif /* ARM_EXT_DCD_ECHO_TEST_H_INCLUDED */

