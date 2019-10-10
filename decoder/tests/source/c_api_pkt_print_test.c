/*
 * \file       c_api_pkt_print_test.c
 * \brief      OpenCSD : C-API test program
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

/*
 * Example of using the library with the C-API. Used to validate that the C-API 
 * functions work.
 *
 * Simple test program to print packets from a single trace ID source stream.
 * Hard coded configuration based on the Juno r1-1 test snapshot for ETMv4 and
 * STM, TC2 test snapshot for ETMv3, PTM.
 *
 * The test source can be set from the command line, but will default to the 
 * ETMv4 trace for trace ID 0x10 on the juno r1-1 test snapshot.
 * This example uses the updated C-API functionality from library version 0.4.0 onwards.
 * Test values are hardcoded from the same values in the snapshots as we do not 
 * explicitly read the snapshot metadata in this example program.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* include the C-API library header */
#include "opencsd/c_api/opencsd_c_api.h"

/* include the test external decoder factory and decoder types headers 
   - separate from the main library includes by definition as external decoder. 
*/
#include "ext_dcd_echo_test_fact.h"
#include "ext_dcd_echo_test.h"

/* path to test snapshots, relative to tests/bin/<plat>/<dbg|rel> build output dir */
#ifdef _WIN32
const char *default_base_snapshot_path="..\\..\\..\\snapshots";
const char *juno_snapshot = "\\juno_r1_1\\";
const char *tc2_snapshot = "\\TC2\\";
#else
const char *default_base_snapshot_path = "../../snapshots";
const char *juno_snapshot = "/juno_r1_1/";
const char *tc2_snapshot = "/TC2/";
#endif
static const char *selected_snapshot;
static const char *usr_snapshot_path = 0;
#define MAX_TRACE_FILE_PATH_LEN 512

/* trace data and memory file dump names and values - taken from snapshot metadata */
const char *trace_data_filename = "cstrace.bin";
const char *stmtrace_data_filename = "cstraceitm.bin";
const char *memory_dump_filename = "kernel_dump.bin";
ocsd_vaddr_t mem_dump_address=0xFFFFFFC000081000;
const ocsd_vaddr_t mem_dump_address_tc2=0xC0008000;

/* test variables - set by command line to feature test API */
static int using_mem_acc_cb = 0;    /* test the memory access callback function */
static int use_region_file = 0;     /* test multi region memory files */
static int using_mem_acc_cb_id = 0; /* test the mem acc callback with trace ID parameter */

/* buffer to handle a packet string */
#define PACKET_STR_LEN 1024
static char packet_str[PACKET_STR_LEN];

/* decide if we decode & monitor, decode only or packet print */
typedef enum _test_op {
    TEST_PKT_PRINT,     // process trace input into discrete packets and print.
    TEST_PKT_DECODE,    // process and decode trace packets, printing discrete packets and generic output.
    TEST_PKT_DECODEONLY // process and decode trace packets, printing generic output packets only.
} test_op_t;

// Default test operations
static test_op_t op = TEST_PKT_PRINT;   // default operation is to packet print
static ocsd_trace_protocol_t test_protocol = OCSD_PROTOCOL_ETMV4I; // ETMV4 protocl
static uint8_t test_trc_id_override = 0x00; // no trace ID override.

/* external decoder testing */
static int test_extern_decoder = 0; /* test the external decoder infrastructure. */
static ocsd_extern_dcd_fact_t *p_ext_fact; /* external decoder factory */
#define EXT_DCD_NAME "ext_echo"

/* raw packet printing test */
static int frame_raw_unpacked = 0;
static int frame_raw_packed = 0;
static int test_printstr = 0;

/* test the library printer API */
static int test_lib_printers = 0;

