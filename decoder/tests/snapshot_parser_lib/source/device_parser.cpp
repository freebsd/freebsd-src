/*
 * \file       device_parser.cpp
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
#include "device_parser.h"

#include "snapshot_parser.h"

#include <sstream>
#include <iostream>
#include <string>
#include <memory>
#include <cstdio>

#include "common/ocsd_error.h"

using namespace std;

ModernSnapshotParser::ModernSnapshotParser() : initialised(false), traceMetaDataIni("")
{
}

ModernSnapshotParser::ModernSnapshotParser( std::istream &iss) : initialised(false), traceMetaDataIni("")
{
    Parser::ParsedDevices pd = Parser::ParseDeviceList(iss);
    std::map<std::string, std::string> rawList = pd.deviceList;
    std::map<std::string, std::string>::iterator it;
    for (it = rawList.begin(); it != rawList.end(); ++it )
    {
        addDevice(it->first, it->second);
    }
    snaphotInfo = pd.snapshotInfo;
    this->traceMetaDataIni = pd.traceMetaDataName;
    initialised = true;
}

ModernSnapshotParser::~ModernSnapshotParser()
{
}

void ModernSnapshotParser::addDevice(std::string rawName, std::string path)
{
    uint32_t deviceID = getUniqueDeviceID(rawName);
    deviceMap[deviceID] = DevPtr(new DeviceInfo(deviceID, path));
}

uint32_t ModernSnapshotParser::getUniqueDeviceID(std::string &rawName)
{
    size_t pos = rawName.find_first_of("0123456789");
    uint32_t candidate = 1;
    if (pos != std::string::npos)
    {
        std::string numbers = rawName.substr(pos);
        istringstream ist(numbers);
        ist >> candidate;
    }
    while (true)
    {
        try
        {
            getDevice(candidate);
            // didn't throw means already in list, so bad candidate
            candidate++;
        }
        catch (ocsdError & /*e*/)
        {
            // threw, so unique ID, so this is good, leave loop
            break;
        }
    }
    return candidate;
}

bool ModernSnapshotParser::isInitialised() const
{
    return initialised;
}

size_t ModernSnapshotParser::getDeviceCount() const
{
    unsigned int deviceCount = 0;
    if (isInitialised())
        deviceCount = deviceMap.size();
    return deviceCount;
}

ModernSnapshotParser::DevPtr ModernSnapshotParser::getDevice(int id)
{
    map<int, DevPtr>::const_iterator it(deviceMap.find(id));
    if (it == deviceMap.end())
    {
        std::ostringstream ost;
        ost << "Unknown device:" << id;
        throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE, ost.str());
    }
    return it->second;
}

void ModernSnapshotParser::addSnapshotInfo(SnapshotInfo snap)
{
    snaphotInfo = snap;
}

void ModernSnapshotParser::getDeviceList(std::vector<int> &devices) const
{
    map<int, DevPtr>::const_iterator it;
    //devices.clear();
    for (it = deviceMap.begin(); it != deviceMap.end(); ++it)
        devices.push_back(it->first);
}

std::string ModernSnapshotParser::getDescription() const
{
    return snaphotInfo.description;
}

std::string ModernSnapshotParser::getVersion() const
{
    return snaphotInfo.version;
}


std::string  ModernSnapshotParser::getTraceMetadataFile() const
{
    return traceMetaDataIni;
}

/* End of File device_parser.cpp */
