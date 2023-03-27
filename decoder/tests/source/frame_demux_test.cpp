/*
* \file     frame_demux_test.cpp
* \brief    OpenCSD: Test the frame demux code for robustness with correct and invalid data.
*
* \copyright  Copyright (c) 2022, ARM Limited. All Rights Reserved.
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

/* Runs sets of test data through the frame demuxer to ensure that it is robust for valid and 
 * invalid inputs
 */

#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <cstring>

#include "opencsd.h"              // the library

 /* Decode tree is the main decoder framework - contains the frame demuxer
    and will have an output printer attached to the raw output */
static DecodeTree* pDecoder = 0;
static const uint32_t base_cfg = OCSD_DFRMTR_FRAME_MEM_ALIGN | 
        OCSD_DFRMTR_PACKED_RAW_OUT | OCSD_DFRMTR_UNPACKED_RAW_OUT;
static ocsdDefaultErrorLogger err_log;
static ocsdMsgLogger logger;

/* test data */
#define ID_BYTE_ID(id) ((uint8_t)(id) << 1 | 0x01)
#define ID_BYTE_DATA(data) ((uint8_t)(data & 0xFE))
#define FLAGS_BYTE(id0, id1, id2, id3, id4, id5, id6, id7) ((uint8_t) ( \
    ((id7 & 0x1) << 7) | ((id6 & 0x1) << 6) | ((id5 & 0x1) << 5) | ((id4 & 0x1) << 4) | \
    ((id3 & 0x1) << 3) | ((id2 & 0x1) << 2) | ((id1 & 0x1) << 1) | (id0 & 0x1) ))
#define HSYNC_BYTES() 0xff, 0x7f
#define FSYNC_BYTES() 0xff, 0xff, 0xff, 0x7f
#define DATASIZE(array) static const size_t array##_sz = sizeof(array) / sizeof(array[0])


static const uint8_t buf_hsync_fsync[] = {
    FSYNC_BYTES(),
    ID_BYTE_ID(0x10), 0x01, ID_BYTE_DATA(0x2), 0x03,
    HSYNC_BYTES(), ID_BYTE_ID(0x20), 0x4, ID_BYTE_DATA(0x5), 0x6,
    ID_BYTE_DATA(0x7), 0x08, HSYNC_BYTES(), ID_BYTE_DATA(0x9), 0xA,
    ID_BYTE_ID(0x10), 0x0B, ID_BYTE_DATA(0xC),
    FLAGS_BYTE(0, 0, 0, 1, 1, 1, 1, 0),
};
DATASIZE(buf_hsync_fsync);

static const uint8_t buf_mem_align[] = {
    ID_BYTE_ID(0x10), 0x01, ID_BYTE_DATA(0x02), 0x03,
    ID_BYTE_DATA(0x04), 0x05, ID_BYTE_DATA(0x06), 0x07,
    ID_BYTE_ID(0x20), 0x08, ID_BYTE_DATA(0x09), 0x0A,
    ID_BYTE_DATA(0x0B), 0x0C, ID_BYTE_DATA(0x0D),
    FLAGS_BYTE(0, 0, 0, 0, 0, 1, 1, 1),
    ID_BYTE_DATA(0x0E), 0x0F, ID_BYTE_ID(0x30), 0x10,
    ID_BYTE_DATA(0x11), 0x12, ID_BYTE_DATA(0x13), 0x14,
    ID_BYTE_DATA(0x15), 0x16, ID_BYTE_ID(0x10), 0x17,
    ID_BYTE_DATA(0x18), 0x19, ID_BYTE_DATA(0x20),
    FLAGS_BYTE(0, 0, 1, 1, 1, 1, 0, 0),
};
DATASIZE(buf_mem_align);

