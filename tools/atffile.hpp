//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if !defined(TOOLS_ATFFILE_HPP)
#define TOOLS_ATFFILE_HPP

#include <map>
#include <string>
#include <vector>

#include "fs.hpp"

namespace tools {

// ------------------------------------------------------------------------
// The "atf_atffile_reader" class.
// ------------------------------------------------------------------------

namespace detail {

class atf_atffile_reader {
    std::istream& m_is;

protected:
    virtual void got_conf(const std::string&, const std::string &);
    virtual void got_prop(const std::string&, const std::string &);
    virtual void got_tp(const std::string&, bool);
    virtual void got_eof(void);

public:
    atf_atffile_reader(std::istream&);
    virtual ~atf_atffile_reader(void);

    void read(void);
};

} // namespace detail

// ------------------------------------------------------------------------
// The "atffile" class.
// ------------------------------------------------------------------------

class atffile {
    std::map< std::string, std::string > m_conf;
    std::vector< std::string > m_tps;
    std::map< std::string, std::string > m_props;

public:
    atffile(const std::map< std::string, std::string >&,
            const std::vector< std::string >&,
            const std::map< std::string, std::string >&);

    const std::map< std::string, std::string >& conf(void) const;
    const std::vector< std::string >& tps(void) const;
    const std::map< std::string, std::string >& props(void) const;
};

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

atffile read_atffile(const tools::fs::path&);

} // namespace tools

#endif // !defined(TOOLS_ATFFILE_HPP)
