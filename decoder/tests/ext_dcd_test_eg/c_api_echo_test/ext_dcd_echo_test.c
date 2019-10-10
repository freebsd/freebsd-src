/*
* \file       ext_dcd_echo_test_fact.c
* \brief      OpenCSD : Echo test custom decoder factory
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opencsd/c_api/opencsd_c_api.h"
#include "opencsd/c_api/ocsd_c_api_types.h"
#include "opencsd/c_api/ocsd_c_api_cust_impl.h"

#include "ext_dcd_echo_test_fact.h"
#include "ext_dcd_echo_test.h"

/** The name of the decoder */
#define DECODER_NAME "ECHO_TEST"

/********* External callback fns passed to library *****/
/** Declare the trace data input function for the decoder - passed to library as call-back. */
static ocsd_datapath_resp_t echo_dcd_trace_data_in(const void *decoder_handle,
    const ocsd_datapath_op_t op,
    const ocsd_trc_index_t index,
    const uint32_t dataBlockSize,
    const uint8_t *pDataBlock,
    uint32_t *numBytesProcessed);

/** Allow library to update the packet monitor / sink in use flags to allow decoder to call CB as appropriate.*/
static void echo_dcd_update_mon_flags(const void *decoder_handle, const int flags);

/********* Fns called by decoder creation factory  *****/
void echo_dcd_init(echo_decoder_t *decoder, 
    ocsd_extern_dcd_inst_t *p_decoder_inst, 
    const echo_dcd_cfg_t *p_config, 
    const ocsd_extern_dcd_cb_fns *p_lib_callbacks);

void echo_dcd_pkt_tostr(echo_dcd_pkt_t *pkt, char *buffer, const int buflen);

/********* Internal decoder functions  *****/
static void echo_dcd_reset(echo_decoder_t *decoder);
static ocsd_datapath_resp_t echo_dcd_process_data(echo_decoder_t *decoder,
    const ocsd_trc_index_t index,
    const uint32_t dataBlockSize,
    const uint8_t *pDataBlock,
    uint32_t *numBytesProcessed);

static ocsd_datapath_resp_t send_gen_packet(echo_decoder_t *decoder);
static ocsd_datapath_resp_t analyse_packet(echo_decoder_t *decoder);
static ocsd_datapath_resp_t send_none_data_op(echo_decoder_t *decoder, const ocsd_datapath_op_t op);
static void print_init_test_message(echo_decoder_t *decoder);

/******Infrastructure testing functionality *********************/
/* As this is a test decoder we want to check which of the callbacks or call-ins are covered for a given test run 
  (by definition they can't all be - so will need a couple of runs to test all)
*/
enum {
    TEST_COV_ERRORLOG_CB = 0,
    TEST_COV_MSGLOG_CB,
    TEST_COV_GEN_ELEM_CB,
    TEST_COV_IDEC_CB,
    TEST_COV_MEM_ACC_CB,
    TEST_COV_PKTMON_CB,
    TEST_COV_PKTSINK_CB,
    TEST_COV_INDATA,
    TEST_COV_INCBFLAGS,
    /**/
    TEST_COV_END
};

typedef enum {
    TEST_RES_NA,    /* not tested */
    TEST_RES_OK,    /* test OK */
    TEST_RES_FAIL   /* test fail */
} test_result_t;

static test_result_t coverage[TEST_COV_END] = { TEST_RES_NA };

#define UPDATE_COVERAGE(i,r) { if(coverage[i] != TEST_RES_FAIL) coverage[i] = r; }

static void print_test_cov_results(echo_decoder_t *decoder);

/*************************/


