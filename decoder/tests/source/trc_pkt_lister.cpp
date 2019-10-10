/*
 * \file       trc_pkt_lister.cpp
 * \brief      OpenCSD : Trace Packet Lister Test program
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

/* Test program / utility - list trace packets in supplied snapshot. */

#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <cstring>

#include "opencsd.h"              // the library
#include "trace_snapshots.h"    // the snapshot reading test library

static bool process_cmd_line_opts( int argc, char* argv[]);
static void ListTracePackets(ocsdDefaultErrorLogger &err_logger, SnapShotReader &reader, const std::string &trace_buffer_name);
static bool process_cmd_line_logger_opts(int argc, char* argv[]);
static void log_cmd_line_opts(int argc, char* argv[]);

    // default path
#ifdef WIN32
static std::string ss_path = ".\\";
#else
static std::string ss_path = "./";
#endif

static std::string source_buffer_name = "";    // source name - used if more than one source
static bool all_source_ids = true;      // output all IDs in source.
static std::vector<uint8_t> id_list;    // output specific IDs in source

static ocsdMsgLogger logger;
static int logOpts = ocsdMsgLogger::OUT_STDOUT | ocsdMsgLogger::OUT_FILE;
static std::string logfileName = "trc_pkt_lister.ppl";
static bool outRawPacked = false;
static bool outRawUnpacked = false;
static bool ss_verbose = false;
static bool decode = false;
static bool no_undecoded_packets = false;
static bool pkt_mon = false;
static int test_waits = 0;
static bool dstream_format = false;
static bool tpiu_format = false;
static bool has_hsync = false;

int main(int argc, char* argv[])
{
    std::ostringstream moss;
    
    if(process_cmd_line_logger_opts(argc,argv))
    {
        printf("Bad logger command line options\nProgram Exiting\n");
        return -2;
    }

    logger.setLogOpts(logOpts);
    logger.setLogFileName(logfileName.c_str());

    moss << "Trace Packet Lister: CS Decode library testing\n";
    moss << "-----------------------------------------------\n\n";
    moss << "** Library Version : " << ocsdVersion::vers_str() << "\n\n";
    logger.LogMsg(moss.str());

    log_cmd_line_opts(argc,argv);

    ocsdDefaultErrorLogger err_log;
    err_log.initErrorLogger(OCSD_ERR_SEV_INFO);
    err_log.setOutputLogger(&logger);

    if(!process_cmd_line_opts(argc, argv))
        return -1;

    moss.str("");
    moss << "Trace Packet Lister : reading snapshot from path " << ss_path << "\n";
    logger.LogMsg(moss.str());

    SnapShotReader ss_reader;
    ss_reader.setSnapshotDir(ss_path);
    ss_reader.setErrorLogger(&err_log);
    ss_reader.setVerboseOutput(ss_verbose);

    if(ss_reader.snapshotFound())
    {
        if(ss_reader.readSnapShot())
        {
            std::vector<std::string> sourceBuffList;
            if(ss_reader.getSourceBufferNameList(sourceBuffList))
            {
                bool bValidSourceName = false;
                // check source name list
                if(source_buffer_name.size() == 0)
                {
                    // default to first in the list
                    source_buffer_name = sourceBuffList[0];
                    bValidSourceName = true;
                }
                else
                {
                    for(size_t i = 0; i < sourceBuffList.size(); i++)
                    {
                        if(sourceBuffList[i] == source_buffer_name)
                        {
                            bValidSourceName = true;
                            break;
                        }
                    }
                }

                if(bValidSourceName)
                {
                    std::ostringstream oss;
                    oss << "Using " << source_buffer_name << " as trace source\n";
                    logger.LogMsg(oss.str());
                    ListTracePackets(err_log,ss_reader,source_buffer_name);
                }
                else
                {
                    std::ostringstream oss;
                    oss << "Trace Packet Lister : Trace source name " << source_buffer_name << " not found\n";
                    logger.LogMsg(oss.str());
                    oss.str("");
                    oss << "Valid source names are:-\n";
                    for(size_t i = 0; i < sourceBuffList.size(); i++)
                    {
                        oss << sourceBuffList[i] << "\n";
                    }
                    logger.LogMsg(oss.str());
                }

            }
            else
                logger.LogMsg("Trace Packet Lister : No trace source buffer names found\n");
        }
        else
            logger.LogMsg("Trace Packet Lister : Failed to read snapshot\n");
    }
    else
    {
        std::ostringstream oss;
        oss << "Trace Packet Lister : Snapshot path" << ss_path << " not found\n";
        logger.LogMsg(oss.str());
    }

    return 0;
}

