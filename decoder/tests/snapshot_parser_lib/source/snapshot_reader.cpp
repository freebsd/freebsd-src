/*
 * \file       snapshot_reader.cpp
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

#include "snapshot_reader.h"
#include "snapshot_parser.h"
#include "device_parser.h"

#include "opencsd.h"

#include <fstream>
#include <iterator>
#include <ios>
#include <iostream>

#ifdef WIN32 
#define DIR_CHAR '\\'
#else
#define DIR_CHAR '/'
#endif

using namespace Parser;

SnapShotReader::SnapShotReader() :
    m_snapshotPath(""),
    m_snapshot_found(false),
    m_i_err_log(0),
    m_errlog_handle(0),
    m_verbose(true),
    m_read_ok(false)
{
    checkPath();    // see if default will work.
}

SnapShotReader::~SnapShotReader()
{
}

void SnapShotReader::setSnapshotDir(const std::string &dir)
{
    m_snapshotPath = dir;
    if(dir.size() > 0) 
    {
        if(dir.at(dir.size()-1) != DIR_CHAR)
            m_snapshotPath += DIR_CHAR;
    }
    m_read_ok = false;
    checkPath();
}

const bool SnapShotReader::readSnapShot()
{
    bool bRead = true;

    std::string iniFile = m_snapshotPath + SnapshotINIFilename;
    std::ifstream in(iniFile.c_str());
    std::ostringstream oss;

    if(in.is_open())
    {       
        Parser::SetVerboseLogging(m_verbose);
        ModernSnapshotParser parser(in);
        in.close();

        if(m_verbose)
        {
            oss.str("");
            oss << "Parsed snapshot.ini." << std::endl;
            oss << "Found " << parser.getDeviceCount() << " devices." << std::endl;
            LogInfo(oss.str());
        }

        std::vector<int> device_list;
        parser.getDeviceList(device_list);

        ModernSnapshotParser::DevPtr device;

        for(size_t i = 0; i < device_list.size(); i++)
        {
            device = parser.getDevice(device_list[i]);
            if(m_verbose)
            {
                oss.str("");
                oss << "Device " << device.get()->getID() << ": Ini file = " << device.get()->getIniFile() << "; Name = " << device.get()->getName() << std::endl;
                LogInfo(oss.str());
            }
            iniFile = m_snapshotPath + device.get()->getIniFile();
            in.open(iniFile.c_str());
            if(in.is_open())
            {
                Parser::Parsed pdev = Parser::ParseSingleDevice(in);
                m_parsed_device_list[pdev.deviceName] = pdev;   // map devices by name
                in.close();

            }
            else
            {
                oss.str("");
                oss << "Failed to open device file : " << iniFile << std::endl;
                LogError(oss.str());
            }
        }
        
        if(parser.getTraceMetadataFile().length() > 0)
        {
            if(m_verbose)
            {
                oss.str("");
                oss << "Trace Metadata ini file found : " << parser.getTraceMetadataFile() << std::endl;
                LogInfo(oss.str());
            }

            iniFile = m_snapshotPath + parser.getTraceMetadataFile();  
            in.open(iniFile.c_str());
            if(in.is_open())
            {
                m_parsed_trace = Parser::ParseTraceMetaData(in);
                in.close();
                if(m_parsed_trace.trace_buffers.size()) // found one or more buffers
                {
                    std::vector<std::string> bufferNames = GetBufferNameList(m_parsed_trace);
                    std::vector<std::string>::iterator bnit = bufferNames.begin();
                    while(bnit != bufferNames.end())
                    {
                        Parser::TraceBufferSourceTree tbst;
                        if(Parser::ExtractSourceTree(*bnit,m_parsed_trace,tbst))
                            m_source_trees[*bnit] = tbst;
                        bnit++;
                    }
                }

            }
            else
            {
                oss.str("");
                oss << "Failed to trace ini file : " << iniFile << std::endl;
                LogError(oss.str());
            }
        }
        else
        {
            oss.str("");
            oss << "Trace Metadata ini file not found." << std::endl;
            LogError(oss.str());
        }

        if(m_verbose)
        {
            oss.str("");
            oss << "Done." << std::endl;
            LogInfo(oss.str());
        }
    }
    else
    {
        oss.str("");
        oss << "Read Error : Failed to open " << iniFile << "." << std::endl;
        LogError(oss.str());
        bRead = false;
    }
    m_read_ok = bRead;
    return bRead;
}

void SnapShotReader::checkPath()
{
    std::string iniFile = m_snapshotPath + SnapshotINIFilename;
    std::ifstream in(iniFile.c_str());
    m_snapshot_found = false;

    if(in.is_open())
    {
        in.close();
        m_snapshot_found = true;
    }
}

void SnapShotReader::setErrorLogger(ITraceErrorLog *err_log) 
{ 
    if(err_log)
    {
        m_i_err_log = err_log;
        m_errlog_handle = m_i_err_log->RegisterErrorSource("snapshot_reader");   
        Parser::SetIErrorLogger(m_i_err_log);
    }
}

void SnapShotReader::LogInfo(const std::string &msg)
{
    if(m_i_err_log)
        m_i_err_log->LogMessage(m_errlog_handle, OCSD_ERR_SEV_INFO, msg);
}

void SnapShotReader::LogError(const std::string &msg)
{
    if(m_i_err_log)
    {
        ocsdError err(OCSD_ERR_SEV_ERROR,OCSD_ERR_TEST_SNAPSHOT_READ,msg);
        m_i_err_log->LogError(m_errlog_handle,&err);
    }
}

bool SnapShotReader::getSourceBufferNameList(std::vector<std::string> &nameList)
{
    nameList.clear();
    if(snapshotFound())
    {
        nameList = GetBufferNameList(m_parsed_trace);
    }
    return (bool)(nameList.size() > 0);
}

bool SnapShotReader::getTraceBufferSourceTree(const std::string &traceBufferName, Parser::TraceBufferSourceTree &sourceTree)
{
    bool found = false;
    std::map<std::string, Parser::TraceBufferSourceTree>::iterator it;
    it = m_source_trees.find(traceBufferName);
    if(it != m_source_trees.end())
    {
        sourceTree = it->second;
        found = true;
    }
    return found;
}

bool SnapShotReader::getDeviceData(const std::string &deviceName, Parser::Parsed **devData)
{
    std::map<std::string, Parser::Parsed>::iterator it;

    *devData = 0;
    it = m_parsed_device_list.find(deviceName);
    if(it != m_parsed_device_list.end())
    {
        *devData = &(it->second);
    }
    return (*devData != 0);
}


/* End of File snapshot_reader.cpp */
