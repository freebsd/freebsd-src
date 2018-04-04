/*
 * \file       snapshot_parser.cpp
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

#include "snapshot_parser.h"

#include <memory>
#include <algorithm>
#include <istream>
#include <iostream>
#include <string>
#include <utility>
using namespace std;

#include "snapshot_parser_util.h"
#include "ini_section_names.h"
using namespace Util;
using namespace Parser;

#include "opencsd.h"

/*************************************************************************
 * Note, this file handles the parsring of the general (device specific) 
 * ini file and the (much smaller) device_list file
 *************************************************************************/

namespace ParserPrivate
{
    //! Handle CRLF terminators and '#' and ';' comments
    void CleanLine(string& line)
    {
        string::size_type endpos = line.find_first_of("\r;#");
        if (endpos != string::npos)
        {
            line.erase(endpos);
        }
    }

    //! Split foo=bar into pair <foo, bar>
    pair<string, string> SplitKeyValue(const string& kv)
    {
        string::size_type eq(kv.find('='));
        if (eq == string::npos)
        {
            throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE, "Couldn't parse '" + kv + "' as key=value"); 
        }
        return make_pair(Trim(kv.substr(0, eq)), Trim(kv.substr(eq + 1)));
    }

    //! Whether line is just tabs and spaces
    bool IsEmpty(const string& line)
    {
        return TrimLeft(line) == "";
    }

    /*! \brief Whether line is of form '[header]'
     *  \param line the line
     *  \param sectionName if function returns true, returns the text between the brackets
     */
    bool IsSectionHeader(const string& line, string& sectionName)
    {
        string::size_type openBracket(line.find('['));
        if (openBracket == string::npos)
        {
            return false;
        }
        string::size_type textStart(openBracket + 1);
        string::size_type closeBracket(line.find(']', textStart));
        if (closeBracket == string::npos)
        {
            return false;
        }
        sectionName.assign(Trim(line.substr(textStart, closeBracket - textStart)));
        return true;
    }

    template <class M, class K, class V> 
    void AddUniqueKey(M& m, const K& key, const V& value, const std::string &keyStr )
    {
        if (!m.insert(make_pair(key, value)).second)
        {
            throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE,  "Duplicate key: " + keyStr);
        }
    }

    void PreventDupes(bool& store, const string& key, const string& section)
    {
        if (store)
        {
            throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE, 
                "Duplicate " + key + " key found in "
                + section + " section"); 
        }
        store = true;
    }


    /*! \class Section
     *  \brief Handle an ini file section begun with a section header ([header])
     */
    class Section
    {
    public:
        virtual ~Section() {}

        //! Notify a key=value definition
        virtual void Define(const string& k, const string& v) = 0;

        //! Notify end of section - we can't handle in dtor because misparses throw. 
        virtual void End() = 0;
    };

    //! The initial state
    class NullSection : public Section
    {
    public:
        void Define(const string& k, const string&)
        {
            throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE,  "Definition of '" + k + "' has no section header");
        }
        void End() {}
    };

    //! Silently ignore sections that are undefined
    class IgnoredSection : public Section
    {
    public:
        void Define(const string& , const string&)
        {
        }
        void End() {}
    };

    //! Handle a [global] section.
    class GlobalSection : public Section
    {
    public:
        GlobalSection(Parsed& result) : m_result(result), m_got_core()
        {
            if (m_result.foundGlobal)
            {
                throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE,  string("Only one ") + GlobalSectionName + " section allowed");
            }
            m_result.foundGlobal = true;
        }

        void Define(const string& k, const string& v)
        {
            if (k == CoreKey)
            {
                PreventDupes(m_got_core, CoreKey, GlobalSectionName);
                m_result.core.assign(v);
            }
            else
            {
                throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE, "Unknown global option '" + k + '\'');
            }
        }

        void End() {}

    private:
        Parsed&     m_result;
        bool        m_got_core;
    };

    //! Handle a [dump] section
    class DumpSection : public Section
    {
    public:
        DumpSection(Parsed& result) 
          : m_result(result),
            m_got_file(), m_got_address(), m_got_length(), m_got_offset(), m_got_space(),
            m_address(), m_length(), m_offset(), m_file(), m_space()
        {}

        void Define(const string& k, const string& v)
        {
            if (k == DumpAddressKey)
            {
                PreventDupes(m_got_address, DumpAddressKey, DumpFileSectionPrefix);
                m_address = DecodeUnsigned<uint64_t>(v);
            }
            else if (k == DumpLengthKey)
            {
                PreventDupes(m_got_length, DumpLengthKey, DumpFileSectionPrefix);
                m_length = DecodeUnsigned<size_t>(v);
            }
            else if (k == DumpOffsetKey)
            {
                PreventDupes(m_got_offset, DumpOffsetKey, DumpFileSectionPrefix);
                m_offset = DecodeUnsigned<size_t>(v);
            }
            else if (k == DumpFileKey)
            {
                PreventDupes(m_got_file, DumpFileKey, DumpFileSectionPrefix);
                m_file = Trim(v, "\"'"); // strip quotes
            }
            else if (k == DumpSpaceKey)
            {
                PreventDupes(m_got_space, DumpSpaceKey, DumpFileSectionPrefix);
                m_space = Trim(v, "\"'"); // strip quotes
            }
            else 
            {
                throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE,  "Unknown dump section key '" + k + '\'');
            }
        }

        void End()
        {
            if (!m_got_address)
            {
                throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE,  "Dump section is missing mandatory address definition");
            }
            if (!m_got_file)
            {
                throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE, "Dump section is missing mandatory file definition");
            }
            
            struct DumpDef add = { m_address, m_file, m_length, m_offset, m_space};
            m_result.dumpDefs.push_back(add);
        }

    private:
        Parsed&     m_result;
        bool        m_got_file;
        bool        m_got_address;
        bool        m_got_length;
        bool        m_got_offset;
        bool        m_got_space;
        uint64_t      m_address;
        size_t      m_length;
        size_t      m_offset;
        string      m_file;
        string      m_space;
    };

    //! Handle an [extendregs] section.
    class ExtendRegsSection : public Section
    {
    public:
        ExtendRegsSection(Parsed& result) : m_result(result)
        {}

        void Define(const string& k, const string& v)
        {
            AddUniqueKey(m_result.extendRegDefs, DecodeUnsigned<uint32_t>(k), DecodeUnsigned<uint32_t>(v),k);
        }

        void End() {}

    private:
        Parsed&     m_result;
    };

    // Handle a [regs] section
    class SymbolicRegsSection : public Section
    {
    public:
        SymbolicRegsSection(Parsed& result) : m_result(result)
        {}

        void Define(const string& k, const string& v)
        {
            const string value = Trim(v, "\"'"); // strip quotes
            AddUniqueKey(m_result.regDefs, k, value,k);
        }

        void End() {}

    private:
        Parsed&     m_result;
    };

    // Handle a [device] section
    class DeviceSection : public Section
    {
    public:
        DeviceSection(Parsed& result) : m_result(result), gotName(false),  gotClass(false), gotType(false)
        {}

        void Define(const string& k, const string& v)
        {
            if (k == DeviceNameKey)
            {
                PreventDupes(gotName, k, DeviceSectionName);
                m_result.deviceName = v;
            }
            else if(k == DeviceClassKey)
            {
                PreventDupes(gotClass, k, DeviceSectionName);
                m_result.deviceClass = v;
            }
            else if(k == DeviceTypeKey)
            {
                PreventDupes(gotType, k, DeviceSectionName);
                m_result.deviceTypeName = v;
            }
        }

        void End() {}

    private:
        Parsed&     m_result;
        bool        gotName;
        bool        gotClass;
        bool        gotType;
    };

    //! Instantiate the appropriate handler for the section name
    auto_ptr<Section> NewSection( const string& sectionName, Parsed& result)
    {
        LogInfoStr( "Start of " + sectionName + " section\n");

        if (sectionName == GlobalSectionName)
        {
            return auto_ptr<Section>(new GlobalSection(result));
        }
        if (sectionName.substr(0,DumpFileSectionLen) == DumpFileSectionPrefix)
        {
            return auto_ptr<Section>(new DumpSection(result));
        }
        else if (sectionName == ExtendedRegsSectionName)
        {
            return auto_ptr<Section>(new ExtendRegsSection(result));
        }      
        else if (sectionName == SymbolicRegsSectionName)
        {
            return auto_ptr<Section>(new SymbolicRegsSection(result));
        }
        else if (sectionName == DeviceSectionName)
        {
            return auto_ptr<Section>(new DeviceSection(result));
        }
        else
        {   
            LogInfoStr("Unknown section ignored: " + sectionName + "\n");
            return auto_ptr<Section>(new IgnoredSection);
        }
    }

    /***** Device List file parsing *********************/
    //! Handle a [device_list] section.
    class DeviceListSection : public Section
    {
    public:
        DeviceListSection(ParsedDevices& result) : m_result(result), nextId(1)
        {}

        void Define(const string& , const string& v)
        {
            // throw away supplied key - DTSL wants them monotonically increasing from 1
            std::ostringstream id;
            id << nextId++;
            m_result.deviceList[id.str()] = v;
        }

        void End() {}

    private:
        ParsedDevices&   m_result;
        uint32_t           nextId;
    };

    //! Instantiate the appropriate handler for the section name
    auto_ptr<Section> NewDeviceList(const string& sectionName, ParsedDevices& result)
    {
        LogInfoStr("Start of " + sectionName + " section\n");

        if (sectionName == DeviceListSectionName)
        {
            return auto_ptr<Section>(new DeviceListSection(result));
        }
        else
        {
            // ignore unexpected sections, there may be others like [trace]
            // which RDDI doesn't care about
            return auto_ptr<Section>(new NullSection);
        }
    }

    // Handle a [snapshot] section
    class SnapshotSection : public Section
    {
    public:
        SnapshotSection(SnapshotInfo& result) : m_result(result), m_gotDescription(false), m_gotVersion(false)
        {}

        void Define(const string& k, const string& v)
        {
            if (k == VersionKey)
            {
                PreventDupes(m_gotVersion, k, SnapshotSectionName);
                m_result.version = v;
                // the only valid contents of this are 1.0, as this is the version that introduced the "snapshot" section
                if (v != "1.0" && v != "1")
                    throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE,  "Illegal snapshot file version: " + v);
            }
            else if (k == DescriptionKey)
            {
                PreventDupes(m_gotDescription, k, SnapshotSectionName);
                m_result.description = v;
            }
        }
        SnapshotInfo getSnapshotInfo() { return m_result; }
        void End() {}

    private:
        SnapshotInfo &m_result;
        bool        m_gotDescription;
        bool        m_gotVersion;

    };

    //! Instantiate the appropriate handler for the section name
    auto_ptr<Section> NewSnapshotInfo(const string& sectionName, ParsedDevices& result)
    {
        LogInfoStr((std::string)"Start of " + sectionName + (std::string)" section\n");

        if (sectionName == SnapshotSectionName)
        {
            return auto_ptr<Section>(new SnapshotSection(result.snapshotInfo));
        }
        else
        {
            // ignore unexpected sections, there may be others like [trace]
            // which RDDI doesn't care about
            return auto_ptr<Section>(new NullSection);
        }
    };

    class TraceSection : public Section
    {
    public:
        TraceSection(ParsedDevices& result) : m_result(result), gotName(false)
        {}

        void Define(const string& k, const string& v)
        {
            if (k == MetadataKey)
            {
                PreventDupes(gotName, k, TraceSectionName);
                m_result.traceMetaDataName = v;
            }
        }

        void End() {}

    private:
        ParsedDevices&     m_result;
        bool               gotName;
    };

    //! Instantiate the appropriate handler for the section name
    auto_ptr<Section> NewTraceMetaData(const string& sectionName, ParsedDevices& result)
    {
        LogInfoStr((std::string)"Start of " + sectionName + (std::string)" section\n");

        if (sectionName == TraceSectionName)
        {
            return auto_ptr<Section>(new TraceSection(result));
        }
        else
        {
            // ignore unexpected sections, there may be others like [trace]
            // which RDDI doesn't care about
            return auto_ptr<Section>(new NullSection);
        }
    };

    class TraceBufferListSection : public Section
    {
    public:
        TraceBufferListSection(ParsedTrace& result) : m_result(result), gotList(false)
        {}

        void Define(const string& k, const string& v)
        {
            if (k == BufferListKey)
            {
                PreventDupes(gotList, k, TraceBuffersSectionName);
                std::string nameList = v;
                std::string::size_type pos;
                while((pos = nameList.find_first_of(',')) != std::string::npos)
                {
                    m_result.buffer_section_names.push_back(nameList.substr(0,pos));
                    nameList=nameList.substr(pos+1,std::string::npos);
                }
                m_result.buffer_section_names.push_back(nameList);
            }
        }

        void End() {}

    private:
        ParsedTrace&     m_result;
        bool             gotList;
    };

    //! Instantiate the appropriate handler for the section name


    class TraceBufferSection : public Section
    {
    public:
        TraceBufferSection(ParsedTrace& result, const std::string &sectionName) : m_result(result), m_sectionName(sectionName),
            name(""), file(""), format(""), gotName(false), gotFile(false), gotFormat(false)
        {}

        void Define(const string& k, const string& v)
        {
            if (k == BufferNameKey)
            {
                PreventDupes(gotName, k, m_sectionName);
                name = v;
            }
            else if (k == BufferFileKey)
            {
                PreventDupes(gotFile, k, m_sectionName);
                file = v;
            }
            else if (k == BufferFormatKey)
            {
                PreventDupes(gotFormat, k, m_sectionName);
                format = v;
            }
        }

        void End() 
        {
            if (!gotName)
            {
                throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE,  "Trace Buffer section missing required buffer name");
            }
            if (!gotFile)
            {
                throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE, "Trace Buffer section is missing mandatory file definition");
            }
            
            struct TraceBufferInfo info = { name, file, format };
            m_result.trace_buffers.push_back(info);
        }


    private:
        ParsedTrace&     m_result;
        std::string m_sectionName;
        std::string name;
        bool gotName;
        std::string file;
        bool gotFile;
        std::string format;
        bool gotFormat;

    };

    class TraceSourceBuffersSection : public Section
    {
    public:
        TraceSourceBuffersSection(ParsedTrace& result) : m_result(result)
        {}

        void Define(const string& k, const string& v)
        {
            // k is the source name, v is the buffer name
            m_result.source_buffer_assoc[k] = v;
        }

        void End() {}

    private:
        ParsedTrace&     m_result;
    };

    class TraceCpuSourceSection : public Section
    {
    public:
        TraceCpuSourceSection(ParsedTrace& result) : m_result(result)
        {}

        void Define(const string& k, const string& v)
        {
            // k is the cpu name, v is the source name
            m_result.cpu_source_assoc[v] = k;
        }

        void End() {}

    private:
        ParsedTrace&     m_result;
    };

    auto_ptr<Section> NewTraceSection(const string& sectionName, ParsedTrace& result)
    {
        LogInfoStr((std::string)"Start of " + sectionName + (std::string)" section\n");

        if (sectionName == TraceBuffersSectionName)
        {
            return auto_ptr<Section>(new TraceBufferListSection(result));
        }
        else if(sectionName == SourceBuffersSectionName)
        {
            return auto_ptr<Section>(new TraceSourceBuffersSection(result));
        }
        else if(sectionName == CoreSourcesSectionName)
        {
            return auto_ptr<Section>(new  TraceCpuSourceSection(result));
        }
        else
        {
            // check the list of buffer sections
            std::vector<std::string>::iterator it = result.buffer_section_names.begin();
            bool matchedName = false;
            while(it != result.buffer_section_names.end())
            {
                if(sectionName == *it)
                {
                    return auto_ptr<Section>(new TraceBufferSection(result, sectionName));
                }
                it++;
            }
            // ignore unexpected sections,
            return auto_ptr<Section>(new IgnoredSection);
        }
    };


}