void print_help()
{
    std::ostringstream oss;
    oss << "Trace Packet Lister - commands\n\n";
    oss << "Snapshot:\n\n";
    oss << "-ss_dir <dir>       Set the directory path to a trace snapshot\n";
    oss << "-ss_verbose         Verbose output when reading the snapshot\n";
    oss << "\nDecode:\n\n";
    oss << "-id <n>             Set an ID to list (may be used multiple times) - default if no id set is for all IDs to be printed\n";
    oss << "-src_name <name>    List packets from a given snapshot source name (defaults to first source found)\n";
    oss << "-dstream_format     Input is DSTREAM framed.";
    oss << "-tpiu               Input from TPIU - sync by FSYNC.";
    oss << "-tpiu_hsync         Input from TPIU - sync by FSYNC and HSYNC.";
    oss << "-decode             Full decode of the packets from the trace snapshot (default is to list undecoded packets only\n";
    oss << "-decode_only        Does not list the undecoded packets, just the trace decode.\n";
    oss << "-o_raw_packed       Output raw packed trace frames\n";
    oss << "-o_raw_unpacked     Output raw unpacked trace data per ID\n";
    oss << "-test_waits <N>     Force wait from packet printer for N packets - test the wait/flush mechanisms for the decoder\n";
    oss << "\nOutput:\n";
    oss << "   Setting any of these options cancels the default output to file & stdout,\n   using _only_ the options supplied.\n\n";
    oss << "-logstdout          Output to stdout -> console.\n";
    oss << "-logstderr          Output to stderr.\n";
    oss << "-logfile            Output to default file - " << logfileName << "\n";
    oss << "-logfilename <name> Output to file <name> \n";


    logger.LogMsg(oss.str());
}

void log_cmd_line_opts(int argc, char* argv[])
{
    std::ostringstream oss;
    oss << "Test Command Line:-\n";
    oss << argv[0] << "   ";
    for(int i = 1; i < argc; i++)
    {
        oss << argv[i] << "  ";
    }
    oss << "\n\n";
    logger.LogMsg(oss.str());
}

// true if element ID filtered out
bool element_filtered(uint8_t elemID)
{
    bool filtered = false;
    if(!all_source_ids)
    {
        filtered = true;
        std::vector<uint8_t>::const_iterator it;
        it = id_list.begin();
        while((it != id_list.end()) && filtered)
        {
            if(*it == elemID)
                filtered = false;
            it++;
        }
    }
    return filtered;
}

bool process_cmd_line_logger_opts(int argc, char* argv[])
{
    bool badLoggerOpts = false;
    bool bChangingOptFlags = false;
    int newlogOpts = ocsdMsgLogger::OUT_NONE;
    std::string opt;
    if(argc > 1)
    {
        int options_to_process = argc - 1;
        int optIdx = 1;
        while(options_to_process > 0)
        {
            opt = argv[optIdx];
            if(opt == "-logstdout")
            {
                newlogOpts |= ocsdMsgLogger::OUT_STDOUT;
                bChangingOptFlags = true;
            }
            else if(opt == "-logstderr")
            {
                newlogOpts |= ocsdMsgLogger::OUT_STDERR;
                bChangingOptFlags = true;
            }
            else if(opt == "-logfile")
            {
                newlogOpts |= ocsdMsgLogger::OUT_FILE;
                bChangingOptFlags = true;
            }
            else if(opt == "-logfilename")
            {
                options_to_process--;
                optIdx++;
                if(options_to_process)
                {
                    logfileName = argv[optIdx];
                    newlogOpts |= ocsdMsgLogger::OUT_FILE;
                    bChangingOptFlags = true;
                }
                else
                {
                    badLoggerOpts = true;
                }
            }
            options_to_process--;
            optIdx++;
        }
    }
    if(bChangingOptFlags)
        logOpts = newlogOpts;
    return badLoggerOpts;
}

