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

#if !defined(_ATF_CXX_PARSER_HPP_)
#define _ATF_CXX_PARSER_HPP_

#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace atf {
namespace parser {

// ------------------------------------------------------------------------
// The "parse_error" class.
// ------------------------------------------------------------------------

class parse_error : public std::runtime_error,
                    public std::pair< size_t, std::string > {
    mutable std::string m_msg;

public:
    parse_error(size_t, std::string);
    ~parse_error(void) throw();

    const char* what(void) const throw();

    operator std::string(void) const;
};

// ------------------------------------------------------------------------
// The "parse_errors" class.
// ------------------------------------------------------------------------

class parse_errors : public std::runtime_error,
                     public std::vector< parse_error > {
    std::vector< parse_error > m_errors;
    mutable std::string m_msg;

public:
    parse_errors(void);
    ~parse_errors(void) throw();

    const char* what(void) const throw();
};

// ------------------------------------------------------------------------
// The "format_error" class.
// ------------------------------------------------------------------------

class format_error : public std::runtime_error {
public:
    format_error(const std::string&);
};

// ------------------------------------------------------------------------
// The "token" class.
// ------------------------------------------------------------------------

typedef int token_type;

//!
//! \brief Representation of a read token.
//!
//! A pair that contains the information of a token read from a stream.
//! It contains the token's type and its associated data, if any.
//!
struct token {
    bool m_inited;
    size_t m_line;
    token_type m_type;
    std::string m_text;

public:
    token(void);
    token(size_t, const token_type&, const std::string& = "");

    size_t lineno(void) const;
    const token_type& type(void) const;
    const std::string& text(void) const;

    operator bool(void) const;
    bool operator!(void) const;
};

// ------------------------------------------------------------------------
// The "tokenizer" class.
// ------------------------------------------------------------------------

//!
//! \brief A stream tokenizer.
//!
//! This template implements an extremely simple, line-oriented stream
//! tokenizer.  It is only able to recognize one character-long delimiters,
//! random-length keywords, skip whitespace and, anything that does not
//! match these rules is supposed to be a word.
//!
//! Parameter IS: The input stream's type.
//!
template< class IS >
class tokenizer {
    IS& m_is;
    size_t m_lineno;
    token m_la;

    bool m_skipws;
    token_type m_eof_type, m_nl_type, m_text_type;

    std::map< char, token_type > m_delims_map;
    std::string m_delims_str;

    char m_quotech;
    token_type m_quotetype;

    std::map< std::string, token_type > m_keywords_map;

    token_type alloc_type(void);

    template< class TKZ >
    friend
    class parser;

public:
    tokenizer(IS&, bool, const token_type&, const token_type&,
              const token_type&, size_t = 1);

    size_t lineno(void) const;

    void add_delim(char, const token_type&);
    void add_keyword(const std::string&, const token_type&);
    void add_quote(char, const token_type&);

