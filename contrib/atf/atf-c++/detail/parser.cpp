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

#include <sstream>

#include "parser.hpp"
#include "sanity.hpp"
#include "text.hpp"

namespace impl = atf::parser;
#define IMPL_NAME "atf::parser"

// ------------------------------------------------------------------------
// The "parse_error" class.
// ------------------------------------------------------------------------

impl::parse_error::parse_error(size_t line, std::string msg) :
    std::runtime_error(msg),
    std::pair< size_t, std::string >(line, msg)
{
}

impl::parse_error::~parse_error(void)
    throw()
{
}

const char*
impl::parse_error::what(void)
    const throw()
{
    try {
        std::ostringstream oss;
        oss << "LONELY PARSE ERROR: " << first << ": " << second;
        m_msg = oss.str();
        return m_msg.c_str();
    } catch (...) {
        return "Could not format message for parsing error.";
    }
}

impl::parse_error::operator std::string(void)
    const
{
    return atf::text::to_string(first) + ": " + second;
}

// ------------------------------------------------------------------------
// The "parse_errors" class.
// ------------------------------------------------------------------------

impl::parse_errors::parse_errors(void) :
    std::runtime_error("No parsing errors yet")
{
    m_msg.clear();
}

impl::parse_errors::~parse_errors(void)
    throw()
{
}

const char*
impl::parse_errors::what(void)
    const throw()
{
    try {
        m_msg = atf::text::join(*this, "\n");
        return m_msg.c_str();
    } catch (...) {
        return "Could not format messages for parsing errors.";
    }
}

// ------------------------------------------------------------------------
// The "format_error" class.
// ------------------------------------------------------------------------

impl::format_error::format_error(const std::string& w) :
    std::runtime_error(w.c_str())
{
}

// ------------------------------------------------------------------------
// The "token" class.
// ------------------------------------------------------------------------

impl::token::token(void) :
    m_inited(false)
{
}

impl::token::token(size_t p_line,
                   const token_type& p_type,
                   const std::string& p_text) :
    m_inited(true),
    m_line(p_line),
    m_type(p_type),
    m_text(p_text)
{
}

size_t
impl::token::lineno(void)
    const
{
    return m_line;
}

const impl::token_type&
impl::token::type(void)
    const
{
    return m_type;
}

const std::string&
impl::token::text(void)
    const
{
    return m_text;
}

impl::token::operator bool(void)
    const
{
    return m_inited;
}

bool
impl::token::operator!(void)
    const
{
    return !m_inited;
}

// ------------------------------------------------------------------------
// The "header_entry" class.
// ------------------------------------------------------------------------

impl::header_entry::header_entry(void)
{
}

impl::header_entry::header_entry(const std::string& n, const std::string& v,
                                 attrs_map as) :
    m_name(n),
    m_value(v),
    m_attrs(as)
{
}

const std::string&
impl::header_entry::name(void) const
{
    return m_name;
}

const std::string&
impl::header_entry::value(void) const
{
    return m_value;
}

const impl::attrs_map&
impl::header_entry::attrs(void) const
{
    return m_attrs;
}

bool
impl::header_entry::has_attr(const std::string& n) const
{
    return m_attrs.find(n) != m_attrs.end();
}

const std::string&
impl::header_entry::get_attr(const std::string& n) const
{
    attrs_map::const_iterator iter = m_attrs.find(n);
    PRE(iter != m_attrs.end());
    return (*iter).second;
}

// ------------------------------------------------------------------------
// The header tokenizer.
// ------------------------------------------------------------------------