static const uint8_t buf_mem_align_8id[] = {
    ID_BYTE_ID(0x10), 0x01, ID_BYTE_DATA(0x02), 0x03,
    ID_BYTE_DATA(0x04), 0x05, ID_BYTE_DATA(0x06), 0x07,
    ID_BYTE_ID(0x20), 0x08, ID_BYTE_DATA(0x09), 0x0A,
    ID_BYTE_DATA(0x0B), 0x0C, ID_BYTE_DATA(0x0D),
    FLAGS_BYTE(0, 0, 0, 0, 0, 1, 1, 1),
    // 8 IDs, all with prev flag
    ID_BYTE_ID(0x01), 0x0E, ID_BYTE_ID(0x02), 0x0F,
    ID_BYTE_ID(0x03), 0x10, ID_BYTE_ID(0x04), 0x11,
    ID_BYTE_ID(0x05), 0x12, ID_BYTE_ID(0x06), 0x13,
    ID_BYTE_ID(0x07), 0x14, ID_BYTE_DATA(0x50),
    FLAGS_BYTE(1, 1, 1, 1, 1, 1, 1, 1),
    ID_BYTE_DATA(0x15), 0x16, ID_BYTE_DATA(0x17), 0x18,
    ID_BYTE_DATA(0x19), 0x1A, ID_BYTE_DATA(0x1B), 0x1C,
    ID_BYTE_ID(0x20), 0x1D, ID_BYTE_DATA(0x1E), 0x1F,
    ID_BYTE_DATA(0x20), 0x21, ID_BYTE_DATA(0x22),
    FLAGS_BYTE(1, 1, 1, 1, 0, 0, 0, 0),
};
DATASIZE(buf_mem_align_8id);

static const uint8_t buf_mem_align_st_rst[] = {
    FSYNC_BYTES(), FSYNC_BYTES(), FSYNC_BYTES(), FSYNC_BYTES(),
    ID_BYTE_ID(0x10), 0x01, ID_BYTE_DATA(0x02), 0x03,
    ID_BYTE_DATA(0x04), 0x05, ID_BYTE_DATA(0x06), 0x07,
    ID_BYTE_ID(0x20), 0x08, ID_BYTE_DATA(0x09), 0x0A,
    ID_BYTE_DATA(0x0B), 0x0C, ID_BYTE_DATA(0x0D),
    FLAGS_BYTE(0, 0, 0, 0, 0, 1, 1, 1),
    ID_BYTE_DATA(0x0E), 0x0F, ID_BYTE_ID(0x30), 0x10,
    ID_BYTE_DATA(0x11), 0x12, ID_BYTE_DATA(0x13), 0x14,
    ID_BYTE_DATA(0x15), 0x16, ID_BYTE_ID(0x10), 0x17,
    ID_BYTE_DATA(0x18), 0x19, ID_BYTE_DATA(0x20),
    FLAGS_BYTE(0, 0, 1, 1, 1, 1, 0, 0),
};
DATASIZE(buf_mem_align_st_rst);

static const uint8_t buf_mem_align_mid_rst[] = {
    ID_BYTE_ID(0x10), 0x01, ID_BYTE_DATA(0x02), 0x03,
    ID_BYTE_DATA(0x04), 0x05, ID_BYTE_DATA(0x06), 0x07,
    ID_BYTE_ID(0x20), 0x08, ID_BYTE_DATA(0x09), 0x0A,
    ID_BYTE_DATA(0x0B), 0x0C, ID_BYTE_DATA(0x0D),
    FLAGS_BYTE(0, 0, 0, 0, 0, 1, 1, 1),
    FSYNC_BYTES(), FSYNC_BYTES(), FSYNC_BYTES(), FSYNC_BYTES(),
    ID_BYTE_DATA(0x0E), 0x0F, ID_BYTE_ID(0x30), 0x10,
    ID_BYTE_DATA(0x11), 0x12, ID_BYTE_DATA(0x13), 0x14,
    ID_BYTE_DATA(0x15), 0x16, ID_BYTE_ID(0x10), 0x17,
    ID_BYTE_DATA(0x18), 0x19, ID_BYTE_DATA(0x20),
    FLAGS_BYTE(0, 0, 1, 1, 1, 1, 0, 0),
};
DATASIZE(buf_mem_align_mid_rst);

static const uint8_t buf_mem_align_en_rst[] = {
    ID_BYTE_ID(0x10), 0x01, ID_BYTE_DATA(0x02), 0x03,
    ID_BYTE_DATA(0x04), 0x05, ID_BYTE_DATA(0x06), 0x07,
    ID_BYTE_ID(0x20), 0x08, ID_BYTE_DATA(0x09), 0x0A,
    ID_BYTE_DATA(0x0B), 0x0C, ID_BYTE_DATA(0x0D),
    FLAGS_BYTE(0, 0, 0, 0, 0, 1, 1, 1),
    ID_BYTE_DATA(0x0E), 0x0F, ID_BYTE_ID(0x30), 0x10,
    ID_BYTE_DATA(0x11), 0x12, ID_BYTE_DATA(0x13), 0x14,
    ID_BYTE_DATA(0x15), 0x16, ID_BYTE_ID(0x10), 0x17,
    ID_BYTE_DATA(0x18), 0x19, ID_BYTE_DATA(0x20),
    FLAGS_BYTE(0, 0, 1, 1, 1, 1, 0, 0),
    FSYNC_BYTES(), FSYNC_BYTES(), FSYNC_BYTES(), FSYNC_BYTES(),
};
DATASIZE(buf_mem_align_en_rst);

