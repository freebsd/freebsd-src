//
// Automated Testing Framework (atf)
//
// Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#if !defined(TOOLS_TEST_PROGRAM_HPP)
#define TOOLS_TEST_PROGRAM_HPP

#include <map>
#include <string>

#include "fs.hpp"
#include "process.hpp"

namespace tools {
namespace test_program {

struct test_case_result {
    std::string m_state;
    int m_value;
    std::string m_reason;

public:
    test_case_result(void) :
        m_state("UNINITIALIZED"),
        m_value(-1),
        m_reason("")
    {
    }

    test_case_result(const std::string& p_state, const int p_value,
                     const std::string& p_reason) :
        m_state(p_state),
        m_value(p_value),
        m_reason(p_reason)
    {
    }

    const std::string&
    state(void) const
    {
        return m_state;
    }

    int
    value(void) const
    {
        return m_value;
    }

    const std::string&
    reason(void) const
    {
        return m_reason;
    }
};

namespace detail {

class atf_tp_reader {
    std::istream& m_is;

    void validate_and_insert(const std::string&, const std::string&,
                             const size_t,
                             std::map< std::string, std::string >&);

protected:
    virtual void got_tc(const std::string&,
                        const std::map< std::string, std::string >&);
    virtual void got_eof(void);

public:
    atf_tp_reader(std::istream&);
    virtual ~atf_tp_reader(void);

    void read(void);
};

test_case_result parse_test_case_result(const std::string&);

} // namespace detail

class atf_tps_writer {
    std::ostream& m_os;

    std::string m_tpname, m_tcname;

public:
    atf_tps_writer(std::ostream&);

    void info(const std::string&, const std::string&);
    void ntps(size_t);

    void start_tp(const std::string&, size_t);
    void end_tp(const std::string&);

    void start_tc(const std::string&);
    void stdout_tc(const std::string&);
    void stderr_tc(const std::string&);
    void end_tc(const std::string&, const std::string&);
};

typedef std::map< std::string, std::map< std::string, std::string > >
    test_cases_map;

struct metadata {
    test_cases_map test_cases;

    metadata(void)
    {
    }

    metadata(const test_cases_map& p_test_cases) :
        test_cases(p_test_cases)
    {
    }
};

class atf_tps_writer;

metadata get_metadata(const tools::fs::path&,
                      const std::map< std::string, std::string >&);
test_case_result read_test_case_result(const tools::fs::path&);
std::pair< std::string, tools::process::status > run_test_case(
    const tools::fs::path&, const std::string&, const std::string&,
    const std::map< std::string, std::string >&,
    const std::map< std::string, std::string >&,
    const tools::fs::path&, const tools::fs::path&, atf_tps_writer&);

} // namespace test_program
} // namespace tools

#endif // !defined(TOOLS_TEST_PROGRAM_HPP)