/** init decoder on creation, along with library instance structure */
void echo_dcd_init(echo_decoder_t *decoder, ocsd_extern_dcd_inst_t *p_decoder_inst, const echo_dcd_cfg_t *p_config, const ocsd_extern_dcd_cb_fns *p_lib_callbacks)
{
    // initialise the decoder instance.

    // zero out the structure
    memset(decoder, 0, sizeof(echo_decoder_t));

    memcpy(&(decoder->reg_config), p_config, sizeof(echo_dcd_cfg_t));       // copy in the config structure.
    memcpy(&(decoder->lib_fns), p_lib_callbacks, sizeof(ocsd_extern_dcd_cb_fns));  // copy in the the library callbacks.

    echo_dcd_reset(decoder);

    // fill out the info to pass back to the library.

    // set up the decoder handle, name and CS Trace ID
    p_decoder_inst->decoder_handle = decoder;
    p_decoder_inst->p_decoder_name = DECODER_NAME;
    p_decoder_inst->cs_id = p_config->cs_id;

    // set up the data input callback
    p_decoder_inst->fn_data_in = echo_dcd_trace_data_in;
    p_decoder_inst->fn_update_pkt_mon = echo_dcd_update_mon_flags;

}

void echo_dcd_pkt_tostr(echo_dcd_pkt_t *pkt, char *buffer, const int buflen)
{
    snprintf(buffer, buflen, "ECHOTP{%d} [0x%02X] (0x%08X)", pkt->header & 0x3, pkt->header, pkt->data);
}

/**** Main decoder implementation ****/
ocsd_datapath_resp_t echo_dcd_trace_data_in(const void *decoder_handle,
    const ocsd_datapath_op_t op,
    const ocsd_trc_index_t index,
    const uint32_t dataBlockSize,
    const uint8_t *pDataBlock,
    uint32_t *numBytesProcessed)
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    echo_decoder_t *decoder = (echo_decoder_t *)decoder_handle;
    UPDATE_COVERAGE(TEST_COV_INDATA,TEST_RES_OK)

    /* Deal with each possible datapath operation.         
    */
    switch (op)
    {
    case OCSD_OP_DATA:
        resp = echo_dcd_process_data(decoder, index, dataBlockSize, pDataBlock, numBytesProcessed);
        break;

    case OCSD_OP_EOT:
        if (decoder->data_in_count > 0)
            lib_cb_LogError(&(decoder->lib_fns), OCSD_ERR_SEV_WARN, OCSD_ERR_PKT_INTERP_FAIL,decoder->curr_pkt_idx,decoder->reg_config.cs_id,"Incomplete packet at end of trace.\n");

        /* if we are in full decoder mode then generate a generic EOT packet. */
        if (decoder->createFlags &  OCSD_CREATE_FLG_FULL_DECODER)
        {
            ocsd_gen_elem_init(&(decoder->out_pkt), OCSD_GEN_TRC_ELEM_EO_TRACE);
            resp = send_gen_packet(decoder);
            send_none_data_op(decoder, OCSD_OP_EOT); /* send EOT to any packet monitor in use */
        }
        else
            resp = send_none_data_op(decoder, OCSD_OP_EOT); /*send EOT to packet sink and any packet monitor in use */
        print_test_cov_results(decoder);    /* end of test run - need to print out the coverage data */
        break;

    case OCSD_OP_FLUSH:
        /* This decoder never saves a list of incoming packets (which some real decoders may have to according to protocol). 
           Additionally this decoder both processes packets and analyses them so there is no second stage to pass the flush request on to.
           Therefore there is nothing to flush */
        break;

    case OCSD_OP_RESET:
        echo_dcd_reset(decoder);
        break;
    }
    return resp;
}

void echo_dcd_update_mon_flags(const void *decoder_handle, const int flags)
{
    lib_cb_updatePktCBFlags(&((echo_decoder_t *)decoder_handle)->lib_fns, flags);
    UPDATE_COVERAGE(TEST_COV_INCBFLAGS,TEST_RES_OK)
}

void echo_dcd_reset(echo_decoder_t *decoder)
{
    decoder->curr_pkt.header = 0;
    decoder->data_in_count = 0;
    decoder->state = DCD_INIT;
}