using namespace ParserPrivate;

Parser::Parsed Parser::ParseSingleDevice(istream& in)
{
    Parsed result;

    string line;
    auto_ptr<Section> section(new NullSection);

    while (getline(in, line))
    {
        CleanLine(line); // remove LF, comments
        string sectionName;

        if (IsSectionHeader(line, sectionName))
        {
            // Section ends with start of next section...
            section->End();
            section = NewSection(sectionName, result);
        }
        else if (!IsEmpty(line))
        {
            if (dynamic_cast<IgnoredSection *>(section.get()) == NULL)
            { // NOT an ignored section, so process it
                pair<string, string> kv(SplitKeyValue(line));
                section->Define(kv.first, kv.second);
            }
        }
    }
    // ... or end of file
    section->End();
    return result;
}

Parser::ParsedDevices Parser::ParseDeviceList(istream& in)
{
    ParsedDevices result;
    result.snapshotInfo.description = "";
    // call the original format 0.0, the device_list format 0.1 and the flexible format (including version) 1.0
    result.snapshotInfo.version = "0.1";
    string line;
    auto_ptr<Section> section(new NullSection);

    while (getline(in, line))
    {
        CleanLine(line); // remove LF, comments
        string sectionName;

        if (IsSectionHeader(line, sectionName))
        {
            // Section ends with start of next section...
            section->End();

            if (sectionName == SnapshotSectionName)
                section = NewSnapshotInfo(sectionName, result);
            else if(sectionName == TraceSectionName)
                section = NewTraceMetaData(sectionName, result);
            else // else rather than elseif for closer compatibility with old tests
                section = NewDeviceList(sectionName, result);
        }
        else if (!IsEmpty(line) &&
                    ( dynamic_cast<DeviceListSection *>(section.get()) != NULL ||
                      dynamic_cast<SnapshotSection *>(section.get()) != NULL ||
                      dynamic_cast<TraceSection *>(section.get()) != NULL
                    )
                )
        {
            pair<string, string> kv(SplitKeyValue(line));
            section->Define(kv.first, kv.second);
        }
    }
    // ... or end of file
    section->End();

    return result;
}