/* Process command line options - choose the operation to use for the test. */
static int process_cmd_line(int argc, char *argv[])
{
    int idx = 1;
    int len = 0;

    while(idx < argc)
    {
        if(strcmp(argv[idx],"-decode_only") == 0)
        {
            op = TEST_PKT_DECODEONLY;
        }
        else if(strcmp(argv[idx],"-decode") == 0)
        {
            op = TEST_PKT_DECODE;
        }
        else if(strcmp(argv[idx],"-id") == 0)
        {
            idx++;
            if(idx < argc)
            {
                test_trc_id_override = (uint8_t)(strtoul(argv[idx],0,0));
                printf("ID override = 0x%02X\n",test_trc_id_override);
            }
        }
        else if(strcmp(argv[idx],"-etmv3") == 0)
        {
            test_protocol =  OCSD_PROTOCOL_ETMV3;
            selected_snapshot = tc2_snapshot;
            mem_dump_address = mem_dump_address_tc2;
        }
        else if(strcmp(argv[idx],"-ptm") == 0)
        {
            test_protocol =  OCSD_PROTOCOL_PTM;
            selected_snapshot = tc2_snapshot;
            mem_dump_address = mem_dump_address_tc2;
        }
        else if(strcmp(argv[idx],"-stm") == 0)
        {
            test_protocol = OCSD_PROTOCOL_STM;
            trace_data_filename = stmtrace_data_filename;
        }
        else if(strcmp(argv[idx],"-test_cb") == 0)
        {
            using_mem_acc_cb = 1;
            use_region_file = 0;
        }
        else if (strcmp(argv[idx], "-test_cb_id") == 0)
        { 
            using_mem_acc_cb = 1;
            use_region_file = 0;
            using_mem_acc_cb_id = 1;
        }
        else if(strcmp(argv[idx],"-test_region_file") == 0)
        {
            use_region_file = 1;
            using_mem_acc_cb = 0;
        }
        else if (strcmp(argv[idx], "-extern") == 0)
        {
            test_extern_decoder = 1;
        }
        else if (strcmp(argv[idx], "-raw") == 0)
        {
            frame_raw_unpacked = 1;
        }
        else if (strcmp(argv[idx], "-raw_packed") == 0)
        {
            frame_raw_packed = 1;
        }
        else if (strcmp(argv[idx], "-test_printstr") == 0)
        {
            test_printstr = 1;
        }
        else if (strcmp(argv[idx], "-test_libprint") == 0)
        {
            test_lib_printers = 1;
        }
        else if(strcmp(argv[idx],"-ss_path") == 0)
        {
            idx++;
            if((idx >= argc) || (strlen(argv[idx]) == 0))
            {
                printf("-ss_path: Missing path parameter or zero length\n");
                return -1;
            }
            else
            {
                len = strlen(argv[idx]);
                if(len >  (MAX_TRACE_FILE_PATH_LEN - 32))
                {
                    printf("-ss_path: path too long\n");
                    return -1;
                }
                usr_snapshot_path = argv[idx];
            }
            
        }
        else if(strcmp(argv[idx],"-help") == 0)
        {
            return -1;
        }
        else 
            printf("Ignored unknown argument %s\n", argv[idx]);
        idx++;
    }
    return 0;
}

static void print_cmd_line_help()
{
    printf("Usage:\n-etmv3|-stm|-ptm|-extern  : choose protocol (one only, default etmv4)\n");
    printf("-id <ID> : decode source for id <ID> (default 0x10)\n");
    printf("-decode | -decode_only : full decode + trace packets / full decode packets only (default trace packets only)\n");
    printf("-raw / -raw_packed: print raw unpacked / packed data;\n");
    printf("-test_printstr | -test_libprint : ttest lib printstr callback | test lib based packet printers\n");
    printf("-test_region_file | -test_cb | -test_cb_id : mem accessor - test multi region file API | test callback API [with trcid] (default single memory file)\n\n");
    printf("-ss_path <path> : path from cwd to /snapshots/ directory. Test prog will append required test subdir\n");
}

/************************************************************************/
/* Memory accessor functionality */
/************************************************************************/

static FILE *dump_file = NULL;  /* pointer to the file providing the opcode memory */
static ocsd_mem_space_acc_t dump_file_mem_space = OCSD_MEM_SPACE_ANY;   /* memory space used by the dump file */
static long mem_file_size = 0;                /* size of the memory file */
static ocsd_vaddr_t mem_file_en_address = 0;  /* end address last inclusive address in file. */

/* log the memacc output */
/* #define LOG_MEMACC_CB */

/* decode memory access using a CallBack function 
* tests CB API and add / remove mem acc API.
*/
static uint32_t do_mem_acc_cb(const void *p_context, const ocsd_vaddr_t address, const ocsd_mem_space_acc_t mem_space, const uint8_t trc_id, const uint32_t reqBytes, uint8_t *byteBuffer)
{
    uint32_t read_bytes = 0;
    size_t file_read_bytes;

    if(dump_file == NULL)
        return 0;
    
    /* bitwise & the incoming mem space and supported mem space to confirm coverage */
    if(((uint8_t)mem_space & (uint8_t)dump_file_mem_space ) == 0)   
        return 0;

    /* calculate the bytes that can be read */
    if((address >= mem_dump_address) && (address <= mem_file_en_address))
    {
        /* some bytes in our range */
        read_bytes = reqBytes;

        if((address + reqBytes - 1) > mem_file_en_address)
        {
            /* more than are available - just read the available */
            read_bytes = (uint32_t)(mem_file_en_address - (address - 1));
        }
    }

    /* read some bytes if more than 0 to read. */ 
    if(read_bytes != 0)
    {
        fseek(dump_file,(long)(address-mem_dump_address),SEEK_SET);
        file_read_bytes = fread(byteBuffer,sizeof(uint8_t),read_bytes,dump_file);
        if(file_read_bytes < read_bytes)
            read_bytes = file_read_bytes;
    }
#ifdef LOG_MEMACC_CB
    sprintf(packet_str, "mem_acc_cb(addr 0x%08llX, size %d, trcID 0x%02X)\n", address, reqBytes, trc_id);
    ocsd_def_errlog_msgout(packet_str);
#endif
    return read_bytes;
}

