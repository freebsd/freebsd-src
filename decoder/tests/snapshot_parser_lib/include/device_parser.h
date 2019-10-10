/*
 * \file       device_parser.h
 * \brief      OpenCSD : Snapshot parser library
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

#ifndef DEVICE_PARSER_H
#define DEVICE_PARSER_H

#include "device_info.h"
#include <memory>

#include <map>
#include <istream>
#include <vector>
#include <cstdint>

#include "snapshot_info.h"

class DeviceParser
{
public:
    virtual ~DeviceParser() {};
    typedef std::shared_ptr<DeviceInfo> DevPtr;
    virtual DevPtr getDevice(int devNo) =0;
    virtual void getDeviceList(std::vector<int> &devices)  const = 0;
    virtual size_t getDeviceCount() const = 0;

};

class SnapshotParser
{
public:
    virtual ~SnapshotParser() {};
    virtual std::string getDescription() const = 0;
    virtual std::string getVersion() const = 0;
};

class ModernSnapshotParser : public DeviceParser, SnapshotParser
{
public:
    ModernSnapshotParser( std::istream &iss);
    ModernSnapshotParser();
    virtual ~ModernSnapshotParser();

    typedef std::shared_ptr<DeviceInfo> DevPtr;
    DevPtr getDevice(int devNo);
    void getDeviceList(std::vector<int> &devices) const;
    
    bool isInitialised() const;
    size_t getDeviceCount() const;

    std::string getTraceMetadataFile() const;

    // Snapshot Info
    std::string getDescription() const;
    std::string getVersion() const;


private:
    void addDevice(std::string rawName, std::string path);
    uint32_t getUniqueDeviceID(std::string &rawName);
    
    void addSnapshotInfo(SnapshotInfo snap);

    SnapshotInfo snaphotInfo;

    // map stores deviceId and associated data
    std::map<int, DevPtr> deviceMap;
    bool initialised;

    std::string traceMetaDataIni;
};

#endif

/* End of File device_parser.h */