bool process_cmd_line_opts(int argc, char* argv[])
{
    bool bOptsOK = true;
    std::string opt;
    if(argc > 1)
    {
        int options_to_process = argc - 1;
        int optIdx = 1;
        while((options_to_process > 0) && bOptsOK)
        {
            opt = argv[optIdx];
            if(opt == "-ss_dir")
            {
                options_to_process--;
                optIdx++;
                if(options_to_process)
                    ss_path = argv[optIdx];
                else
                {
                    logger.LogMsg("Trace Packet Lister : Error: Missing directory string on -ss_dir option\n");
                    bOptsOK = false;
                }
            }
            else if(opt == "-id")
            {
                options_to_process--;
                optIdx++;
                if(options_to_process)
                {
                    uint8_t Id = (uint8_t)strtoul(argv[optIdx],0,0);
                    if((Id == 0) || (Id >= 0x70))
                    {                        
                        std::ostringstream iderrstr;
                        iderrstr << "Trace Packet Lister : Error: invalid ID number 0x" << std::hex << (uint32_t)Id << " on -id option" << std::endl;
                        logger.LogMsg(iderrstr.str());
                        bOptsOK = false;
                    }
                    else
                    {
                        all_source_ids = false;
                        id_list.push_back(Id);
                    }                   
                }
                else
                {
                    logger.LogMsg("Trace Packet Lister : Error: No ID number on -id option\n");
                    bOptsOK = false;
                }
            }
            else if(strcmp(argv[optIdx], "-src_name") == 0)
            {
                options_to_process--;
                optIdx++;
                if(options_to_process)
                    source_buffer_name = argv[optIdx];
                else
                {
                    logger.LogMsg("Trace Packet Lister : Error: Missing source name string on -src_name option\n");
                    bOptsOK = false;
                }
            }
            else if(strcmp(argv[optIdx], "-test_waits") == 0)
            {
                options_to_process--;
                optIdx++;
                if(options_to_process)
                {
                    test_waits = (int)strtol(argv[optIdx],0,0);
                    if(test_waits < 0)
                        test_waits = 0;
                }
                else
                {
                    logger.LogMsg("Trace Packet Lister : Error: wait count value on -test_waits option\n");
                    bOptsOK = false;
                }
            }
            else if(strcmp(argv[optIdx], "-o_raw_packed") == 0)
            {
                outRawPacked = true;     
            }
            else if(strcmp(argv[optIdx], "-o_raw_unpacked") == 0)
            {
                outRawUnpacked = true;
            }
            else if(strcmp(argv[optIdx], "-ss_verbose") == 0)
            {
                ss_verbose = true;
            }
            else if(strcmp(argv[optIdx], "-decode") == 0)
            {
                decode = true;              
            }
            else if(strcmp(argv[optIdx], "-pkt_mon") == 0)
            {
                pkt_mon = true;              
            }
            else if(strcmp(argv[optIdx], "-decode_only") == 0)
            {
                no_undecoded_packets = true;
                decode = true; 
            }
            else if((strcmp(argv[optIdx], "-help") == 0) || (strcmp(argv[optIdx], "--help") == 0) || (strcmp(argv[optIdx], "-h") == 0))
            {
                print_help();
                bOptsOK = false;
            }
            else if((opt == "-logstdout") || (opt == "-logstderr") || 
                (opt == "-logfile") || (opt == "-logfilename"))
            {
                // skip all these as processed earlier

                // also additionally skip any filename parameter
                if(opt == "-logfilename")
                {
                    options_to_process--;
                    optIdx++;
                }
            }
            else if (strcmp(argv[optIdx], "-dstream_format") == 0)
            {
                dstream_format = true;
            }
            else if (strcmp(argv[optIdx], "-tpiu") == 0)
            {
                tpiu_format = true;
            }
            else if (strcmp(argv[optIdx], "-tpiu_hsync") == 0)
            {
                has_hsync = true;
                tpiu_format = true;
            }
            else
            {
                std::ostringstream errstr;
                errstr << "Trace Packet Lister : Warning: Ignored unknown option " << argv[optIdx] << "." << std::endl;
                logger.LogMsg(errstr.str());
            }
            options_to_process--;
            optIdx++;
        }
        
    }
    return bOptsOK;
}

//
// if decoding the gen elem printer will be injecting waits, but we may ge a cont from the packet processors if a complete packet is not available.
// if packet processing only, then waits will be coming from there until the count is extinguished
// wait testing with packet processor only really works correctly if we are doing a single source as there is no way at this 
// point to know which source has sent the _WAIT. with multi packet processor waiting may get false warnings once the _WAITs run out.
bool ExpectingPPrintWaitResp(DecodeTree *dcd_tree, TrcGenericElementPrinter &genElemPrinter)
{
    bool ExpectingWaits = false;
    std::vector<ItemPrinter *> &printers = dcd_tree->getPrinterList();
    if(test_waits > 0)
    {
        // see if last response was from the Gen elem printer expecting a wait
        ExpectingWaits = genElemPrinter.needAckWait();

        // now see if any of the active packet printers are returing wait responses.
        if(!ExpectingWaits)
        {
            std::vector<ItemPrinter *>::iterator it;
            it = printers.begin();
            while((it != printers.end()) && !ExpectingWaits)
            {
                ExpectingWaits = (bool)((*it)->getTestWaits() != 0);
                it++;
            }
        }

        // nothing waiting - and no outstanding wait cycles in the Gen elem printer.
        if(!ExpectingWaits && (genElemPrinter.getTestWaits() == 0))
            test_waits = 0;     // zero out the input value if none of the printers currently have waits scheduled.
    }
    return ExpectingWaits;
}

