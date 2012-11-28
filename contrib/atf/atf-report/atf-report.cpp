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

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "atf-c/defs.h"

#include "atf-c++/detail/application.hpp"
#include "atf-c++/detail/fs.hpp"
#include "atf-c++/detail/sanity.hpp"
#include "atf-c++/detail/text.hpp"
#include "atf-c++/detail/ui.hpp"

#include "reader.hpp"

typedef std::auto_ptr< std::ostream > ostream_ptr;

static ostream_ptr
open_outfile(const atf::fs::path& path)
{
    ostream_ptr osp;
    if (path.str() == "-")
        osp = ostream_ptr(new std::ofstream("/dev/stdout"));
    else
        osp = ostream_ptr(new std::ofstream(path.c_str()));
    if (!(*osp))
        throw std::runtime_error("Could not create file " + path.str());
    return osp;
}

static std::string
format_tv(struct timeval* tv)
{
    std::ostringstream output;
    output << static_cast< long >(tv->tv_sec) << '.'
           << std::setfill('0') << std::setw(6)
           << static_cast< long >(tv->tv_usec);
    return output.str();
}

// ------------------------------------------------------------------------
// The "writer" interface.
// ------------------------------------------------------------------------

//!
//! \brief A base class that defines an output format.
//!
//! The writer base class defines a generic interface to output formats.
//! This is meant to be subclassed, and each subclass can redefine any
//! method to format the information as it wishes.
//!
//! This class is not tied to a output stream nor a file because, depending
//! on the output format, we will want to write to a single file or to
//! multiple ones.
//!
class writer {
public:
    writer(void) {}
    virtual ~writer(void) {}

    virtual void write_info(const std::string&, const std::string&) {}
    virtual void write_ntps(size_t) {}
    virtual void write_tp_start(const std::string&, size_t) {}
    virtual void write_tp_end(struct timeval*, const std::string&) {}
    virtual void write_tc_start(const std::string&) {}
    virtual void write_tc_stdout_line(const std::string&) {}
    virtual void write_tc_stderr_line(const std::string&) {}
    virtual void write_tc_end(const std::string&, struct timeval*,
                              const std::string&) {}
    virtual void write_eof(void) {}
};

// ------------------------------------------------------------------------
// The "csv_writer" class.
// ------------------------------------------------------------------------

//!
//! \brief A very simple plain-text output format.
//!
//! The csv_writer class implements a very simple plain-text output
//! format that summarizes the results of each executed test case.  The
//! results are meant to be easily parseable by third-party tools, hence
//! they are formatted as a CSV file.
//!
class csv_writer : public writer {
    ostream_ptr m_os;
    bool m_failed;

    std::string m_tpname;
    std::string m_tcname;

public:
    csv_writer(const atf::fs::path& p) :
        m_os(open_outfile(p))
    {
    }

    virtual
    void
    write_tp_start(const std::string& name,
                   size_t ntcs ATF_DEFS_ATTRIBUTE_UNUSED)
    {
        m_tpname = name;
        m_failed = false;
    }

    virtual
    void
    write_tp_end(struct timeval* tv, const std::string& reason)
    {
        const std::string timestamp = format_tv(tv);

        if (!reason.empty())
            (*m_os) << "tp, " << timestamp << ", " << m_tpname << ", bogus, "
                    << reason << "\n";
        else if (m_failed)
            (*m_os) << "tp, " << timestamp << ", "<< m_tpname << ", failed\n";
        else
            (*m_os) << "tp, " << timestamp << ", "<< m_tpname << ", passed\n";
    }

    virtual
    void
    write_tc_start(const std::string& name)
    {
        m_tcname = name;
    }

    virtual
    void
    write_tc_end(const std::string& state, struct timeval* tv,
                 const std::string& reason)
    {
        std::string str = m_tpname + ", " + m_tcname + ", " + state;
        if (!reason.empty())
            str += ", " + reason;
        (*m_os) << "tc, " << format_tv(tv) << ", " << str << "\n";

        if (state == "failed")
            m_failed = true;
    }
};

// ------------------------------------------------------------------------
// The "ticker_writer" class.
// ------------------------------------------------------------------------

//!
//! \brief A console-friendly output format.
//!
//! The ticker_writer class implements a formatter that is user-friendly
//! in the sense that it shows the execution of test cases in an easy to
//! read format.  It is not meant to be parseable and its format can
//! freely change across releases.
//!
class ticker_writer : public writer {
    ostream_ptr m_os;