static const uint8_t buf_bad_data[] = {
0xff, 0xff, 0xff, 0x7f, 0x30, 0xff, 0x53, 0x54, 0x4d, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0, 0x36, 0xff, 0xb1, 0xff, 0x36, 0x36, 0x36, 0x36, 0x36, 0x2b,
0x36, 0x36, 0x3a, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
0x36, 0x36, 0x36, 0x36, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
0, 0x2c, 0, 0, 0, 0x32, 0x1, 0,
};
DATASIZE(buf_bad_data);

static ocsd_err_t initDecoder(int init_opts)
{
    pDecoder = DecodeTree::CreateDecodeTree(OCSD_TRC_SRC_FRAME_FORMATTED, init_opts);
    if (!pDecoder)
        return OCSD_ERR_MEM;
    return OCSD_OK;
}

static void destroyDecoder()
{
    delete pDecoder;
    pDecoder = 0;
}

static void printTestHeaderStr(const char* hdr_str)
{
    std::ostringstream oss;

    oss << "\n---------------------------------------------------------\n";
    oss << hdr_str;
    oss << "\n---------------------------------------------------------\n";
    logger.LogMsg(oss.str());
}

static void printSubTestName(const int test_num, const char* name)
{
    std::ostringstream oss;

    oss << "\n..Sub Test " << test_num << " : " << name << "\n";
    logger.LogMsg(oss.str());
}

static ocsd_err_t setConfig(uint32_t flags)
{
    TraceFormatterFrameDecoder* pFmt = pDecoder->getFrameDeformatter();
    return pFmt->Configure(flags);

}

// fail and print on none RESP_CONT response.
static ocsd_datapath_resp_t checkDataPathValue(ocsd_datapath_resp_t resp, int& failed_count)
{
    if (resp == OCSD_RESP_CONT)
        return resp;
    
    std::ostringstream oss;
    oss << "\nTest Datapath error response: " << ocsdDataRespStr(resp).getStr() << "\n";
    logger.LogMsg(oss.str());
    failed_count++;
    return resp;
}

static void resetDecoder(int& failed)
{
    checkDataPathValue(pDecoder->TraceDataIn(OCSD_OP_RESET, 0, 0, 0, 0), failed);
}


static void checkInOutSizes(const char *test, size_t in, size_t out, int& failed)
{
    if (in != out) {
        failed++;
        std::ostringstream oss;
        oss << test << " test failed - mismatch between processed and input sizes:";
        oss << " In=" << in << "; Out=" << out;
        logger.LogMsg(oss.str());
    }
}

static int checkResult(int failed)
{
    std::ostringstream oss;
    oss << "\nTEST : " << ((failed) ? "FAIL" : "PASS") << "\n";
    logger.LogMsg(oss.str());
    return failed;
}
static int testDemuxInit()
{
    ocsd_err_t err;
    std::ostringstream oss;
    int failed = 0;
    
    printTestHeaderStr("Demux Init Tests - check bad input rejected");

    // init with invalid no flags
    oss.str("");
    oss << "\nCheck 0 flag error: ";
    err = initDecoder(0);
    if (err) {
        err = err_log.GetLastError()->getErrorCode();
    }
    if (err != OCSD_ERR_INVALID_PARAM_VAL) {
        oss << "FAIL: expected error code not returned\n";
        failed++;
    }
    else
        oss << "PASS\n";
    logger.LogMsg(oss.str());

    // init with invalid unknown flags 
    oss.str("");
    oss << "\nCheck unknown flag error: ";
    err = initDecoder(0x80 | OCSD_DFRMTR_FRAME_MEM_ALIGN);
    if (err) {
        err = err_log.GetLastError()->getErrorCode();
    }
    if (err != OCSD_ERR_INVALID_PARAM_VAL) {
        oss << "FAIL: expected error code not returned\n";
        failed++;
    }
    else
        oss << "PASS\n";
    logger.LogMsg(oss.str());

    // init with bad combo
    oss.str("");
    oss << "\nCheck bad combination flag error: ";
    err = initDecoder(OCSD_DFRMTR_FRAME_MEM_ALIGN | OCSD_DFRMTR_HAS_FSYNCS);
    if (err) {
        err = err_log.GetLastError()->getErrorCode();
    }
    if (err != OCSD_ERR_INVALID_PARAM_VAL) {
        oss << "FAIL: expected error code not returned\n";
        failed++;
    }
    else
        oss << "PASS\n";
    logger.LogMsg(oss.str());

    return failed;
}