static uint32_t mem_acc_cb(const void *p_context, const ocsd_vaddr_t address, const ocsd_mem_space_acc_t mem_space, const uint32_t reqBytes, uint8_t *byteBuffer)
{
    return do_mem_acc_cb(p_context, address, mem_space, 0xff, reqBytes, byteBuffer);
}

static uint32_t mem_acc_id_cb(const void *p_context, const ocsd_vaddr_t address, const ocsd_mem_space_acc_t mem_space, const uint8_t trc_id, const uint32_t reqBytes, uint8_t *byteBuffer)
{
    return do_mem_acc_cb(p_context, address, mem_space, trc_id, reqBytes, byteBuffer);
}


/* Create the memory accessor using the callback function and attach to decode tree */
static ocsd_err_t create_mem_acc_cb(dcd_tree_handle_t dcd_tree_h, const char *mem_file_path)
{
    ocsd_err_t err = OCSD_OK;
    dump_file = fopen(mem_file_path,"rb");
    if(dump_file != NULL)
    {
        fseek(dump_file,0,SEEK_END);
        mem_file_size = ftell(dump_file);
        mem_file_en_address = mem_dump_address + mem_file_size - 1;

        if (using_mem_acc_cb_id)
            err = ocsd_dt_add_callback_trcid_mem_acc(dcd_tree_h, mem_dump_address, 
                mem_file_en_address, dump_file_mem_space, &mem_acc_id_cb, 0);
        else
            err = ocsd_dt_add_callback_mem_acc(dcd_tree_h, mem_dump_address, 
                mem_file_en_address, dump_file_mem_space, &mem_acc_cb, 0);
        if(err != OCSD_OK)
        {
            fclose(dump_file);
            dump_file = NULL;
        }            
    }
    else
        err = OCSD_ERR_MEM_ACC_FILE_NOT_FOUND;
    return err;
}

/* remove the callback memory accessor from decode tree */
static void destroy_mem_acc_cb(dcd_tree_handle_t dcd_tree_h)
{
    if(dump_file != NULL)
    {
        ocsd_dt_remove_mem_acc(dcd_tree_h,mem_dump_address,dump_file_mem_space);
        fclose(dump_file);
        dump_file = NULL;
    }
}

/* create and attach the memory accessor according to required test parameters */
static ocsd_err_t create_test_memory_acc(dcd_tree_handle_t handle)
{
    ocsd_err_t ret = OCSD_OK;
    char mem_file_path[MAX_TRACE_FILE_PATH_LEN];
    uint32_t i0adjust = 0x100;
    int i = 0;
    
    /* region list to test multi region memory file API */
    ocsd_file_mem_region_t region_list[4];

    /* path to the file containing the memory image traced - raw binary data in the snapshot  */
    if(usr_snapshot_path != 0)
        strcpy(mem_file_path,usr_snapshot_path);
    else
        strcpy(mem_file_path,default_base_snapshot_path);
    strcat(mem_file_path,selected_snapshot);
    strcat(mem_file_path,memory_dump_filename);

    /* 
    * decide how to handle the file - test the normal memory accessor (contiguous binary file), 
    * a callback accessor or a multi-region file (e.g. similar to using the code region in a .so) 
    *
    * The same memory dump file is used in each case, we just present it differently
    * to test the API functions.
    */

    /* memory access callback */
    if(using_mem_acc_cb)
    {
        ret = create_mem_acc_cb(handle,mem_file_path);
    }
    /* multi region file */
    else if(use_region_file)
    {

        dump_file = fopen(mem_file_path,"rb");
        if(dump_file != NULL)
        {
            fseek(dump_file,0,SEEK_END);
            mem_file_size = ftell(dump_file);
            fclose(dump_file);

            /* populate the region list - split existing file into four regions */
            for(i = 0; i < 4; i++)
            {
                if(i != 0)
                    i0adjust = 0;
                region_list[i].start_address = mem_dump_address + (i *  mem_file_size/4) + i0adjust;
                region_list[i].region_size = (mem_file_size/4) - i0adjust;
                region_list[i].file_offset = (i * mem_file_size/4) +  i0adjust;
            }

            /* create a memory file accessor - full binary file */
            ret = ocsd_dt_add_binfile_region_mem_acc(handle,&region_list[0],4,OCSD_MEM_SPACE_ANY,mem_file_path);
        }
        else 
            ret  = OCSD_ERR_MEM_ACC_FILE_NOT_FOUND;
    }
    /* create a memory file accessor - simple contiguous full binary file */
    else
    {        
        ret = ocsd_dt_add_binfile_mem_acc(handle,mem_dump_address,OCSD_MEM_SPACE_ANY,mem_file_path);
    }
    return ret;
}