    size_t m_curtp, m_ntps;
    size_t m_tcs_passed, m_tcs_failed, m_tcs_skipped, m_tcs_expected_failures;
    std::string m_tcname, m_tpname;
    std::vector< std::string > m_failed_tcs;
    std::map< std::string, std::string > m_expected_failures_tcs;
    std::vector< std::string > m_failed_tps;

    void
    write_info(const std::string& what, const std::string& val)
    {
        if (what == "tests.root") {
            (*m_os) << "Tests root: " << val << "\n\n";
        }
    }

    void
    write_ntps(size_t ntps)
    {
        m_curtp = 1;
        m_tcs_passed = 0;
        m_tcs_failed = 0;
        m_tcs_skipped = 0;
        m_tcs_expected_failures = 0;
        m_ntps = ntps;
    }

    void
    write_tp_start(const std::string& tp, size_t ntcs)
    {
        using atf::text::to_string;
        using atf::ui::format_text;

        m_tpname = tp;

        (*m_os) << format_text(tp + " (" + to_string(m_curtp) +
                               "/" + to_string(m_ntps) + "): " +
                               to_string(ntcs) + " test cases")
                << "\n";
        (*m_os).flush();
    }

    void
    write_tp_end(struct timeval* tv, const std::string& reason)
    {
        using atf::ui::format_text_with_tag;

        m_curtp++;

        if (!reason.empty()) {
            (*m_os) << format_text_with_tag("BOGUS TEST PROGRAM: Cannot "
                                            "trust its results because "
                                            "of `" + reason + "'",
                                            m_tpname + ": ", false)
                    << "\n";
            m_failed_tps.push_back(m_tpname);
        }
        (*m_os) << "[" << format_tv(tv) << "s]\n\n";
        (*m_os).flush();

        m_tpname.clear();
    }

    void
    write_tc_start(const std::string& tcname)
    {
        m_tcname = tcname;

        (*m_os) << "    " + tcname + ": ";
        (*m_os).flush();
    }

    void
    write_tc_end(const std::string& state, struct timeval* tv,
                 const std::string& reason)
    {
        std::string str;

        (*m_os) << "[" << format_tv(tv) << "s] ";

        if (state == "expected_death" || state == "expected_exit" ||
            state == "expected_failure" || state == "expected_signal" ||
            state == "expected_timeout") {
            str = "Expected failure: " + reason;
            m_tcs_expected_failures++;
            m_expected_failures_tcs[m_tpname + ":" + m_tcname] = reason;
        } else if (state == "failed") {
            str = "Failed: " + reason;
            m_tcs_failed++;
            m_failed_tcs.push_back(m_tpname + ":" + m_tcname);
        } else if (state == "passed") {
            str = "Passed.";
            m_tcs_passed++;
        } else if (state == "skipped") {
            str = "Skipped: " + reason;
            m_tcs_skipped++;
        } else
            UNREACHABLE;

        // XXX Wrap text.  format_text_with_tag does not currently allow
        // to specify the current column, which is needed because we have
        // already printed the tc's name.
        (*m_os) << str << '\n';

        m_tcname = "";
    }

    static void
    write_expected_failures(const std::map< std::string, std::string >& xfails,
                            std::ostream& os)
    {
        using atf::ui::format_text;
        using atf::ui::format_text_with_tag;

        os << format_text("Test cases for known bugs:") << "\n";

        for (std::map< std::string, std::string >::const_iterator iter =
             xfails.begin(); iter != xfails.end(); iter++) {
            const std::string& name = (*iter).first;
            const std::string& reason = (*iter).second;

            os << format_text_with_tag(reason, "    " + name + ": ", false)
               << "\n";
        }
    }

    void
    write_eof(void)
    {
        using atf::text::join;
        using atf::text::to_string;
        using atf::ui::format_text;
        using atf::ui::format_text_with_tag;

        if (!m_failed_tps.empty()) {
            (*m_os) << format_text("Failed (bogus) test programs:")
                    << "\n";
            (*m_os) << format_text_with_tag(join(m_failed_tps, ", "),
                                            "    ", false) << "\n\n";
        }

        if (!m_expected_failures_tcs.empty()) {
            write_expected_failures(m_expected_failures_tcs, *m_os);
            (*m_os) << "\n";
        }

        if (!m_failed_tcs.empty()) {
            (*m_os) << format_text("Failed test cases:") << "\n";
            (*m_os) << format_text_with_tag(join(m_failed_tcs, ", "),
                                            "    ", false) << "\n\n";
        }

        (*m_os) << format_text("Summary for " + to_string(m_ntps) +
                               " test programs:") << "\n";
        (*m_os) << format_text_with_tag(to_string(m_tcs_passed) +
                                        " passed test cases.",
                                        "    ", false) << "\n";
        (*m_os) << format_text_with_tag(to_string(m_tcs_failed) +
                                        " failed test cases.",
                                        "    ", false) << "\n";
        (*m_os) << format_text_with_tag(to_string(m_tcs_expected_failures) +
                                        " expected failed test cases.",
                                        "    ", false) << "\n";
        (*m_os) << format_text_with_tag(to_string(m_tcs_skipped) +
                                        " skipped test cases.",
                                        "    ", false) << "\n";
    }

public:
    ticker_writer(const atf::fs::path& p) :
        m_os(open_outfile(p))
    {
    }
};

