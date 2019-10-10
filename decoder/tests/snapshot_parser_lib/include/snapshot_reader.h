/*
 * \file       snapshot_reader.h
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

#ifndef ARM_SNAPSHOT_READER_H_INCLUDED
#define ARM_SNAPSHOT_READER_H_INCLUDED

#include <string>
#include <vector>

#include "snapshot_parser.h"

class ITraceErrorLog;

class SnapShotReader
{
public:
    SnapShotReader();
    ~SnapShotReader();

    void setSnapshotDir(const std::string &dir);
    const std::string &getSnapShotDir() const { return m_snapshotPath; };

    const bool readSnapShot();    // read the snapshot dir 

    // true if the snapshot ini file can be found on the path.
    const bool snapshotFound() const { return m_snapshot_found; };

    const bool snapshotReadOK() const { return m_read_ok; };
    
    void setErrorLogger(ITraceErrorLog *err_log);
    void setVerboseOutput(const bool bVerbose) { m_verbose = bVerbose; };

    bool getSourceBufferNameList(std::vector<std::string> &nameList);
    bool getTraceBufferSourceTree(const std::string &traceBufferName, Parser::TraceBufferSourceTree &sourceTree);

    bool getDeviceData(const std::string &deviceName, Parser::Parsed **devData);

private:
    void checkPath();   // see if the ini file can be opened on the current path
    void LogInfo(const std::string &msg);
    void LogError(const std::string &msg);


    std::string m_snapshotPath; // snapshot directory - default to cwd.
    bool m_snapshot_found;  // true if the path supplied can be opened.
    ITraceErrorLog *m_i_err_log;
    ocsd_hndl_err_log_t m_errlog_handle;
    bool m_verbose; // true for verbose output.
    bool m_read_ok;
    
    // list of parsed device ini files, mapped by device name .
    // <device_name (ETM_0 | cpu_0)> : { <parsed_device_data> }
    std::map<std::string, Parser::Parsed> m_parsed_device_list; 

    Parser::ParsedTrace m_parsed_trace; // the parsed trace meta data

    // trace metadata rearranged as source trees, mapped by source name
    // <source_buffer_name> : { buffer_info, {<ETM_0:cpu_0>,....} }
    std::map<std::string, Parser::TraceBufferSourceTree> m_source_trees; 
};

// the top level snapshot file name.
const char* const SnapshotINIFilename("snapshot.ini");

#endif // ARM_SNAPSHOT_READER_H_INCLUDED

/* End of File snapshot_reader.h */
