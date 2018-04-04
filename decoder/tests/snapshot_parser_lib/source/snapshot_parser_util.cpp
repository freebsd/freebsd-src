/*
 * \file       snapshot_parser_util.cpp
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

#include "snapshot_parser_util.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <iterator>
using namespace std;

//#include <boost/regex.hpp>

// Make "0xNNNNNNNN" for use in user messages
string Util::MakeAddressString(uint32_t address)
{
    ostringstream oss;
    oss << "0x" << hex << setw(8) << setfill('0') << address;
    return oss.str();
}

// Make "0xNNNNNNNNNNNNNNNN" for use in user messages
string Util::MakeAddressString(uint64_t address)
{
    ostringstream oss;
    oss << "0x" << hex << setw(16) << setfill('0') << address;
    return oss.str();
}

string Util::TrimLeft(const std::string& s, const std::string& ws)
{
    string out(s);
    return out.erase(0, s.find_first_not_of(ws));
}

string Util::TrimRight(const std::string& s, const std::string& ws)
{
    string out(s);
    return out.erase(s.find_last_not_of(ws) + 1);
}

string Util::Trim(const std::string& s, const std::string& ws)
{
    return TrimRight(TrimLeft(s, ws), ws);
}

/*bool Util::DoRegexReplace(string& original, const string& re, 
                          const string& replacement)
{
    const string before(original);
    const boost::regex ex(re);
    string after;
    boost::regex_replace(back_inserter(after), before.begin(), before.end(), 
        ex, replacement);
    original.assign(after);
    return before != after;
}*/

/* End of File snapshot_parser_util.cpp */