ocsd_datapath_resp_t echo_dcd_process_data(echo_decoder_t *decoder,
    const ocsd_trc_index_t index,
    const uint32_t dataBlockSize,
    const uint8_t *pDataBlock,
    uint32_t *numBytesProcessed)
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    uint32_t bytesUsed = 0;

    while (OCSD_DATA_RESP_IS_CONT(resp) && (bytesUsed < dataBlockSize))
    {
        switch (decoder->state)
        {

        case DCD_INIT:
            /* on initialisation / after reset output a not-synced indicator */
            ocsd_gen_elem_init(&decoder->out_pkt, OCSD_GEN_TRC_ELEM_NO_SYNC);
            resp = send_gen_packet(decoder);
            decoder->state = DCD_WAIT_SYNC; /* wait for the first sync point */
            print_init_test_message(decoder);  /* because this is in fact a test decoder - print verification messages */
            break;

        case DCD_WAIT_SYNC:
            /* In this 'protocol' sync will be a single 0x00 byte.
               Some decoders may output "unsynced packets" markers if in packet processing only mode, or on the
               packet monitor output if in use. We are not bothering here. */
            if (pDataBlock[bytesUsed] == 0x00)
                decoder->state = DCD_PROC_PACKETS;
            bytesUsed++;
            break;

        case DCD_PROC_PACKETS:
            /* collect our ECHO_DCD_PKT_SIZE byte packets into the data in buffer */
            if (decoder->data_in_count < ECHO_DCD_PKT_SIZE)
            {
                if (decoder->data_in_count == 0)
                    decoder->curr_pkt_idx = index + bytesUsed;  /* record the correct start of packet index in the buffer. */
                decoder->data_in[decoder->data_in_count++] = pDataBlock[bytesUsed++];                
            }

            /* if we have ECHO_DCD_PKT_SIZE bytes we have a packet */
            if (decoder->data_in_count == ECHO_DCD_PKT_SIZE)
            {
                resp = analyse_packet(decoder);
                decoder->data_in_count = 0; /* done with the current packet */
            }
            break;
        }
    }
    *numBytesProcessed = bytesUsed;
    return resp;
}

ocsd_datapath_resp_t send_gen_packet(echo_decoder_t *decoder)
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    /* Only output generic decode packets if we are in full decode mode. */
    if (decoder->createFlags &  OCSD_CREATE_FLG_FULL_DECODER)
    {
        resp = lib_cb_GenElemOp(&decoder->lib_fns, decoder->curr_pkt_idx, decoder->reg_config.cs_id, &decoder->out_pkt);
        UPDATE_COVERAGE(TEST_COV_GEN_ELEM_CB, (OCSD_DATA_RESP_IS_FATAL(resp) ? TEST_RES_FAIL : TEST_RES_OK))
    }
    return resp;
}

ocsd_datapath_resp_t send_none_data_op(echo_decoder_t *decoder, const ocsd_datapath_op_t op)
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    ocsd_extern_dcd_cb_fns *p_fns = &(decoder->lib_fns);
    
    /* send a none data op to the packet monitor or packet sink if in packet processing only mode
       None data ops have the data parameters all set to 0.
     */

     /* if the packet monitor callback is in use. */
    if (lib_cb_usePktMon(p_fns))
        lib_cb_PktMon(p_fns, op, 0, 0, 0, 0);

    /* if the packet sink is in use then we shouldn't be in full decoder mode.*/
    if (lib_cb_usePktSink(p_fns))
        resp = lib_cb_PktDataSink(p_fns, op, 0, 0);

    return resp;
}

void print_init_test_message(echo_decoder_t * decoder)
{
    ocsd_extern_dcd_cb_fns *p_fns = &(decoder->lib_fns);
    if (lib_cb_LogMsg(p_fns, OCSD_ERR_SEV_ERROR, "Echo_Test_Decoder: Init - LogMsgCB test.\n") == OCSD_OK)
        UPDATE_COVERAGE(TEST_COV_MSGLOG_CB, TEST_RES_OK)
    else
        UPDATE_COVERAGE(TEST_COV_MSGLOG_CB, TEST_RES_FAIL)

    if(lib_cb_LogError(p_fns, OCSD_ERR_SEV_ERROR, OCSD_OK, 0, decoder->reg_config.cs_id, "Echo_Test_Decoder - Init - LogErrorCB test.\n") == OCSD_OK)
        UPDATE_COVERAGE(TEST_COV_ERRORLOG_CB, TEST_RES_OK)
    else
        UPDATE_COVERAGE(TEST_COV_ERRORLOG_CB, TEST_RES_FAIL)
}