/************************************************************************/
/** Packet printers */
/************************************************************************/

/* 
* Callback function to process the packets in the packet processor output stream  - 
* simply print them out in this case to the library message/error logger.
*/
ocsd_datapath_resp_t packet_handler(void *context, const ocsd_datapath_op_t op, const ocsd_trc_index_t index_sop, const void *p_packet_in)
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    int offset = 0;

    switch(op)
    {
    case OCSD_OP_DATA:
        sprintf(packet_str,"Idx:%"  OCSD_TRC_IDX_STR "; ", index_sop);
        offset = strlen(packet_str);
   
        /* 
        * got a packet - convert to string and use the libraries' message output to print to file and stdoout 
        * Since the test always prints a single ID, we know the protocol type.
        */
        if(ocsd_pkt_str(test_protocol,p_packet_in,packet_str+offset,PACKET_STR_LEN-offset) == OCSD_OK)
        {
            /* add in <CR> */
            if(strlen(packet_str) == PACKET_STR_LEN - 1) /* maximum length */
                packet_str[PACKET_STR_LEN-2] = '\n';
            else
                strcat(packet_str,"\n");

            /* print it using the library output logger. */
            ocsd_def_errlog_msgout(packet_str);
        }
        else
            resp = OCSD_RESP_FATAL_INVALID_PARAM;  /* mark fatal error */
        break;

    case OCSD_OP_EOT:
        sprintf(packet_str,"**** END OF TRACE ****\n");
        ocsd_def_errlog_msgout(packet_str);
        break;

    default: break;
    }

    return resp;
}

/* print an array of hex data - used by the packet monitor to print hex data from packet.*/
static int print_data_array(const uint8_t *p_array, const int array_size, char *p_buffer, int buf_size)
{
    int chars_printed = 0;
    int bytes_processed;
    p_buffer[0] = 0;
    
    if(buf_size > 9)
    {
        /* set up the header */
        strcat(p_buffer,"[ ");
        chars_printed+=2;

        for(bytes_processed = 0; bytes_processed < array_size; bytes_processed++)
        {
           sprintf(p_buffer+chars_printed,"0x%02X ", p_array[bytes_processed]);
           chars_printed += 5;
           if((chars_printed + 5) > buf_size)
               break;
        }

        strcat(p_buffer,"];");
        chars_printed+=2;
    }
    else if(buf_size >= 4)
    {
        sprintf(p_buffer,"[];");
        chars_printed+=3;
    }
    return chars_printed;
}

/*
* Callback function to process packets and packet data from the monitor output of the 
* packet processor. Again print them to the library error logger. 
*/
void packet_monitor(    void *context, 
                        const ocsd_datapath_op_t op,
                        const ocsd_trc_index_t index_sop, 
                        const void *p_packet_in,
                        const uint32_t size,
                        const uint8_t *p_data)
{
    int offset = 0;

    switch(op)
    {
    default: break;
    case OCSD_OP_DATA:
        sprintf(packet_str,"Idx:%"  OCSD_TRC_IDX_STR ";", index_sop);
        offset = strlen(packet_str);
        offset+= print_data_array(p_data,size,packet_str+offset,PACKET_STR_LEN-offset);

        /* got a packet - convert to string and use the libraries' message output to print to file and stdoout */
        if(ocsd_pkt_str(test_protocol,p_packet_in,packet_str+offset,PACKET_STR_LEN-offset) == OCSD_OK)
        {
            /* add in <CR> */
            if(strlen(packet_str) == PACKET_STR_LEN - 1) /* maximum length */
                packet_str[PACKET_STR_LEN-2] = '\n';
            else
                strcat(packet_str,"\n");

            /* print it using the library output logger. */
            ocsd_def_errlog_msgout(packet_str);
        }
        break;

    case OCSD_OP_EOT:
        sprintf(packet_str,"**** END OF TRACE ****\n");
        ocsd_def_errlog_msgout(packet_str);
        break;
    }
}


