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

extern "C" {
#include <sys/time.h>
}

#include <cassert>
#include <cstdlib>
#include <map>
#include <sstream>
#include <utility>

#include "defs.hpp"
#include "parser.hpp"
#include "reader.hpp"
#include "text.hpp"

namespace impl = tools::atf_report;
#define IMPL_NAME "tools::atf_report"

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

template< typename Type >
Type
string_to_int(const std::string& str)
{
    std::istringstream ss(str);
    Type s;
    ss >> s;

    return s;
}

// ------------------------------------------------------------------------
// The "atf_tps" auxiliary parser.
// ------------------------------------------------------------------------

namespace atf_tps {

static const tools::parser::token_type eof_type = 0;
static const tools::parser::token_type nl_type = 1;
static const tools::parser::token_type text_type = 2;
static const tools::parser::token_type colon_type = 3;
static const tools::parser::token_type comma_type = 4;
static const tools::parser::token_type tps_count_type = 5;
static const tools::parser::token_type tp_start_type = 6;
static const tools::parser::token_type tp_end_type = 7;
static const tools::parser::token_type tc_start_type = 8;
static const tools::parser::token_type tc_so_type = 9;
static const tools::parser::token_type tc_se_type = 10;
static const tools::parser::token_type tc_end_type = 11;
static const tools::parser::token_type passed_type = 12;
static const tools::parser::token_type failed_type = 13;
static const tools::parser::token_type skipped_type = 14;
static const tools::parser::token_type info_type = 16;
static const tools::parser::token_type expected_death_type = 17;
static const tools::parser::token_type expected_exit_type = 18;
static const tools::parser::token_type expected_failure_type = 19;
static const tools::parser::token_type expected_signal_type = 20;
static const tools::parser::token_type expected_timeout_type = 21;

class tokenizer : public tools::parser::tokenizer< std::istream > {
public:
    tokenizer(std::istream& is, size_t curline) :
        tools::parser::tokenizer< std::istream >
            (is, true, eof_type, nl_type, text_type, curline)
    {
        add_delim(':', colon_type);
        add_delim(',', comma_type);
        add_keyword("tps-count", tps_count_type);
        add_keyword("tp-start", tp_start_type);
        add_keyword("tp-end", tp_end_type);
        add_keyword("tc-start", tc_start_type);
        add_keyword("tc-so", tc_so_type);
        add_keyword("tc-se", tc_se_type);
        add_keyword("tc-end", tc_end_type);
        add_keyword("passed", passed_type);
        add_keyword("failed", failed_type);
        add_keyword("skipped", skipped_type);
        add_keyword("info", info_type);
        add_keyword("expected_death", expected_death_type);
        add_keyword("expected_exit", expected_exit_type);
        add_keyword("expected_failure", expected_failure_type);
        add_keyword("expected_signal", expected_signal_type);
        add_keyword("expected_timeout", expected_timeout_type);
    }
};

} // namespace atf_tps

struct timeval
read_timeval(tools::parser::parser< atf_tps::tokenizer >& parser)
{
    using namespace atf_tps;

    tools::parser::token t = parser.expect(text_type, "timestamp");
    const std::string::size_type divider = t.text().find('.');
    if (divider == std::string::npos || divider == 0 ||
        divider == t.text().length() - 1)
        throw tools::parser::parse_error(t.lineno(),
                                       "Malformed timestamp value " + t.text());

    struct timeval tv;
    tv.tv_sec = string_to_int< long >(t.text().substr(0, divider));
    tv.tv_usec = string_to_int< long >(t.text().substr(divider + 1));
    return tv;
}

// ------------------------------------------------------------------------
// The "atf_tps_reader" class.
// ------------------------------------------------------------------------

impl::atf_tps_reader::atf_tps_reader(std::istream& is) :
    m_is(is)
{
}

impl::atf_tps_reader::~atf_tps_reader(void)
{
}

