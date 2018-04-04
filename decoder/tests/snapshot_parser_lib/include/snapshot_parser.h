/*
 * \file       snapshot_parser.h
 * \brief      OpenCSD : Snapshot Parser Library
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

#ifndef ARM_SNAPSHOT_PARSER_H_INCLUDED
#define ARM_SNAPSHOT_PARSER_H_INCLUDED

#include <cstdint>
#include <sstream>
#include <exception>
#include <string>
#include <vector>
#include <map>
#include <istream>

#include "snapshot_parser_util.h"
#include "snapshot_info.h"

class ITraceErrorLog;   // forward declare the OCSD error log interface.

namespace Parser
{
    //! \brief Stores a parsed [dump] section
    struct DumpDef
    {
        uint64_t     address;
        std::string     path;
        std::size_t     length;
        std::size_t     offset;
        std::string     space;
    };



    //! \brief Stores the entire parsed device ini file
    struct Parsed
    {
        Parsed() : foundGlobal() {}

        bool                                foundGlobal;
        std::string                         core;
        std::vector<DumpDef>                dumpDefs;
        std::map<std::string, 
                 std::string, // register value is stored as a string initially to cope with > 64 bit registers
                 Util::CaseInsensitiveLess> regDefs;
        std::map<uint32_t, uint32_t>            extendRegDefs;
        std::string                         deviceName;
        std::string                         deviceClass;
        std::string                         deviceTypeName;  // Cortex-Ax or ETMvN


    };
    /*! \brief Parse the device ini file and call back on the builder as appropriate.
     *  \param input the ini file
     *  \return parsed definitions
     */
    Parsed ParseSingleDevice(std::istream& input);

    //! \brief Stores the entire device list
    struct ParsedDevices
    {
        std::map<std::string, std::string>  deviceList;
        SnapshotInfo                        snapshotInfo;
        std::string                         traceMetaDataName;
    };

    /*! \brief Parse the snapshot.ini file that contains the device list and call back on the builder as appropriate.
     *  \param input the ini file
     *  \return parsed definitions
     */
    ParsedDevices ParseDeviceList(std::istream& input);

    // basic info about the buffer
    struct TraceBufferInfo 
    {
        std::string bufferName;
        std::string dataFileName;
        std::string dataFormat;
    };

    // list of buffers and associations as presented in the ini file.
    struct ParsedTrace 
    {
        std::vector<std::string> buffer_section_names;
        std::vector<TraceBufferInfo> trace_buffers;
        std::map<std::string, std::string> source_buffer_assoc;  // trace source name -> trace buffer name assoc
        std::map<std::string, std::string> cpu_source_assoc;    // trace source name -> cpu_name assoc
    };

    // single buffer information containing just the assoc for the buffer 
    // -> created by processing the ini data for a single named buffer.
    // this can then be used to create a decode tree in the decode library.
    struct TraceBufferSourceTree
    {
        TraceBufferInfo buffer_info;    
        std::map<std::string, std::string> source_core_assoc;    // list of source names attached to core device names (e.g. ETM_0:cpu_0)
    };

    // parse the trace metadata ini file.
    ParsedTrace ParseTraceMetaData(std::istream& input);

    // build a source tree for a single buffer
    bool ExtractSourceTree(const std::string &buffer_name, ParsedTrace &metadata, TraceBufferSourceTree &buffer_data);

    std::vector<std::string> GetBufferNameList(ParsedTrace &metadata);


    static ITraceErrorLog *s_pErrorLogger = 0;
    static ocsd_hndl_err_log_t s_errlog_handle = 0;
    static bool s_verbose_logging = true;

    void SetIErrorLogger(ITraceErrorLog *i_err_log);
    void SetVerboseLogging(bool verbose);
    ITraceErrorLog *GetIErrorLogger();
    void LogInfoStr(const std::string &logMsg);


}

#endif // ARM_SNAPSHOT_PARSER_H_INCLUDED

/* End of File snapshot_parser.h */