// ------------------------------------------------------------------------
// The "xml" class.
// ------------------------------------------------------------------------

//!
//! \brief A single-file XML output format.
//!
//! The xml_writer class implements a formatter that prints the results
//! of test cases in an XML format easily parseable later on by other
//! utilities.
//!
class xml_writer : public writer {
    ostream_ptr m_os;

    std::string m_tcname, m_tpname;

    static
    std::string
    attrval(const std::string& str)
    {
        return str;
    }

    static
    std::string
    elemval(const std::string& str)
    {
        std::string ostr;
        for (std::string::const_iterator iter = str.begin();
             iter != str.end(); iter++) {
            switch (*iter) {
            case '&': ostr += "&amp;"; break;
            case '<': ostr += "&lt;"; break;
            case '>': ostr += "&gt;"; break;
            default:  ostr += *iter;
            }
        }
        return ostr;
    }

    void
    write_info(const std::string& what, const std::string& val)
    {
        (*m_os) << "<info class=\"" << what << "\">" << val << "</info>\n";
    }

    void
    write_tp_start(const std::string& tp,
                   size_t ntcs ATF_DEFS_ATTRIBUTE_UNUSED)
    {
        (*m_os) << "<tp id=\"" << attrval(tp) << "\">\n";
    }

    void
    write_tp_end(struct timeval* tv, const std::string& reason)
    {
        if (!reason.empty())
            (*m_os) << "<failed>" << elemval(reason) << "</failed>\n";
        (*m_os) << "<tp-time>" << format_tv(tv) << "</tp-time>";
        (*m_os) << "</tp>\n";
    }

    void
    write_tc_start(const std::string& tcname)
    {
        (*m_os) << "<tc id=\"" << attrval(tcname) << "\">\n";
    }

    void
    write_tc_stdout_line(const std::string& line)
    {
        (*m_os) << "<so>" << elemval(line) << "</so>\n";
    }

    void
    write_tc_stderr_line(const std::string& line)
    {
        (*m_os) << "<se>" << elemval(line) << "</se>\n";
    }

    void
    write_tc_end(const std::string& state, struct timeval* tv,
                 const std::string& reason)
    {
        std::string str;

        if (state == "expected_death" || state == "expected_exit" ||
            state == "expected_failure" || state == "expected_signal" ||
            state == "expected_timeout") {
            (*m_os) << "<" << state << ">" << elemval(reason)
                    << "</" << state << ">\n";
        } else if (state == "passed") {
            (*m_os) << "<passed />\n";
        } else if (state == "failed") {
            (*m_os) << "<failed>" << elemval(reason) << "</failed>\n";
        } else if (state == "skipped") {
            (*m_os) << "<skipped>" << elemval(reason) << "</skipped>\n";
        } else
            UNREACHABLE;
        (*m_os) << "<tc-time>" << format_tv(tv) << "</tc-time>";
        (*m_os) << "</tc>\n";
    }

    void
    write_eof(void)
    {
        (*m_os) << "</tests-results>\n";
    }

public:
    xml_writer(const atf::fs::path& p) :
        m_os(open_outfile(p))
    {
        (*m_os) << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
                << "<!DOCTYPE tests-results PUBLIC "
                   "\"-//NetBSD//DTD ATF Tests Results 0.1//EN\" "
                   "\"http://www.NetBSD.org/XML/atf/tests-results.dtd\">\n\n"
                   "<tests-results>\n";
    }
};

// ------------------------------------------------------------------------
// The "converter" class.
// ------------------------------------------------------------------------