/*
* printer for the generic trace elements when decoder output is being processed
*/
ocsd_datapath_resp_t gen_trace_elem_print(const void *p_context, const ocsd_trc_index_t index_sop, const uint8_t trc_chan_id, const ocsd_generic_trace_elem *elem)
{
    ocsd_datapath_resp_t resp = OCSD_RESP_CONT;
    int offset = 0;

    sprintf(packet_str,"Idx:%"  OCSD_TRC_IDX_STR "; TrcID:0x%02X; ", index_sop, trc_chan_id);
    offset = strlen(packet_str);

    if(ocsd_gen_elem_str(elem, packet_str+offset,PACKET_STR_LEN - offset) == OCSD_OK)
    {
        /* add in <CR> */
        if(strlen(packet_str) == PACKET_STR_LEN - 1) /* maximum length */
            packet_str[PACKET_STR_LEN-2] = '\n';
        else
            strcat(packet_str,"\n");
    }
    else
    {
        strcat(packet_str,"Unable to create element string\n");
    }

    /* print it using the library output logger. */
    ocsd_def_errlog_msgout(packet_str);

    return resp;
}

/************************************************************************/
/** decoder creation **/

/*** generic ***/
static ocsd_err_t create_generic_decoder(dcd_tree_handle_t handle, const char *p_name, const void *p_cfg, const void *p_context)
{
    ocsd_err_t ret = OCSD_OK;
    uint8_t CSID = 0;

    if(op == TEST_PKT_PRINT) /* test operation set to packet printing only */
    {
        /* 
         * Create a packet processor on the decode tree for the configuration we have. 
         *  We need to supply the configuration 
         */
        ret = ocsd_dt_create_decoder(handle,p_name,OCSD_CREATE_FLG_PACKET_PROC,p_cfg,&CSID);
        if(ret == OCSD_OK)
        {
            /* Attach the packet handler to the output of the packet processor - referenced by CSID */
            if (test_lib_printers)
                ret = ocsd_dt_set_pkt_protocol_printer(handle, CSID, 0);
            else
                ret = ocsd_dt_attach_packet_callback(handle,CSID, OCSD_C_API_CB_PKT_SINK,&packet_handler,p_context);
            if(ret != OCSD_OK)
                ocsd_dt_remove_decoder(handle,CSID); /* if the attach failed then destroy the decoder. */
        }
    }
    else
    {
        /* Full decode - need decoder, and memory dump */

        /* create the packet decoder and packet processor pair from the supplied name */
        ret = ocsd_dt_create_decoder(handle,p_name,OCSD_CREATE_FLG_FULL_DECODER,p_cfg,&CSID);
        if(ret == OCSD_OK)
        {
            if((op != TEST_PKT_DECODEONLY) && (ret == OCSD_OK))
            {
                /* 
                * print the packets as well as the decode - use the packet processors monitor 
                * output this time, as the main output is attached to the packet decoder. 
                */
                if (test_lib_printers)
                    ret = ocsd_dt_set_pkt_protocol_printer(handle, CSID, 1);
                else
                    ret = ocsd_dt_attach_packet_callback(handle,CSID,OCSD_C_API_CB_PKT_MON,packet_monitor,p_context);
            }

            /* attach a memory accessor */
            if(ret == OCSD_OK)
                ret = create_test_memory_acc(handle);

            /* if the attach failed then destroy the decoder. */
            if(ret != OCSD_OK)
                ocsd_dt_remove_decoder(handle,CSID); 
        }
    }
    return ret;
}

/*** ETMV4 specific settings ***/
static ocsd_err_t create_decoder_etmv4(dcd_tree_handle_t dcd_tree_h)
{
    ocsd_etmv4_cfg trace_config;

    /* 
    * populate the ETMv4 configuration structure with
    * hard coded values from snapshot .ini files.
    */

    trace_config.arch_ver   = ARCH_V8;
    trace_config.core_prof  = profile_CortexA;

    trace_config.reg_configr    = 0x000000C1;
    trace_config.reg_traceidr   = 0x00000010;   /* this is the trace ID -> 0x10, change this to analyse other streams in snapshot.*/

    if(test_trc_id_override != 0)
    {
        trace_config.reg_traceidr = (uint32_t)test_trc_id_override;
    }

    trace_config.reg_idr0   = 0x28000EA1;
    trace_config.reg_idr1   = 0x4100F403;
    trace_config.reg_idr2   = 0x00000488;
    trace_config.reg_idr8   = 0x0;
    trace_config.reg_idr9   = 0x0;
    trace_config.reg_idr10  = 0x0;
    trace_config.reg_idr11  = 0x0;
    trace_config.reg_idr12  = 0x0;
    trace_config.reg_idr13  = 0x0;

    /* create an ETMV4 decoder - no context needed as we have a single stream to a single handler. */
    return create_generic_decoder(dcd_tree_h,OCSD_BUILTIN_DCD_ETMV4I,(void *)&trace_config,0);
}