    token next(void);
    std::string rest_of_line(void);
};

template< class IS >
tokenizer< IS >::tokenizer(IS& p_is,
                           bool p_skipws,
                           const token_type& p_eof_type,
                           const token_type& p_nl_type,
                           const token_type& p_text_type,
                           size_t p_lineno) :
    m_is(p_is),
    m_lineno(p_lineno),
    m_skipws(p_skipws),
    m_eof_type(p_eof_type),
    m_nl_type(p_nl_type),
    m_text_type(p_text_type),
    m_quotech(-1)
{
}

template< class IS >
size_t
tokenizer< IS >::lineno(void)
    const
{
    return m_lineno;
}

template< class IS >
void
tokenizer< IS >::add_delim(char delim, const token_type& type)
{
    m_delims_map[delim] = type;
    m_delims_str += delim;
}

template< class IS >
void
tokenizer< IS >::add_keyword(const std::string& keyword,
                             const token_type& type)
{
    m_keywords_map[keyword] = type;
}

template< class IS >
void
tokenizer< IS >::add_quote(char ch, const token_type& type)
{
    m_quotech = ch;
    m_quotetype = type;
}

template< class IS >
token
tokenizer< IS >::next(void)
{
    if (m_la) {
        token t = m_la;
        m_la = token();
        if (t.type() == m_nl_type)
            m_lineno++;
        return t;
    }

    char ch;
    std::string text;

    bool done = false, quoted = false;
    token t(m_lineno, m_eof_type, "<<EOF>>");
    while (!done && m_is.get(ch).good()) {
        if (ch == m_quotech) {
            if (text.empty()) {
                bool escaped = false;
                while (!done && m_is.get(ch).good()) {
                    if (!escaped) {
                        if (ch == '\\')
                            escaped = true;
                        else if (ch == '\n') {
                            m_la = token(m_lineno, m_nl_type, "<<NEWLINE>>");
                            throw parse_error(t.lineno(),
                                              "Missing double quotes before "
                                              "end of line");
                        } else if (ch == m_quotech)
                            done = true;
                        else
                            text += ch;
                    } else {
                        text += ch;
                        escaped = false;
                    }
                }
                if (!m_is.good())
                    throw parse_error(t.lineno(),
                                      "Missing double quotes before "
                                      "end of file");
                t = token(m_lineno, m_text_type, text);
                quoted = true;
            } else {
                m_is.putback(ch);
                done = true;
            }
        } else {
            typename std::map< char, token_type >::const_iterator idelim;
            idelim = m_delims_map.find(ch);
            if (idelim != m_delims_map.end()) {
                done = true;
                if (text.empty())
                    t = token(m_lineno, (*idelim).second,
                                   std::string("") + ch);
                else
                    m_is.putback(ch);
            } else if (ch == '\n') {
                done = true;
                if (text.empty())
                    t = token(m_lineno, m_nl_type, "<<NEWLINE>>");
                else
                    m_is.putback(ch);
            } else if (m_skipws && (ch == ' ' || ch == '\t')) {
                if (!text.empty())
                    done = true;
            } else
                text += ch;
        }
    }

    if (!quoted && !text.empty()) {
        typename std::map< std::string, token_type >::const_iterator ikw;
        ikw = m_keywords_map.find(text);
        if (ikw != m_keywords_map.end())
            t = token(m_lineno, (*ikw).second, text);
        else
            t = token(m_lineno, m_text_type, text);
    }

    if (t.type() == m_nl_type)
        m_lineno++;

    return t;
}

template< class IS >
std::string
tokenizer< IS >::rest_of_line(void)
{
    std::string str;
    while (m_is.good() && m_is.peek() != '\n')
        str += m_is.get();
    return str;
}

// ------------------------------------------------------------------------
// The "parser" class.
// ------------------------------------------------------------------------

template< class TKZ >
class parser {
    TKZ& m_tkz;
    token m_last;
    parse_errors m_errors;
    bool m_thrown;

public:
    parser(TKZ& tkz);
    ~parser(void);

    bool good(void) const;
    void add_error(const parse_error&);
    bool has_errors(void) const;

    token next(void);
    std::string rest_of_line(void);
    token reset(const token_type&);

    token
    expect(const token_type&,
           const std::string&);

    token
    expect(const token_type&,
           const token_type&,
           const std::string&);

    token
    expect(const token_type&,
           const token_type&,
           const token_type&,
           const std::string&);

    token
    expect(const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const std::string&);

    token
    expect(const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const std::string&);

