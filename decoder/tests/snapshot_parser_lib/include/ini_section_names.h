/*
 * \file       ini_section_names.h
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

/* based on original RDDI source file 'ini_section_names.h"*/

#ifndef INC_INI_SECTION_NAMES_H
#define INC_INI_SECTION_NAMES_H

/* snapshot.ini keys */
const char* const SnapshotSectionName("snapshot");
const char* const VersionKey("version");
const char* const DescriptionKey("description");

const char* const DeviceListSectionName("device_list");

const char* const TraceSectionName("trace");
const char* const MetadataKey("metadata");

/* device .ini keys (device_N.ini or cpu_N.ini)*/
const char* const DeviceSectionName("device");
const char* const DeviceNameKey("name");
const char* const DeviceClassKey("class");
const char* const DeviceTypeKey("type");

const char* const SymbolicRegsSectionName("regs");

const char* const DumpFileSectionPrefix("dump");
const size_t      DumpFileSectionLen = 4;
const char* const DumpAddressKey("address");
const char* const DumpLengthKey("length");
const char* const DumpOffsetKey("offset");
const char* const DumpFileKey("file");
const char* const DumpSpaceKey("space");

/* trace.ini keys */
const char * const TraceBuffersSectionName("trace_buffers");
const char*  const BufferListKey("buffers");

const char * const BufferSectionPrefix("buffer");
const size_t       BufferSectionLen = 6;
const char*  const BufferNameKey("name");
const char*  const BufferFileKey("file");
const char*  const BufferFormatKey("format");

const char * const SourceBuffersSectionName("source_buffers");
const char * const CoreSourcesSectionName("core_trace_sources");

/* deprecated / unused in trace decode */ 
const char* const GlobalSectionName("global");
const char* const CoreKey("core");
const char* const ExtendedRegsSectionName("extendregs");
const char* const ClustersSectionName("clusters");

#endif // INC_INI_SECTION_NAMES_H

/* End of File ini_section_names.h */