//!
//! \brief A reader that redirects events to multiple writers.
//!
//! The converter class implements an atf_tps_reader that, for each event
//! raised by the parser, redirects it to multiple writers so that they
//! can reformat it according to their output rules.
//!
class converter : public atf::atf_report::atf_tps_reader {
    typedef std::vector< writer* > outs_vector;
    outs_vector m_outs;

    void
    got_info(const std::string& what, const std::string& val)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_info(what, val);
    }

    void
    got_ntps(size_t ntps)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_ntps(ntps);
    }

    void
    got_tp_start(const std::string& tp, size_t ntcs)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_tp_start(tp, ntcs);
    }

    void
    got_tp_end(struct timeval* tv, const std::string& reason)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_tp_end(tv, reason);
    }

    void
    got_tc_start(const std::string& tcname)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_tc_start(tcname);
    }

    void
    got_tc_stdout_line(const std::string& line)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_tc_stdout_line(line);
    }

    void
    got_tc_stderr_line(const std::string& line)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_tc_stderr_line(line);
    }

    void
    got_tc_end(const std::string& state, struct timeval* tv,
               const std::string& reason)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_tc_end(state, tv, reason);
    }

    void
    got_eof(void)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            (*iter)->write_eof();
    }

public:
    converter(std::istream& is) :
        atf::atf_report::atf_tps_reader(is)
    {
    }

    ~converter(void)
    {
        for (outs_vector::iterator iter = m_outs.begin();
             iter != m_outs.end(); iter++)
            delete *iter;
    }

    void
    add_output(const std::string& fmt, const atf::fs::path& p)
    {
        if (fmt == "csv") {
            m_outs.push_back(new csv_writer(p));
        } else if (fmt == "ticker") {
            m_outs.push_back(new ticker_writer(p));
        } else if (fmt == "xml") {
            m_outs.push_back(new xml_writer(p));
        } else
            throw std::runtime_error("Unknown format `" + fmt + "'");
    }
};

// ------------------------------------------------------------------------
// The "atf_report" class.
// ------------------------------------------------------------------------

class atf_report : public atf::application::app {
    static const char* m_description;

    typedef std::pair< std::string, atf::fs::path > fmt_path_pair;
    std::vector< fmt_path_pair > m_oflags;

    void process_option(int, const char*);
    options_set specific_options(void) const;

public:
    atf_report(void);

    int main(void);
};

const char* atf_report::m_description =
    "atf-report is a tool that parses the output of atf-run and "
    "generates user-friendly reports in multiple different formats.";

atf_report::atf_report(void) :
    app(m_description, "atf-report(1)", "atf(7)")
{
}

void
atf_report::process_option(int ch, const char* arg)
{
    switch (ch) {
    case 'o':
        {
            std::string str(arg);
            std::string::size_type pos = str.find(':');
            if (pos == std::string::npos)
                throw std::runtime_error("Syntax error in -o option");
            else {
                std::string fmt = str.substr(0, pos);
                atf::fs::path path = atf::fs::path(str.substr(pos + 1));
                m_oflags.push_back(fmt_path_pair(fmt, path));
            }
        }
        break;

    default:
        UNREACHABLE;
    }
}

atf_report::options_set
atf_report::specific_options(void)
    const
{
    using atf::application::option;
    options_set opts;
    opts.insert(option('o', "fmt:path", "Adds a new output file; multiple "
                                        "ones can be specified, and a - "
                                        "path means stdout"));
    return opts;
}

int
atf_report::main(void)
{
    if (m_argc > 0)
        throw std::runtime_error("No arguments allowed");

    if (m_oflags.empty())
        m_oflags.push_back(fmt_path_pair("ticker", atf::fs::path("-")));

    // Look for path duplicates.
    std::set< atf::fs::path > paths;
    for (std::vector< fmt_path_pair >::const_iterator iter = m_oflags.begin();
         iter != m_oflags.end(); iter++) {
        atf::fs::path p = (*iter).second;
        if (p == atf::fs::path("/dev/stdout"))
            p = atf::fs::path("-");
        if (paths.find(p) != paths.end())
            throw std::runtime_error("The file `" + p.str() + "' was "
                                     "specified more than once");
        paths.insert((*iter).second);
    }

    // Generate the output files.
    converter cnv(std::cin);
    for (std::vector< fmt_path_pair >::const_iterator iter = m_oflags.begin();
         iter != m_oflags.end(); iter++)
        cnv.add_output((*iter).first, (*iter).second);
    cnv.read();

    return EXIT_SUCCESS;
}

int
main(int argc, char* const* argv)
{
    return atf_report().run(argc, argv);
}