    token
    expect(const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const token_type&,
           const std::string&);
};

template< class TKZ >
parser< TKZ >::parser(TKZ& tkz) :
    m_tkz(tkz),
    m_thrown(false)
{
}

template< class TKZ >
parser< TKZ >::~parser(void)
{
    if (!m_errors.empty() && !m_thrown)
        throw m_errors;
}

template< class TKZ >
bool
parser< TKZ >::good(void)
    const
{
    return m_tkz.m_is.good();
}

template< class TKZ >
void
parser< TKZ >::add_error(const parse_error& pe)
{
    m_errors.push_back(pe);
}

template< class TKZ >
bool
parser< TKZ >::has_errors(void)
    const
{
    return !m_errors.empty();
}

template< class TKZ >
token
parser< TKZ >::next(void)
{
    token t = m_tkz.next();

    m_last = t;

    if (t.type() == m_tkz.m_eof_type) {
        if (!m_errors.empty()) {
            m_thrown = true;
            throw m_errors;
        }
    }

    return t;
}

template< class TKZ >
std::string
parser< TKZ >::rest_of_line(void)
{
    return m_tkz.rest_of_line();
}

template< class TKZ >
token
parser< TKZ >::reset(const token_type& stop)
{
    token t = m_last;

    while (t.type() != m_tkz.m_eof_type && t.type() != stop)
        t = next();

    return t;
}

template< class TKZ >
token
parser< TKZ >::expect(const token_type& t1,
                      const std::string& textual)
{
    token t = next();

    if (t.type() != t1)
        throw parse_error(t.lineno(),
                          "Unexpected token `" + t.text() +
                          "'; expected " + textual);

    return t;
}

template< class TKZ >
token
parser< TKZ >::expect(const token_type& t1,
                      const token_type& t2,
                      const std::string& textual)
{
    token t = next();

    if (t.type() != t1 && t.type() != t2)
        throw parse_error(t.lineno(),
                          "Unexpected token `" + t.text() +
                          "'; expected " + textual);

    return t;
}

template< class TKZ >
token
parser< TKZ >::expect(const token_type& t1,
                      const token_type& t2,
                      const token_type& t3,
                      const std::string& textual)
{
    token t = next();

    if (t.type() != t1 && t.type() != t2 && t.type() != t3)
        throw parse_error(t.lineno(),
                          "Unexpected token `" + t.text() +
                          "'; expected " + textual);

    return t;
}

template< class TKZ >
token
parser< TKZ >::expect(const token_type& t1,
                      const token_type& t2,
                      const token_type& t3,
                      const token_type& t4,
                      const std::string& textual)
{
    token t = next();

    if (t.type() != t1 && t.type() != t2 && t.type() != t3 &&
        t.type() != t4)
        throw parse_error(t.lineno(),
                          "Unexpected token `" + t.text() +
                          "'; expected " + textual);

    return t;
}

template< class TKZ >
token
parser< TKZ >::expect(const token_type& t1,
                      const token_type& t2,
                      const token_type& t3,
                      const token_type& t4,
                      const token_type& t5,
                      const token_type& t6,
                      const token_type& t7,
                      const std::string& textual)
{
    token t = next();

    if (t.type() != t1 && t.type() != t2 && t.type() != t3 &&
        t.type() != t4 && t.type() != t5 && t.type() != t6 &&
        t.type() != t7)
        throw parse_error(t.lineno(),
                          "Unexpected token `" + t.text() +
                          "'; expected " + textual);

    return t;
}

template< class TKZ >
token
parser< TKZ >::expect(const token_type& t1,
                      const token_type& t2,
                      const token_type& t3,
                      const token_type& t4,
                      const token_type& t5,
                      const token_type& t6,
                      const token_type& t7,
                      const token_type& t8,
                      const std::string& textual)
{
    token t = next();

    if (t.type() != t1 && t.type() != t2 && t.type() != t3 &&
        t.type() != t4 && t.type() != t5 && t.type() != t6 &&
        t.type() != t7 && t.type() != t8)
        throw parse_error(t.lineno(),
                          "Unexpected token `" + t.text() +
                          "'; expected " + textual);

    return t;
}

#define ATF_PARSER_CALLBACK(parser, func) \
    do { \
        if (!(parser).has_errors()) \
            func; \
    } while (false)

// ------------------------------------------------------------------------
// Header parsing.
// ------------------------------------------------------------------------

typedef std::map< std::string, std::string > attrs_map;

class header_entry {
    std::string m_name;
    std::string m_value;
    attrs_map m_attrs;

public:
    header_entry(void);
    header_entry(const std::string&, const std::string&,
                 attrs_map = attrs_map());

    const std::string& name(void) const;
    const std::string& value(void) const;
    const attrs_map& attrs(void) const;
    bool has_attr(const std::string&) const;
    const std::string& get_attr(const std::string&) const;
};

typedef std::map< std::string, header_entry > headers_map;

std::pair< size_t, headers_map > read_headers(std::istream&, size_t);
void write_headers(const headers_map&, std::ostream&);
void validate_content_type(const headers_map&, const std::string&, int);

} // namespace parser
} // namespace atf

#endif // !defined(_ATF_CXX_PARSER_HPP_)