void print_test_cov_results(echo_decoder_t *decoder)
{
    int i;
    ocsd_extern_dcd_cb_fns *p_fns = &(decoder->lib_fns);
    static char *results[] = {
        "Not Tested", "Passed", "Failed"
    };
    static char *cov_elem_names[] = {
        "ERRORLOG_CB",
        "MSGLOG_CB",
        "GEN_ELEM_CB",
        "IDEC_CB",
        "MEM_ACC_CB",
        "PKTMON_CB",
        "PKTSINK_CB",
        "INDATA",
        "INCBFLAGS"
    };
    char coverage_message[256];

    for (i = 0; i < TEST_COV_END; i++)
    {
        sprintf(coverage_message, "Element %s : %s\n",cov_elem_names[i],results[coverage[i]]);
        if (coverage[TEST_COV_MSGLOG_CB] == TEST_RES_OK)    /* check we can use the msg logger for outputting the results */
            lib_cb_LogMsg(p_fns, OCSD_ERR_SEV_ERROR, coverage_message);
        else
	  printf("%s", coverage_message);
    }
}


/* This is the packet decode portion of the decoder. 
*  incoming protocol packets are analysed to create generic output packets. 
*/
ocsd_datapath_resp_t analyse_packet(echo_decoder_t * decoder)
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    ocsd_extern_dcd_cb_fns *p_fns = &(decoder->lib_fns);
    uint32_t num_mem_bytes = 4;
    uint8_t mem_buffer[4];
    ocsd_instr_info instr_info;
    ocsd_err_t err;

    /* create a packet from the data */
    decoder->curr_pkt.header = decoder->data_in[0];
    decoder->curr_pkt.data = *((uint32_t *)&decoder->data_in[1]);
    
    /* if the packet monitor callback is in use - output the newly created packet. */
    if (lib_cb_usePktMon(p_fns))
    {
        lib_cb_PktMon(p_fns, OCSD_OP_DATA, decoder->curr_pkt_idx, (const void *)(&decoder->curr_pkt), ECHO_DCD_PKT_SIZE, decoder->data_in);
        UPDATE_COVERAGE(TEST_COV_PKTMON_CB, TEST_RES_OK)
    }

    /* if the packet sink is in use then we shouldn't be in full decoder mode.*/
    if (lib_cb_usePktSink(p_fns))
    {
        resp = lib_cb_PktDataSink(p_fns, OCSD_OP_DATA, decoder->curr_pkt_idx, (const void *)(&decoder->curr_pkt));
        UPDATE_COVERAGE(TEST_COV_PKTSINK_CB, (OCSD_DATA_RESP_IS_FATAL(resp) ? TEST_RES_FAIL : TEST_RES_OK))
    }
    else if (decoder->createFlags &  OCSD_CREATE_FLG_FULL_DECODER) /* no packet sink so are we full decoder? */
    {
        /*  Full decode - generate generic output packets. 
        
            A real decoder will sometimes require multiple input packets per output packet, or may generate multiple output
            packets per single input packet. Here we stick at 1:1 for test simplicity.

            This code will also test the infrastructure callbacks to ensure that everything holds together correctly.
        */

        /* nominally 4 types of packet */
        switch (decoder->curr_pkt.header & 0x3)
        {
        case 0:
            ocsd_gen_elem_init(&decoder->out_pkt, OCSD_GEN_TRC_ELEM_CUSTOM);    /* full custom packet */
            decoder->out_pkt.extended_data = 1;/* mark the extended ptr in use */
            decoder->out_pkt.ptr_extended_data = decoder->data_in;  /* the custom packet data in this protocol just the packet itself (hence 'echo')*/
            break;

        case 1:
            /* custom decoders can re-use existing packet types if they follow the rules for those types. */
            ocsd_gen_elem_init(&decoder->out_pkt, OCSD_GEN_TRC_ELEM_INSTR_RANGE); 
            /* fake up an address range using the input data */
            decoder->out_pkt.st_addr = decoder->curr_pkt.data & 0xFFFFFFF0;
            decoder->out_pkt.en_addr = decoder->curr_pkt.data + 0x10 + (((uint32_t)decoder->curr_pkt.header) << 2);
            decoder->out_pkt.isa = ocsd_isa_custom;
            decoder->out_pkt.last_instr_exec = (decoder->curr_pkt.header & 0x4) ? 1 : 0;
            break;

        case 2:
            /* test the memory access callback. */
            err = lib_cb_MemAccess(p_fns, decoder->curr_pkt.data & 0xFFFFFFF0, decoder->reg_config.cs_id, OCSD_MEM_SPACE_ANY, &num_mem_bytes, mem_buffer);
            if (err != OCSD_OK)
                lib_cb_LogError(p_fns, OCSD_ERR_SEV_ERROR, err, decoder->curr_pkt_idx, decoder->reg_config.cs_id, "Error accessing memory area\n.");
            UPDATE_COVERAGE(TEST_COV_MEM_ACC_CB,(err == OCSD_OK ? TEST_RES_OK : TEST_RES_FAIL))
            if (num_mem_bytes == 0)
            {
                /* unable to read the address... */
                ocsd_gen_elem_init(&decoder->out_pkt, OCSD_GEN_TRC_ELEM_ADDR_NACC);
                decoder->out_pkt.st_addr = decoder->curr_pkt.data & 0xFFFFFFF0;
            }
            else
            {
                /* try something different */
                ocsd_gen_elem_init(&decoder->out_pkt, OCSD_GEN_TRC_ELEM_CYCLE_COUNT);
                decoder->out_pkt.cycle_count = *((uint32_t *)mem_buffer);
                decoder->out_pkt.has_cc = 1;
            }
            break;

        case 3:
            /* test the ARM instruction decode callback */
            instr_info.pe_type.arch = ARCH_V8;
            instr_info.pe_type.profile = profile_CortexA;
            instr_info.isa = ocsd_isa_aarch64;
            instr_info.opcode = decoder->curr_pkt.data;
            instr_info.instr_addr = decoder->curr_pkt.data & 0xFFFFF000;
            instr_info.dsb_dmb_waypoints = 0;
            
            err = lib_cb_DecodeArmInst(p_fns, &instr_info);
            UPDATE_COVERAGE(TEST_COV_IDEC_CB, (err == OCSD_OK ? TEST_RES_OK : TEST_RES_FAIL))
            if (err != OCSD_OK)
            {
                lib_cb_LogError(p_fns, OCSD_ERR_SEV_ERROR, err, decoder->curr_pkt_idx, decoder->reg_config.cs_id, "Error decoding instruction\n.");
                ocsd_gen_elem_init(&decoder->out_pkt, OCSD_GEN_TRC_ELEM_CUSTOM);
                decoder->out_pkt.has_ts = 1;
                decoder->out_pkt.timestamp = decoder->curr_pkt.data;
            }
            else
            {
                ocsd_gen_elem_init(&decoder->out_pkt, OCSD_GEN_TRC_ELEM_INSTR_RANGE);
                /* fake up an address range using the input data */
                decoder->out_pkt.st_addr = decoder->curr_pkt.data & 0xFFFFFFF0;
                decoder->out_pkt.en_addr = decoder->curr_pkt.data + 0x10 + (((uint32_t)decoder->curr_pkt.header) << 2);
                decoder->out_pkt.isa = ocsd_isa_aarch64;
                decoder->out_pkt.last_instr_exec = (decoder->curr_pkt.header & 0x4) ? 1 : 0;
                decoder->out_pkt.last_i_type = instr_info.type;
                decoder->out_pkt.last_i_subtype = instr_info.sub_type;
            }
            break;
        }
        resp = send_gen_packet(decoder);
    }
    return resp;
}