void AttachPacketPrinters( DecodeTree *dcd_tree)
{
    uint8_t elemID;
    std::ostringstream oss;

    // attach packet printers to each trace source in the tree
    DecodeTreeElement *pElement = dcd_tree->getFirstElement(elemID);
    while(pElement && !no_undecoded_packets)
    {
        if(!element_filtered(elemID))
        {
            oss.str("");

            ItemPrinter *pPrinter;
            ocsd_err_t err = dcd_tree->addPacketPrinter(elemID, (bool)(decode || pkt_mon),&pPrinter);
            if (err == OCSD_OK)
            {
                // if not decoding or monitor only
                if((!(decode || pkt_mon)) && test_waits)
                    pPrinter->setTestWaits(test_waits);

                oss << "Trace Packet Lister : Protocol printer " << pElement->getDecoderTypeName() <<  " on Trace ID 0x" << std::hex << (uint32_t)elemID << "\n";
            }
            else
                oss << "Trace Packet Lister : Failed to Protocol printer " << pElement->getDecoderTypeName() << " on Trace ID 0x" << std::hex << (uint32_t)elemID << "\n";
            logger.LogMsg(oss.str());

        }
        pElement = dcd_tree->getNextElement(elemID);
    }

}

void ConfigureFrameDeMux(DecodeTree *dcd_tree, RawFramePrinter **framePrinter)
{
    // configure the frame deformatter, and attach a frame printer to the frame deformatter if needed
    TraceFormatterFrameDecoder *pDeformatter = dcd_tree->getFrameDeformatter();
    if(pDeformatter != 0)
    {
        // configuration - memory alinged buffer
        uint32_t configFlags = pDeformatter->getConfigFlags();

        // check for TPIU FSYNC & HSYNC
        if (tpiu_format) configFlags |= OCSD_DFRMTR_HAS_FSYNCS;
        if (has_hsync) configFlags |= OCSD_DFRMTR_HAS_HSYNCS;
        // if FSYNC (& HSYNC) - cannot be mem frame aligned.
        if (tpiu_format) configFlags &= ~OCSD_DFRMTR_FRAME_MEM_ALIGN;

        if (!configFlags)
        {
            configFlags = OCSD_DFRMTR_FRAME_MEM_ALIGN;
            pDeformatter->Configure(configFlags);
        }
        if (outRawPacked || outRawUnpacked)
        {
            if (outRawPacked) configFlags |= OCSD_DFRMTR_PACKED_RAW_OUT;
            if (outRawUnpacked) configFlags |= OCSD_DFRMTR_UNPACKED_RAW_OUT;
            dcd_tree->addRawFramePrinter(framePrinter, configFlags);
        }
    }
}

