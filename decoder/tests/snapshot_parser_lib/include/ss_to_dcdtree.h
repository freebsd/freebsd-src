/*
 * \file       ss_to_dcdtree.h
 * \brief      OpenCSD : Create a decode tree given a snapshot database.
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

#ifndef ARM_SS_TO_DCDTREE_H_INCLUDED
#define ARM_SS_TO_DCDTREE_H_INCLUDED

#include <string>

#include "opencsd.h"
#include "snapshot_parser.h"
#include "snapshot_reader.h"

class DecodeTree;
class ITraceErrorLog;

class CreateDcdTreeFromSnapShot
{
public:
    CreateDcdTreeFromSnapShot();
    ~CreateDcdTreeFromSnapShot();
    
    void initialise(SnapShotReader *m_pReader, ITraceErrorLog *m_pErrLogInterface);

    bool createDecodeTree(const std::string &SourceBufferName, bool bPacketProcOnly);
    void destroyDecodeTree();
    DecodeTree *getDecodeTree() const { return m_pDecodeTree; };
    const char *getBufferFileName() const { return m_BufferFileName.c_str(); };

    // TBD: add in filters for ID list, first ID found.

private:
    // create a decoder related to a core source (ETM, PTM)
    bool createPEDecoder(const std::string &coreName, Parser::Parsed *devSrc);
    // protocol specific core source decoders
    bool createETMv4Decoder(const std::string &coreName, Parser::Parsed *devSrc, const bool bDataChannel = false);
    bool createETMv3Decoder(const std::string &coreName, Parser::Parsed *devSrc);
    bool createPTMDecoder(const std::string &coreName, Parser::Parsed *devSrc);
    // TBD add etmv4d

    // create a decoder related to a software trace source (ITM, STM)
    bool createSTDecoder(Parser::Parsed *devSrc);
    // protocol specific decoders
    bool createSTMDecoder(Parser::Parsed *devSrc);

    typedef struct _regs_to_access {
        const char *pszName;
        bool failIfMissing;
        uint32_t *value;
        uint32_t val_default;
    } regs_to_access_t;

    bool getRegisters(std::map<std::string, std::string, Util::CaseInsensitiveLess> &regDefs, int numRegs, regs_to_access_t *reg_access_array);
    bool getRegByPrefix(std::map<std::string, std::string, Util::CaseInsensitiveLess> &regDefs, 
                        regs_to_access_t &reg_accessor);
    bool getCoreProfile(const std::string &coreName, ocsd_arch_version_t &arch_ver, ocsd_core_profile_t &core_prof);

    void LogError(const std::string &msg);
    void LogError(const ocsdError &err);

    void processDumpfiles(std::vector<Parser::DumpDef> &dumps);




    bool m_bInit;
    DecodeTree *m_pDecodeTree;
    SnapShotReader *m_pReader;
    ITraceErrorLog *m_pErrLogInterface;
    ocsd_hndl_err_log_t m_errlog_handle;

    bool m_bPacketProcOnly;
    std::string m_BufferFileName;

    CoreArchProfileMap m_arch_profiles;
};


#endif // ARM_SS_TO_DCDTREE_H_INCLUDED

/* End of File ss_to_dcdtree.h */