namespace header {

static const impl::token_type eof_type = 0;
static const impl::token_type nl_type = 1;
static const impl::token_type text_type = 2;
static const impl::token_type colon_type = 3;
static const impl::token_type semicolon_type = 4;
static const impl::token_type dblquote_type = 5;
static const impl::token_type equal_type = 6;

class tokenizer : public impl::tokenizer< std::istream > {
public:
    tokenizer(std::istream& is, size_t curline) :
        impl::tokenizer< std::istream >
            (is, true, eof_type, nl_type, text_type, curline)
    {
        add_delim(';', semicolon_type);
        add_delim(':', colon_type);
        add_delim('=', equal_type);
        add_quote('"', dblquote_type);
    }
};

static
impl::parser< header::tokenizer >&
read(impl::parser< header::tokenizer >& p, impl::header_entry& he)
{
    using namespace header;

    impl::token t = p.expect(text_type, nl_type, "a header name");
    if (t.type() == nl_type) {
        he = impl::header_entry();
        return p;
    }
    std::string hdr_name = t.text();

    t = p.expect(colon_type, "`:'");

    t = p.expect(text_type, "a textual value");
    std::string hdr_value = t.text();

    impl::attrs_map attrs;

    for (;;) {
        t = p.expect(eof_type, semicolon_type, nl_type,
                     "eof, `;' or new line");
        if (t.type() == eof_type || t.type() == nl_type)
            break;

        t = p.expect(text_type, "an attribute name");
        std::string attr_name = t.text();

        t = p.expect(equal_type, "`='");

        t = p.expect(text_type, "word or quoted string");
        std::string attr_value = t.text();
        attrs[attr_name] = attr_value;
    }

    he = impl::header_entry(hdr_name, hdr_value, attrs);

    return p;
}

static
std::ostream&
write(std::ostream& os, const impl::header_entry& he)
{
    std::string line = he.name() + ": " + he.value();
    impl::attrs_map as = he.attrs();
    for (impl::attrs_map::const_iterator iter = as.begin(); iter != as.end();
         iter++) {
        PRE((*iter).second.find('\"') == std::string::npos);
        line += "; " + (*iter).first + "=\"" + (*iter).second + "\"";
    }

    os << line << "\n";

    return os;
}

} // namespace header

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

std::pair< size_t, impl::headers_map >
impl::read_headers(std::istream& is, size_t curline)
{
    using impl::format_error;

    headers_map hm;

    //
    // Grammar
    //
    // header = entry+ nl
    // entry = line nl
    // line = text colon text
    //        (semicolon (text equal (text | dblquote string dblquote)))*
    // string = quoted_string
    //

    header::tokenizer tkz(is, curline);
    impl::parser< header::tokenizer > p(tkz);

    bool first = true;
    for (;;) {
        try {
            header_entry he;
            if (!header::read(p, he).good() || he.name().empty())
                break;

            if (first && he.name() != "Content-Type")
                throw format_error("Could not determine content type");
            else
                first = false;

            hm[he.name()] = he;
        } catch (const impl::parse_error& pe) {
            p.add_error(pe);
            p.reset(header::nl_type);
        }
    }

    if (!is.good())
        throw format_error("Unexpected end of stream");

    return std::pair< size_t, headers_map >(tkz.lineno(), hm);
}

void
impl::write_headers(const impl::headers_map& hm, std::ostream& os)
{
    PRE(!hm.empty());
    headers_map::const_iterator ct = hm.find("Content-Type");
    PRE(ct != hm.end());
    header::write(os, (*ct).second);
    for (headers_map::const_iterator iter = hm.begin(); iter != hm.end();
         iter++) {
        if ((*iter).first != "Content-Type")
            header::write(os, (*iter).second);
    }
    os << "\n";
}

void
impl::validate_content_type(const impl::headers_map& hm, const std::string& fmt,
                            int version)
{
    using impl::format_error;

    headers_map::const_iterator iter = hm.find("Content-Type");
    if (iter == hm.end())
        throw format_error("Could not determine content type");

    const header_entry& he = (*iter).second;
    if (he.value() != fmt)
        throw format_error("Mismatched content type: expected `" + fmt +
                           "' but got `" + he.value() + "'");

    if (!he.has_attr("version"))
        throw format_error("Could not determine version");
    const std::string& vstr = atf::text::to_string(version);
    if (he.get_attr("version") != vstr)
        throw format_error("Mismatched version: expected `" +
                           vstr + "' but got `" +
                           he.get_attr("version") + "'");
}