void
impl::atf_tps_reader::got_info(
    const std::string& what ATF_DEFS_ATTRIBUTE_UNUSED,
    const std::string& val ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_ntps(size_t ntps ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_tp_start(
    const std::string& tp ATF_DEFS_ATTRIBUTE_UNUSED,
    size_t ntcs ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_tp_end(
    struct timeval* tv ATF_DEFS_ATTRIBUTE_UNUSED,
    const std::string& reason ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_tc_start(
    const std::string& tcname ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_tc_stdout_line(
    const std::string& line ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_tc_stderr_line(
    const std::string& line ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_tc_end(
    const std::string& state ATF_DEFS_ATTRIBUTE_UNUSED,
    struct timeval* tv ATF_DEFS_ATTRIBUTE_UNUSED,
    const std::string& reason ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
impl::atf_tps_reader::got_eof(void)
{
}

void
impl::atf_tps_reader::read_info(void* pptr)
{
    using tools::parser::parse_error;
    using namespace atf_tps;

    tools::parser::parser< tokenizer >& p =
        *reinterpret_cast< tools::parser::parser< tokenizer >* >
        (pptr);

    (void)p.expect(colon_type, "`:'");

    tools::parser::token t = p.expect(text_type, "info property name");
    (void)p.expect(comma_type, "`,'");
    got_info(t.text(), tools::text::trim(p.rest_of_line()));

    (void)p.expect(nl_type, "new line");
}

void
impl::atf_tps_reader::read_tp(void* pptr)
{
    using tools::parser::parse_error;
    using namespace atf_tps;

    tools::parser::parser< tokenizer >& p =
        *reinterpret_cast< tools::parser::parser< tokenizer >* >
        (pptr);

    tools::parser::token t = p.expect(tp_start_type,
                                    "start of test program");

    t = p.expect(colon_type, "`:'");

    struct timeval s1 = read_timeval(p);

    t = p.expect(comma_type, "`,'");

    t = p.expect(text_type, "test program name");
    std::string tpname = t.text();

    t = p.expect(comma_type, "`,'");

    t = p.expect(text_type, "number of test programs");
    size_t ntcs = string_to_int< std::size_t >(t.text());

    t = p.expect(nl_type, "new line");

    ATF_PARSER_CALLBACK(p, got_tp_start(tpname, ntcs));

    size_t i = 0;
    while (p.good() && i < ntcs) {
        try {
            read_tc(&p);
            i++;
        } catch (const parse_error& pe) {
            p.add_error(pe);
            p.reset(nl_type);
        }
    }
    t = p.expect(tp_end_type, "end of test program");

    t = p.expect(colon_type, "`:'");

    struct timeval s2 = read_timeval(p);

    struct timeval s3;
    timersub(&s2, &s1, &s3);

    t = p.expect(comma_type, "`,'");

    t = p.expect(text_type, "test program name");
    if (t.text() != tpname)
        throw parse_error(t.lineno(), "Test program name used in "
                                      "terminator does not match "
                                      "opening");

    t = p.expect(nl_type, comma_type,
                 "new line or comma_type");
    std::string reason;
    if (t.type() == comma_type) {
        reason = tools::text::trim(p.rest_of_line());
        if (reason.empty())
            throw parse_error(t.lineno(),
                              "Empty reason for failed test program");
        t = p.next();
    }

    ATF_PARSER_CALLBACK(p, got_tp_end(&s3, reason));
}

void
impl::atf_tps_reader::read_tc(void* pptr)
{
    using tools::parser::parse_error;
    using namespace atf_tps;

    tools::parser::parser< tokenizer >& p =
        *reinterpret_cast< tools::parser::parser< tokenizer >* >
        (pptr);

    tools::parser::token t = p.expect(tc_start_type, "start of test case");

    t = p.expect(colon_type, "`:'");

    struct timeval s1 = read_timeval(p);

    t = p.expect(comma_type, "`,'");

    t = p.expect(text_type, "test case name");
    std::string tcname = t.text();

    ATF_PARSER_CALLBACK(p, got_tc_start(tcname));

    t = p.expect(nl_type, "new line");

    t = p.expect(tc_end_type, tc_so_type, tc_se_type,
                 "end of test case or test case's stdout/stderr line");
    while (t.type() != tc_end_type &&
           (t.type() == tc_so_type || t.type() == tc_se_type)) {
        tools::parser::token t2 = t;

        t = p.expect(colon_type, "`:'");

        std::string line = p.rest_of_line();

        if (t2.type() == tc_so_type) {
            ATF_PARSER_CALLBACK(p, got_tc_stdout_line(line));
        } else {
            assert(t2.type() == tc_se_type);
            ATF_PARSER_CALLBACK(p, got_tc_stderr_line(line));
        }

        t = p.expect(nl_type, "new line");

        t = p.expect(tc_end_type, tc_so_type, tc_se_type,
                     "end of test case or test case's stdout/stderr line");
    }

    t = p.expect(colon_type, "`:'");

    struct timeval s2 = read_timeval(p);

    struct timeval s3;
    timersub(&s2, &s1, &s3);

    t = p.expect(comma_type, "`,'");

    t = p.expect(text_type, "test case name");
    if (t.text() != tcname)
        throw parse_error(t.lineno(),
                          "Test case name used in terminator does not "
                          "match opening");

    t = p.expect(comma_type, "`,'");

    t = p.expect(expected_death_type, expected_exit_type, expected_failure_type,
        expected_signal_type, expected_timeout_type, passed_type, failed_type,
        skipped_type, "expected_{death,exit,failure,signal,timeout}, failed, "
        "passed or skipped");
    if (t.type() == passed_type) {
        ATF_PARSER_CALLBACK(p, got_tc_end("passed", &s3, ""));
    } else {
        std::string state;
        if (t.type() == expected_death_type) state = "expected_death";
        else if (t.type() == expected_exit_type) state = "expected_exit";
        else if (t.type() == expected_failure_type) state = "expected_failure";
        else if (t.type() == expected_signal_type) state = "expected_signal";
        else if (t.type() == expected_timeout_type) state = "expected_timeout";
        else if (t.type() == failed_type) state = "failed";
        else if (t.type() == skipped_type) state = "skipped";
        else std::abort();

        t = p.expect(comma_type, "`,'");
        std::string reason = tools::text::trim(p.rest_of_line());
        if (reason.empty())
            throw parse_error(t.lineno(), "Empty reason for " + state +
                " test case result");
        ATF_PARSER_CALLBACK(p, got_tc_end(state, &s3, reason));
    }

    t = p.expect(nl_type, "new line");
}

void
impl::atf_tps_reader::read(void)
{
    using tools::parser::parse_error;
    using namespace atf_tps;

    std::pair< size_t, tools::parser::headers_map > hml =
        tools::parser::read_headers(m_is, 1);
    tools::parser::validate_content_type(hml.second,
                                         "application/X-atf-tps", 3);

    tokenizer tkz(m_is, hml.first);
    tools::parser::parser< tokenizer > p(tkz);

    try {
        tools::parser::token t;

        while ((t = p.expect(tps_count_type, info_type, "tps-count or info "
                             "field")).type() == info_type)
            read_info(&p);

        t = p.expect(colon_type, "`:'");

        t = p.expect(text_type, "number of test programs");
        size_t ntps = string_to_int< std::size_t >(t.text());
        ATF_PARSER_CALLBACK(p, got_ntps(ntps));

        t = p.expect(nl_type, "new line");

        size_t i = 0;
        while (p.good() && i < ntps) {
            try {
                read_tp(&p);
                i++;
            } catch (const parse_error& pe) {
                p.add_error(pe);
                p.reset(nl_type);
            }
        }

        while ((t = p.expect(eof_type, info_type, "end of stream or info "
                             "field")).type() == info_type)
            read_info(&p);
        ATF_PARSER_CALLBACK(p, got_eof());
    } catch (const parse_error& pe) {
        p.add_error(pe);
        p.reset(nl_type);
    }
}