/*** ETMV3 specific settings ***/
static ocsd_err_t create_decoder_etmv3(dcd_tree_handle_t dcd_tree_h)
{
    ocsd_etmv3_cfg trace_config_etmv3;

    /* 
    * populate the ETMv3 configuration structure with
    * hard coded values from snapshot .ini files.
    */

    trace_config_etmv3.arch_ver = ARCH_V7;
    trace_config_etmv3.core_prof = profile_CortexA;
    trace_config_etmv3.reg_ccer  = 0x344008F2;
    trace_config_etmv3.reg_ctrl  = 0x10001860;
    trace_config_etmv3.reg_idr  = 0x410CF250;
    trace_config_etmv3.reg_trc_id  = 0x010;
    if(test_trc_id_override != 0)
    {
        trace_config_etmv3.reg_trc_id = (uint32_t)test_trc_id_override;
    }

    /* create an ETMV3 decoder - no context needed as we have a single stream to a single handler. */
    return create_generic_decoder(dcd_tree_h,OCSD_BUILTIN_DCD_ETMV3,(void *)&trace_config_etmv3,0);
}

/*** PTM specific settings ***/
static ocsd_err_t create_decoder_ptm(dcd_tree_handle_t dcd_tree_h)
{
    ocsd_ptm_cfg trace_config_ptm;

    /* 
    * populate the PTM configuration structure with
    * hard coded values from snapshot .ini files.
    */

    trace_config_ptm.arch_ver = ARCH_V7;
    trace_config_ptm.core_prof = profile_CortexA;
    trace_config_ptm.reg_ccer  = 0x34C01AC2;
    trace_config_ptm.reg_ctrl  = 0x10001000;
    trace_config_ptm.reg_idr  = 0x411CF312;
    trace_config_ptm.reg_trc_id  = 0x013;
    if(test_trc_id_override != 0)
    {
        trace_config_ptm.reg_trc_id = (uint32_t)test_trc_id_override;
    }

    /* create an PTM decoder - no context needed as we have a single stream to a single handler. */
    return create_generic_decoder(dcd_tree_h,OCSD_BUILTIN_DCD_PTM,(void *)&trace_config_ptm,0);

}

/*** STM specific settings ***/
static ocsd_err_t create_decoder_stm(dcd_tree_handle_t dcd_tree_h)
{
    ocsd_stm_cfg trace_config_stm;

    /* 
    * populate the STM configuration structure with
    * hard coded values from snapshot .ini files.
    */
    #define STMTCSR_TRC_ID_MASK     0x007F0000
    #define STMTCSR_TRC_ID_SHIFT    16

    trace_config_stm.reg_tcsr = 0x00A00005;
    if(test_trc_id_override != 0)
    {
        trace_config_stm.reg_tcsr &= ~STMTCSR_TRC_ID_MASK;
        trace_config_stm.reg_tcsr |= ((((uint32_t)test_trc_id_override) << STMTCSR_TRC_ID_SHIFT) & STMTCSR_TRC_ID_MASK);
    }
    trace_config_stm.reg_feat3r = 0x10000;  /* channel default */
    trace_config_stm.reg_devid = 0xFF;      /* master default */

    /* not using hw event trace decode */
    trace_config_stm.reg_hwev_mast = 0;
    trace_config_stm.reg_feat1r = 0;
    trace_config_stm.hw_event = HwEvent_Unknown_Disabled;

    /* create a STM decoder - no context needed as we have a single stream to a single handler. */
    return create_generic_decoder(dcd_tree_h, OCSD_BUILTIN_DCD_STM, (void *)&trace_config_stm, 0);
}

static ocsd_err_t create_decoder_extern(dcd_tree_handle_t dcd_tree_h)
{
    echo_dcd_cfg_t trace_cfg_ext;

    /* setup the custom configuration */
    trace_cfg_ext.cs_id = 0x010;
    if (test_trc_id_override != 0)
    {
        trace_cfg_ext.cs_id = (uint32_t)test_trc_id_override;
    }

    /* create an external decoder - no context needed as we have a single stream to a single handler. */
    return create_generic_decoder(dcd_tree_h, EXT_DCD_NAME, (void *)&trace_cfg_ext, 0);
}

