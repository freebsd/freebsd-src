/*
 * \file       snapshot_parser_util.h
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

#ifndef ARM_SNAPSHOT_PARSER_UTIL_H_INCLUDED
#define ARM_SNAPSHOT_PARSER_UTIL_H_INCLUDED

#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <cstdint>

#include "common/ocsd_error.h"

namespace Util
{
    //! format an address as '0xNNNNNNNN'
    std::string MakeAddressString(uint32_t address);
    //! format a an address as '0xNNNNNNNNNNNNNNNN'
    std::string MakeAddressString(uint64_t address);

    //! remove leading garbage from a string
    std::string TrimLeft(const std::string& s, const std::string& ws = " \t");

    //! remove trailing garbage from a string
    std::string TrimRight(const std::string& s, const std::string& ws = " \t");

    //! remove leading and trailing garbage from a string
    std::string Trim(const std::string& s, const std::string& ws = " \t");

    //! Functions to decode an integer
    //
    inline void ThrowUnsignedConversionError(const std::string& s)
    {
        throw ocsdError(OCSD_ERR_SEV_ERROR, OCSD_ERR_TEST_SNAPSHOT_PARSE, "Could not parse '" + s + "' as unsigned integer");
    }

    template <class T>
    T DecodeUnsigned(const std::string& s)
    {
        char *endptr(0);
        // Address can be up to 64 bits, ensure there is enough storage
#ifdef WIN32
        uint64_t result(_strtoui64(s.c_str(), &endptr, 0));
#else
        uint64_t result(std::strtoull(s.c_str(), &endptr, 0));
#endif
        if (*endptr != '\0')
        {
            ThrowUnsignedConversionError(s);
            return T(); // keep compiler happy
        }
        return static_cast<T>(result);
    }

    class CaseInsensitiveLess
    {
    public:
        bool operator() (const std::string& s1, const std::string& s2) const
        {
            return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), cmp);
        }

    private:
        static bool cmp(unsigned char c1, unsigned char c2)
        {
            return std::tolower(c1) < std::tolower(c2); 
        }
    };
    
    inline bool CaseInsensitiveEquals(const std::string& s1, const std::string& s2)
    {
        return !CaseInsensitiveLess()(s1, s2) && !CaseInsensitiveLess()(s2, s1); 
    }
}

#endif // ARM_SNAPSHOT_PARSER_UTIL_H_INCLUDED

/* End of File snapshot_parser_util.h */