static int runDemuxBadDataTest() 
{

    int failed = 0;
    uint32_t processed = 0;
    std::ostringstream oss;
    ocsd_datapath_resp_t resp;

    printTestHeaderStr("Demux Bad Data Test - arbitrary test data input");

    setConfig(base_cfg | OCSD_DFRMTR_RESET_ON_4X_FSYNC);

    // reset the decoder.
    resetDecoder(failed);
    resp = checkDataPathValue(pDecoder->TraceDataIn(OCSD_OP_DATA, 0, buf_bad_data_sz, buf_bad_data, &processed), failed);
    if ((resp == OCSD_RESP_FATAL_INVALID_DATA) && 
        (err_log.GetLastError()->getErrorCode() == OCSD_ERR_DFMTR_BAD_FHSYNC))
    {
        failed--; // cancel the fail - we require that the error happens for bad input
        oss << "Got correct error response for invalid input\n";
    }
    else
    {
        oss << "Expected error code not returned\n";
    }
    logger.LogMsg(oss.str());

    setConfig(base_cfg);
    return checkResult(failed);
}

static int runHSyncFSyncTest()
{
    uint32_t cfg_flags = base_cfg;
    uint32_t processed = 0, total = 0;
    ocsd_trc_index_t index = 0;
    int failed = 0;
    ocsd_datapath_resp_t resp;
    std::ostringstream oss;

    printTestHeaderStr("FSYNC & HSYNC tests: check hander code for TPIU captures works.");

    // set for hsync / fsync operation
    cfg_flags &= ~OCSD_DFRMTR_FRAME_MEM_ALIGN; // clear mem align
    cfg_flags |= OCSD_DFRMTR_HAS_HSYNCS | OCSD_DFRMTR_HAS_FSYNCS;
    setConfig(cfg_flags);

    // straight frame test with fsync + hsync
    printSubTestName(1, "HSyncFSync frame");
    resetDecoder(failed);
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, index, buf_hsync_fsync_sz, buf_hsync_fsync, &processed),
        failed);
    checkInOutSizes("HSyncFSync frame", buf_hsync_fsync_sz, processed, failed);

    // test fsync broken across 2 input blocks
    printSubTestName(2, "HSyncFSync split frame");
    resetDecoder(failed);
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, index, 2, buf_hsync_fsync, &processed),
        failed);
    total += processed;
    index += processed;
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, index, buf_hsync_fsync_sz - processed, buf_hsync_fsync + processed, &processed),
        failed);
    total += processed;
    checkInOutSizes("HSyncFSync split frame", buf_hsync_fsync_sz, total, failed);

    // check bad input data is rejected.
    printSubTestName(3, "HSyncFSync bad input data");
    resetDecoder(failed);
    resp = checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, index, buf_bad_data_sz, buf_bad_data, &processed),
        failed);
    if ((resp == OCSD_RESP_FATAL_INVALID_DATA) &&
        (err_log.GetLastError()->getErrorCode() == OCSD_ERR_DFMTR_BAD_FHSYNC))
    {
        failed--; // cancel the fail - we require that the error happens for bad input
        oss << "Got correct error response for invalid input\n";
    }
    else
    {
        oss << "Expected error code not returned\n";
    }
    logger.LogMsg(oss.str());


    setConfig(base_cfg);
    return checkResult(failed);
}