static ocsd_err_t attach_raw_printers(dcd_tree_handle_t dcd_tree_h)
{
    ocsd_err_t err = OCSD_OK;
    int flags = 0;
    if (frame_raw_unpacked)
        flags |= OCSD_DFRMTR_UNPACKED_RAW_OUT;
    if (frame_raw_packed)
        flags |= OCSD_DFRMTR_PACKED_RAW_OUT;
    if (flags)
    {
        err = ocsd_dt_set_raw_frame_printer(dcd_tree_h, flags);
    }
    return err;
}

static void print_output_str(const void *p_context, const char *psz_msg_str, const int str_len)
{
    printf("** CUST_PRNTSTR: %s", psz_msg_str);
}

static ocsd_err_t test_printstr_cb(dcd_tree_handle_t dcd_tree_h)
{
    ocsd_err_t err = OCSD_OK;
    if (test_printstr)
        err = ocsd_def_errlog_set_strprint_cb(dcd_tree_h, 0, print_output_str);
    return err;
}
/************************************************************************/

ocsd_err_t register_extern_decoder()
{
    ocsd_err_t err = OCSD_ERR_NO_PROTOCOL;

    p_ext_fact = ext_echo_get_dcd_fact();
    if (p_ext_fact)
    {
        err = ocsd_register_custom_decoder(EXT_DCD_NAME, p_ext_fact);
        if (err == OCSD_OK)
            test_protocol = p_ext_fact->protocol_id;
        else
            printf("External Decoder Registration: Failed to register decoder.");
    }
    else
        printf("External Decoder Registration: Failed to get decoder factory.");

    return err;
}

/* create a decoder according to options */
static ocsd_err_t create_decoder(dcd_tree_handle_t dcd_tree_h)
{
    ocsd_err_t err = OCSD_OK;
    
    /* extended for the external decoder testing*/
    if (test_extern_decoder)
            err = register_extern_decoder();
    if (err != OCSD_OK)
        return err;

    switch(test_protocol)
    {
    case OCSD_PROTOCOL_ETMV4I:
        err = create_decoder_etmv4(dcd_tree_h);
        break;

    case OCSD_PROTOCOL_ETMV3:
        err = create_decoder_etmv3(dcd_tree_h);
        break;

    case OCSD_PROTOCOL_STM:
        err = create_decoder_stm(dcd_tree_h);
        break;

    case OCSD_PROTOCOL_PTM:
        err = create_decoder_ptm(dcd_tree_h);
        break;

        /* we only register a single external decoder in this test, 
        so it will always be assigned the first custom protocol ID */
    case OCSD_PROTOCOL_CUSTOM_0:
        err = create_decoder_extern(dcd_tree_h);
        break;

    default:
        err = OCSD_ERR_NO_PROTOCOL;
        break;
    }
    return err;
}

#define INPUT_BLOCK_SIZE 1024

/* process buffer until done or error */
ocsd_err_t process_data_block(dcd_tree_handle_t dcd_tree_h, int block_index, uint8_t *p_block, const int block_size)
{
    ocsd_err_t ret = OCSD_OK;
    uint32_t bytes_done = 0;
    ocsd_datapath_resp_t dp_ret = OCSD_RESP_CONT;
    uint32_t bytes_this_time = 0;

    while((bytes_done < (uint32_t)block_size) && (ret == OCSD_OK))
    {
        if(OCSD_DATA_RESP_IS_CONT(dp_ret))
        {
            dp_ret = ocsd_dt_process_data(dcd_tree_h, 
                                OCSD_OP_DATA,
                                block_index+bytes_done,
                                block_size-bytes_done,
                                ((uint8_t *)p_block)+bytes_done,
                                &bytes_this_time);
            bytes_done += bytes_this_time;
        }
        else if(OCSD_DATA_RESP_IS_WAIT(dp_ret))
        {
            dp_ret = ocsd_dt_process_data(dcd_tree_h, OCSD_OP_FLUSH,0,0,NULL,NULL);
        }
        else
            ret = OCSD_ERR_DATA_DECODE_FATAL; /* data path responded with an error - stop processing */
    }
    return ret;
}

