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

#include <cassert>
#include <cstdlib>
#include <fstream>

#include "atffile.hpp"
#include "defs.hpp"
#include "exceptions.hpp"
#include "expand.hpp"
#include "parser.hpp"

namespace impl = tools;
namespace detail = tools::detail;

namespace {

typedef std::map< std::string, std::string > vars_map;

} // anonymous namespace

// ------------------------------------------------------------------------
// The "atf_atffile" auxiliary parser.
// ------------------------------------------------------------------------

namespace atf_atffile {

static const tools::parser::token_type eof_type = 0;
static const tools::parser::token_type nl_type = 1;
static const tools::parser::token_type text_type = 2;
static const tools::parser::token_type colon_type = 3;
static const tools::parser::token_type conf_type = 4;
static const tools::parser::token_type dblquote_type = 5;
static const tools::parser::token_type equal_type = 6;
static const tools::parser::token_type hash_type = 7;
static const tools::parser::token_type prop_type = 8;
static const tools::parser::token_type tp_type = 9;
static const tools::parser::token_type tp_glob_type = 10;

class tokenizer : public tools::parser::tokenizer< std::istream > {
public:
    tokenizer(std::istream& is, size_t curline) :
        tools::parser::tokenizer< std::istream >
            (is, true, eof_type, nl_type, text_type, curline)
    {
        add_delim(':', colon_type);
        add_delim('=', equal_type);
        add_delim('#', hash_type);
        add_quote('"', dblquote_type);
        add_keyword("conf", conf_type);
        add_keyword("prop", prop_type);
        add_keyword("tp", tp_type);
        add_keyword("tp-glob", tp_glob_type);
    }
};

} // namespace atf_atffile

// ------------------------------------------------------------------------
// The "atf_atffile_reader" class.
// ------------------------------------------------------------------------

detail::atf_atffile_reader::atf_atffile_reader(std::istream& is) :
    m_is(is)
{
}

detail::atf_atffile_reader::~atf_atffile_reader(void)
{
}