static int runMemAlignTest()
{
    uint32_t processed = 0;
    int failed = 0;

    printTestHeaderStr("MemAligned Buffer tests: exercise the 16 byte frame buffer handler");

    // default decoder set to mem align so just run the test.

    // straight frame pair
    printSubTestName(1, "MemAlignFrame");
    resetDecoder(failed);
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, 0, buf_mem_align_sz, buf_mem_align, &processed),
        failed);
    checkInOutSizes("MemAlignFrame", buf_mem_align_sz, processed, failed);

    // frame with 8 id test
    printSubTestName(2, "MemAlignFrame-8-ID");
    resetDecoder(failed);
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, 0, buf_mem_align_8id_sz, buf_mem_align_8id, &processed),
        failed);
    checkInOutSizes("MemAlignFrame-8-ID", buf_mem_align_8id_sz, processed, failed);

    // check reset FSYNC frame handling
    setConfig(base_cfg | OCSD_DFRMTR_RESET_ON_4X_FSYNC);
    printSubTestName(3, "MemAlignFrame-rst_st");
    resetDecoder(failed);
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, 0, buf_mem_align_st_rst_sz, buf_mem_align_st_rst, &processed),
        failed);
    checkInOutSizes("MemAlignFrame-rst_st", buf_mem_align_st_rst_sz, processed, failed);

    printSubTestName(4, "MemAlignFrame-rst_mid");
    resetDecoder(failed);
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, 0, buf_mem_align_mid_rst_sz, buf_mem_align_mid_rst, &processed),
        failed);
    checkInOutSizes("MemAlignFrame-rst_mid", buf_mem_align_mid_rst_sz, processed, failed);

    printSubTestName(5, "MemAlignFrame-rst_en");
    resetDecoder(failed);
    checkDataPathValue(
        pDecoder->TraceDataIn(OCSD_OP_DATA, 0, buf_mem_align_en_rst_sz, buf_mem_align_en_rst, &processed),
        failed);
    checkInOutSizes("MemAlignFrame-rst_en", buf_mem_align_en_rst_sz, processed, failed);

    setConfig(base_cfg);
    return checkResult(failed);
}

int main(int argc, char* argv[])
{
    int failed = 0;
    ocsd_err_t err;
    std::ostringstream moss;
    RawFramePrinter* framePrinter = 0;

    /* initialise logger */
    
    static const int logOpts = ocsdMsgLogger::OUT_STDOUT | ocsdMsgLogger::OUT_FILE;
    
    logger.setLogOpts(logOpts);
    logger.setLogFileName("frame_demux_test.ppl");
    moss << "---------------------------------------------------------\n";
    moss << "Trace Demux Frame Test - check CoreSight frame processing\n";
    moss << "---------------------------------------------------------\n\n";
    moss << "** Library Version : " << ocsdVersion::vers_str() << "\n\n";
    logger.LogMsg(moss.str());

    /* initialise error logger */
    err_log.initErrorLogger(OCSD_ERR_SEV_INFO);
    err_log.setOutputLogger(&logger);
    DecodeTree::setAlternateErrorLogger(&err_log);

    /* run the init tests */
    failed += testDemuxInit();

    /* create a decoder for the remainder of the tests */
    err = initDecoder(base_cfg);
    moss.str("");
    moss << "Creating Decoder for active Demux testing\n";
    if (!err && pDecoder) {
        err = pDecoder->addRawFramePrinter(&framePrinter, OCSD_DFRMTR_PACKED_RAW_OUT | OCSD_DFRMTR_UNPACKED_RAW_OUT);
        if (err)
            moss << "Failed to add Frame printer\n";
    }
    if (err || !pDecoder) {

        moss << "Failed to initialise decoder for remainder of the tests\nSkipping active demux tests\n";
        failed++;
    }

    /* remainder of the tests that need an active decoder */
    if (!err) {
        try {
            failed += runMemAlignTest();
            failed += runHSyncFSyncTest();
            failed += runDemuxBadDataTest();
        }
        catch (ocsdError& err) {
            moss.str("");
            moss << "*** TEST ERROR: Unhandled error from tests. Aborting test run ***\n";
            moss << err.getErrorString(err) << "\n";
            logger.LogMsg(moss.str());
            failed++;
        }
    }

    /* testing done */
    moss.str("");
    moss << "\n\n---------------------------------------------------------\n";
    moss << "Trace Demux Testing Complete\n";
    if (failed)
        moss << "FAILED: recorded " << failed << " errors or failures.\n";
    else
        moss << "PASSED ALL tests\n";
    moss << "\n\n---------------------------------------------------------\n";

    logger.LogMsg(moss.str());

    if (pDecoder)
        destroyDecoder();

    return failed ? -1 : 0;
}