int process_trace_data(FILE *pf)
{
    ocsd_err_t ret = OCSD_OK;
    dcd_tree_handle_t dcdtree_handle = C_API_INVALID_TREE_HANDLE;
    uint8_t data_buffer[INPUT_BLOCK_SIZE];
    ocsd_trc_index_t index = 0;
    size_t data_read;


    /*  Create a decode tree for this source data.
        source data is frame formatted, memory aligned from an ETR (no frame syncs) so create tree accordingly 
    */
    dcdtree_handle = ocsd_create_dcd_tree(OCSD_TRC_SRC_FRAME_FORMATTED, OCSD_DFRMTR_FRAME_MEM_ALIGN);

    if(dcdtree_handle != C_API_INVALID_TREE_HANDLE)
    {

        ret = create_decoder(dcdtree_handle);
        ocsd_tl_log_mapped_mem_ranges(dcdtree_handle);

        if (ret == OCSD_OK)
        {
            /* attach the generic trace element output callback */
            if (test_lib_printers)
                ret = ocsd_dt_set_gen_elem_printer(dcdtree_handle);
            else
                ret = ocsd_dt_set_gen_elem_outfn(dcdtree_handle, gen_trace_elem_print, 0);
        }


        /* raw print and str print cb options tested in their init functions */
        if (ret == OCSD_OK)
            ret = test_printstr_cb(dcdtree_handle);

        if (ret == OCSD_OK)
            ret = attach_raw_printers(dcdtree_handle);


        /* now push the trace data through the packet processor */
        while(!feof(pf) && (ret == OCSD_OK))
        {
            /* read from file */
            data_read = fread(data_buffer,1,INPUT_BLOCK_SIZE,pf);
            if(data_read > 0)
            {
                /* process a block of data - any packets from the trace stream 
                   we have configured will appear at the callback 
                */
                ret = process_data_block(dcdtree_handle, 
                                index,
                                data_buffer,
                                data_read);
                index += data_read;
            }
            else if(ferror(pf))
                ret = OCSD_ERR_FILE_ERROR;
        }

        /* no errors - let the data path know we are at end of trace */
        if(ret == OCSD_OK)
            ocsd_dt_process_data(dcdtree_handle, OCSD_OP_EOT, 0,0,NULL,NULL);


        /* shut down the mem acc CB if in use. */
        if(using_mem_acc_cb)
        {
            destroy_mem_acc_cb(dcdtree_handle);
        }

        /* dispose of the decode tree - which will dispose of any packet processors we created 
        */
        ocsd_destroy_dcd_tree(dcdtree_handle);
    }
    else
    {
        printf("Failed to create trace decode tree\n");
        ret = OCSD_ERR_NOT_INIT;
    }
    return (int)ret;
}

int main(int argc, char *argv[])
{
    FILE *trace_data;
    char trace_file_path[MAX_TRACE_FILE_PATH_LEN];
    int ret = 0, i, len;
    char message[512];

    /* default to juno */
    selected_snapshot = juno_snapshot;

    /* command line params */
    if(process_cmd_line(argc,argv) != 0)
    {
        print_cmd_line_help();
        return -2;
    }
    
    /* trace data file path */
    if(usr_snapshot_path != 0)
        strcpy(trace_file_path,usr_snapshot_path);
    else
        strcpy(trace_file_path,default_base_snapshot_path);
    strcat(trace_file_path,selected_snapshot);
    strcat(trace_file_path,trace_data_filename);
    printf("opening %s trace data file\n",trace_file_path);
    trace_data = fopen(trace_file_path,"rb");

    if(trace_data != NULL)
    {
        /* set up the logging in the library - enable the error logger, with an output printer*/
        ret = ocsd_def_errlog_init(OCSD_ERR_SEV_INFO,1);
        
        /* set up the output - to file and stdout, set custom logfile name */
        if(ret == 0)
            ret = ocsd_def_errlog_config_output(C_API_MSGLOGOUT_FLG_FILE | C_API_MSGLOGOUT_FLG_STDOUT, "c_api_test.log");

        /* print sign-on message in log */
        sprintf(message, "C-API packet print test\nLibrary Version %s\n\n",ocsd_get_version_str());
        ocsd_def_errlog_msgout(message);

        /* print command line used */
        message[0] = 0;
        len = 0;
        for (i = 0; i < argc; i++)
        {
            len += strlen(argv[i]) + 1;
            if (len < 512)
            {
                strcat(message, argv[i]);
                strcat(message, " ");
            }
        }
        if((len + 2) < 512)
            strcat(message, "\n\n");
        ocsd_def_errlog_msgout(message);

        /* process the trace data */
        if(ret == 0)
            ret = process_trace_data(trace_data);

        /* close the data file */
        fclose(trace_data);
    }
    else
    {
        printf("Unable to open file %s to process trace data\n", trace_file_path);
        ret = -1;
    }
    return ret;
}
/* End of File simple_pkt_c_api.c */