void
detail::atf_atffile_reader::got_conf(
    const std::string& name ATF_DEFS_ATTRIBUTE_UNUSED,
    const std::string& val ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
detail::atf_atffile_reader::got_prop(
    const std::string& name ATF_DEFS_ATTRIBUTE_UNUSED,
    const std::string& val ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
detail::atf_atffile_reader::got_tp(
    const std::string& name ATF_DEFS_ATTRIBUTE_UNUSED,
    bool isglob ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
detail::atf_atffile_reader::got_eof(void)
{
}

void
detail::atf_atffile_reader::read(void)
{
    using tools::parser::parse_error;
    using namespace atf_atffile;

    std::pair< size_t, tools::parser::headers_map > hml =
        tools::parser::read_headers(m_is, 1);
    tools::parser::validate_content_type(hml.second,
        "application/X-atf-atffile", 1);

    tokenizer tkz(m_is, hml.first);
    tools::parser::parser< tokenizer > p(tkz);

    for (;;) {
        try {
            tools::parser::token t =
                p.expect(conf_type, hash_type, prop_type, tp_type,
                         tp_glob_type, nl_type, eof_type,
                         "conf, #, prop, tp, tp-glob, a new line or eof");
            if (t.type() == eof_type)
                break;

            if (t.type() == conf_type) {
                t = p.expect(colon_type, "`:'");

                t = p.expect(text_type, "variable name");
                std::string var = t.text();

                t = p.expect(equal_type, "equal sign");

                t = p.expect(text_type, "word or quoted string");
                ATF_PARSER_CALLBACK(p, got_conf(var, t.text()));
            } else if (t.type() == hash_type) {
                (void)p.rest_of_line();
            } else if (t.type() == prop_type) {
                t = p.expect(colon_type, "`:'");

                t = p.expect(text_type, "property name");
                std::string name = t.text();

                t = p.expect(equal_type, "equale sign");

                t = p.expect(text_type, "word or quoted string");
                ATF_PARSER_CALLBACK(p, got_prop(name, t.text()));
            } else if (t.type() == tp_type) {
                t = p.expect(colon_type, "`:'");

                t = p.expect(text_type, "word or quoted string");
                ATF_PARSER_CALLBACK(p, got_tp(t.text(), false));
            } else if (t.type() == tp_glob_type) {
                t = p.expect(colon_type, "`:'");

                t = p.expect(text_type, "word or quoted string");
                ATF_PARSER_CALLBACK(p, got_tp(t.text(), true));
            } else if (t.type() == nl_type) {
                continue;
            } else
                std::abort();

            t = p.expect(nl_type, hash_type, eof_type,
                         "new line or comment");
            if (t.type() == hash_type) {
                (void)p.rest_of_line();
                t = p.next();
            } else if (t.type() == eof_type)
                break;
        } catch (const parse_error& pe) {
            p.add_error(pe);
            p.reset(nl_type);
        }
    }

    ATF_PARSER_CALLBACK(p, got_eof());
}

// ------------------------------------------------------------------------
// The "reader" helper class.
// ------------------------------------------------------------------------

class reader : public detail::atf_atffile_reader {
    const tools::fs::directory& m_dir;
    vars_map m_conf, m_props;
    std::vector< std::string > m_tps;

    void
    got_tp(const std::string& name, bool isglob)
    {
        if (isglob) {
            std::vector< std::string > ms =
                tools::expand::expand_glob(name, m_dir.names());
            // Cannot use m_tps.insert(iterator, begin, end) here because it
            // does not work under Solaris.
            for (std::vector< std::string >::const_iterator iter = ms.begin();
                 iter != ms.end(); iter++)
                m_tps.push_back(*iter);
        } else {
            if (m_dir.find(name) == m_dir.end())
                throw tools::not_found_error< tools::fs::path >
                    ("Cannot locate the " + name + " file",
                     tools::fs::path(name));
            m_tps.push_back(name);
        }
    }

    void
    got_prop(const std::string& name, const std::string& val)
    {
        m_props[name] = val;
    }

    void
    got_conf(const std::string& var, const std::string& val)
    {
        m_conf[var] = val;
    }

public:
    reader(std::istream& is, const tools::fs::directory& dir) :
        detail::atf_atffile_reader(is),
        m_dir(dir)
    {
    }

    const vars_map&
    conf(void)
        const
    {
        return m_conf;
    }

    const vars_map&
    props(void)
        const
    {
        return m_props;
    }

    const std::vector< std::string >&
    tps(void)
        const
    {
        return m_tps;
    }
};

// ------------------------------------------------------------------------
// The "atffile" class.
// ------------------------------------------------------------------------

impl::atffile::atffile(const vars_map& config_vars,
                       const std::vector< std::string >& test_program_names,
                       const vars_map& properties) :
    m_conf(config_vars),
    m_tps(test_program_names),
    m_props(properties)
{
    assert(properties.find("test-suite") != properties.end());
}

const std::vector< std::string >&
impl::atffile::tps(void)
    const
{
    return m_tps;
}

const vars_map&
impl::atffile::conf(void)
    const
{
    return m_conf;
}

const vars_map&
impl::atffile::props(void)
    const
{
    return m_props;
}

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

// XXX Glob expansion and file existance checks certainly do not belong in
// a *parser*.  This needs to be taken out...
impl::atffile
impl::read_atffile(const tools::fs::path& filename)
{
    // Scan the directory where the atffile lives in to gather a list of
    // all possible test programs in it.
    tools::fs::directory dir(filename.branch_path());
    dir.erase(filename.leaf_name());
    tools::fs::directory::iterator iter = dir.begin();
    while (iter != dir.end()) {
        const std::string& name = (*iter).first;
        const tools::fs::file_info& fi = (*iter).second;

        // Discard hidden files and non-executable ones so that they are
        // not candidates for glob matching.
        if (name[0] == '.' || (!fi.is_owner_executable() &&
                               !fi.is_group_executable()))
            dir.erase(iter++);
        else
            iter++;
    }

    // Parse the atffile.
    std::ifstream is(filename.c_str());
    if (!is)
        throw tools::not_found_error< tools::fs::path >
            ("Cannot open Atffile", filename);
    reader r(is, dir);
    r.read();
    is.close();

    // Sanity checks.
    if (r.props().find("test-suite") == r.props().end())
        throw tools::not_found_error< std::string >
            ("Undefined property `test-suite'", "test-suite");

    return atffile(r.conf(), r.tps(), r.props());
}