void ListTracePackets(ocsdDefaultErrorLogger &err_logger, SnapShotReader &reader, const std::string &trace_buffer_name)
{
    CreateDcdTreeFromSnapShot tree_creator;

    tree_creator.initialise(&reader, &err_logger);

    if(tree_creator.createDecodeTree(trace_buffer_name, (decode == false)))
    {
        DecodeTree *dcd_tree = tree_creator.getDecodeTree();
        dcd_tree->setAlternateErrorLogger(&err_logger);

        RawFramePrinter *framePrinter = 0;
        TrcGenericElementPrinter *genElemPrinter = 0;

        AttachPacketPrinters(dcd_tree);

        ConfigureFrameDeMux(dcd_tree, &framePrinter);

        // if decoding set the generic element printer to the output interface on the tree.
        if(decode)
        {
            std::ostringstream oss;
            //dcd_tree->setGenTraceElemOutI(genElemPrinter);
            dcd_tree->addGenElemPrinter(&genElemPrinter);
            oss << "Trace Packet Lister : Set trace element decode printer\n";
            logger.LogMsg(oss.str());
            genElemPrinter->setTestWaits(test_waits);
        }

        if(decode)
            dcd_tree->logMappedRanges();    // print out the mapped ranges

         // check if we have attached at least one printer
        if(decode || (PktPrinterFact::numPrinters(dcd_tree->getPrinterList()) > 0))
        {
            // set up the filtering at the tree level (avoid pushing to processors with no attached printers)
            if(!all_source_ids)
                dcd_tree->setIDFilter(id_list);
            else
                dcd_tree->clearIDFilter();

            // need to push the data through the decode tree.
            std::ifstream in;
            in.open(tree_creator.getBufferFileName(),std::ifstream::in | std::ifstream::binary);
            if(in.is_open())
            {
                ocsd_datapath_resp_t dataPathResp = OCSD_RESP_CONT;
                static const int bufferSize = 1024;
                uint8_t trace_buffer[bufferSize];   // temporary buffer to load blocks of data from the file
                uint32_t trace_index = 0;           // index into the overall trace buffer (file).

                // process the file, a buffer load at a time
                while(!in.eof() && !OCSD_DATA_RESP_IS_FATAL(dataPathResp))
                {
                    if (dstream_format)
                    { 
                        in.read((char *)&trace_buffer[0], 512 - 8);
                    }
                    else
                        in.read((char *)&trace_buffer[0],bufferSize);   // load a block of data into the buffer

                    std::streamsize nBuffRead = in.gcount();    // get count of data loaded.
                    std::streamsize nBuffProcessed = 0;         // amount processed in this buffer.
                    uint32_t nUsedThisTime = 0;

                    // process the current buffer load until buffer done, or fatal error occurs
                    while((nBuffProcessed < nBuffRead) && !OCSD_DATA_RESP_IS_FATAL(dataPathResp))
                    {
                        if(OCSD_DATA_RESP_IS_CONT(dataPathResp))
                        {
                            dataPathResp = dcd_tree->TraceDataIn(
                                OCSD_OP_DATA,
                                trace_index,
                                (uint32_t)(nBuffRead - nBuffProcessed),
                                &(trace_buffer[0])+nBuffProcessed,
                                &nUsedThisTime);

                            nBuffProcessed += nUsedThisTime;
                            trace_index += nUsedThisTime;

                            // test printers can inject _WAIT responses - see if we are expecting one...
                            if(ExpectingPPrintWaitResp(dcd_tree, *genElemPrinter))
                            {
                                if(OCSD_DATA_RESP_IS_CONT(dataPathResp))
                                {
                                    // not wait or fatal - log a warning here.
                                    std::ostringstream oss;
                                    oss << "Trace Packet Lister : WARNING : Data in; data Path expected WAIT response\n";
                                    logger.LogMsg(oss.str());
                                }
                            }
                        }
                        else // last response was _WAIT
                        {
                            // may need to acknowledge a wait from the gen elem printer
                            if(genElemPrinter->needAckWait())
                                genElemPrinter->ackWait();

                            // dataPathResp not continue or fatal so must be wait...
                            dataPathResp = dcd_tree->TraceDataIn(OCSD_OP_FLUSH,0,0,0,0);
                        }
                    }

                    /* dump dstream footers */
                    if (dstream_format) {
                        in.read((char *)&trace_buffer[0], 8);
                        if (outRawPacked)
                        {
                            std::ostringstream oss;
                            oss << "DSTREAM footer [";
                            for (int i = 0; i < 8; i++)
                            {
                                oss << "0x" << std::hex << (int)trace_buffer[i] << " ";
                            }
                            oss << "]\n";
                            logger.LogMsg(oss.str());
                        }
                    }
                }

                // fatal error - no futher processing
                if(OCSD_DATA_RESP_IS_FATAL(dataPathResp))
                {
                    std::ostringstream oss;
                    oss << "Trace Packet Lister : Data Path fatal error\n";
                    logger.LogMsg(oss.str());
                    ocsdError *perr = err_logger.GetLastError();
                    if(perr != 0)
                        logger.LogMsg(ocsdError::getErrorString(perr));

                }
                else
                {
                    // mark end of trace into the data path
                    dcd_tree->TraceDataIn(OCSD_OP_EOT,0,0,0,0);
                }

                // close the input file.
                in.close();

                std::ostringstream oss;
                oss << "Trace Packet Lister : Trace buffer done, processed " << trace_index << " bytes.\n";
                logger.LogMsg(oss.str());

            }
            else
            {
                std::ostringstream oss;
                oss << "Trace Packet Lister : Error : Unable to open trace buffer.\n";
                logger.LogMsg(oss.str());
            }

        }
        else
        {
                std::ostringstream oss;
                oss << "Trace Packet Lister : No supported protocols found.\n";
                logger.LogMsg(oss.str());
        }

        // clean up

        // get rid of the decode tree.
        tree_creator.destroyDecodeTree();
    }
}

/* End of File trc_pkt_lister.cpp */
