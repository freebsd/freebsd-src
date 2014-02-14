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

#if !defined(TOOLS_FORMATS_HPP)
#define TOOLS_FORMATS_HPP

extern "C" {
#include <sys/time.h>
}

#include <istream>
#include <string>

namespace tools {
namespace atf_report {

struct test_case_result {
    enum state_enum {
        PASSED,
        FAILED,
        SKIPPED,
    };
    const state_enum state;
    const std::string& reason;

    test_case_result(const state_enum p_state, const std::string& p_reason) :
        state(p_state),
        reason(p_reason)
    {
    }
};

class atf_tps_reader {
    std::istream& m_is;

    void read_info(void*);
    void read_tp(void*);
    void read_tc(void*);

protected:
    virtual void got_info(const std::string&, const std::string&);
    virtual void got_ntps(size_t);
    virtual void got_tp_start(const std::string&, size_t);
    virtual void got_tp_end(struct timeval*, const std::string&);

    virtual void got_tc_start(const std::string&);
    virtual void got_tc_stdout_line(const std::string&);
    virtual void got_tc_stderr_line(const std::string&);
    virtual void got_tc_end(const std::string&, struct timeval*,
                            const std::string&);
    virtual void got_eof(void);

public:
    atf_tps_reader(std::istream&);
    virtual ~atf_tps_reader(void);

    void read(void);
};

} // namespace atf_report
} // namespace tools

#endif // !defined(TOOLS_FORMATS_HPP)