// parse the trace metadata ini file.
ParsedTrace Parser::ParseTraceMetaData(std::istream& in)
{
    ParsedTrace result;

    string line;
    auto_ptr<Section> section(new NullSection);

    while (getline(in, line))
    {
        CleanLine(line); // remove LF, comments
        string sectionName;

        if (IsSectionHeader(line, sectionName))
        {
            // Section ends with start of next section...
            section->End();
            section = NewTraceSection(sectionName, result);
        }
        else if (!IsEmpty(line))
        {
            if (dynamic_cast<IgnoredSection *>(section.get()) == NULL)
            { // NOT an ignored section, so process it
                pair<string, string> kv(SplitKeyValue(line));
                section->Define(kv.first, kv.second);
            }
        }
    }
    // ... or end of file
    section->End();
    return result;
}

    // build a source tree for a single buffer
bool Parser::ExtractSourceTree(const std::string &buffer_name, ParsedTrace &metadata, TraceBufferSourceTree &buffer_data)
{   
    bool bFoundbuffer = false;
    std::vector<TraceBufferInfo>::iterator it = metadata.trace_buffers.begin();

    while((it != metadata.trace_buffers.end()) && !bFoundbuffer)
    {
        if(it->bufferName == buffer_name)
        {
            bFoundbuffer = true;
            buffer_data.buffer_info = *it;
        }
        it++;
    }

    if(bFoundbuffer)
    {
        std::map<std::string, std::string>::iterator sbit = metadata.source_buffer_assoc.begin();
        while(sbit != metadata.source_buffer_assoc.end())
        {
            if(sbit->second == buffer_data.buffer_info.bufferName)
            {
                // found a source in this buffer...
                buffer_data.source_core_assoc[sbit->first] = metadata.cpu_source_assoc[sbit->first];
            }
            sbit++;
        }
    }
    return bFoundbuffer;
}

std::vector<std::string> Parser::GetBufferNameList(ParsedTrace &metadata)
{
    std::vector<std::string> nameList;
    std::vector<TraceBufferInfo>::iterator it = metadata.trace_buffers.begin();
    while(it != metadata.trace_buffers.end())
    {
        nameList.push_back(it->bufferName);
        it++;
    }
    return nameList;
}

void Parser::SetIErrorLogger(ITraceErrorLog *i_err_log)
{ 
    s_pErrorLogger = i_err_log; 
    if(s_pErrorLogger)
    {
        s_errlog_handle = s_pErrorLogger->RegisterErrorSource("snapshot_parser");
    }
}

ITraceErrorLog *Parser::GetIErrorLogger() 
{ 
    return s_pErrorLogger; 
}

void Parser::LogInfoStr(const std::string &logMsg)
{
    if(GetIErrorLogger() && s_verbose_logging)
        GetIErrorLogger()->LogMessage(s_errlog_handle,OCSD_ERR_SEV_INFO,logMsg);
}

void Parser::SetVerboseLogging(bool verbose) 
{ 
    s_verbose_logging = verbose; 
}
/* End of File snapshot_parser.cpp */
