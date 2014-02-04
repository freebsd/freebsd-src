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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
}

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

extern "C" {
#include "atf-c/error.h"
#include "atf-c/tc.h"
#include "atf-c/utils.h"
}

#include "noncopyable.hpp"
#include "tests.hpp"

#include "detail/application.hpp"
#include "detail/auto_array.hpp"
#include "detail/env.hpp"
#include "detail/exceptions.hpp"
#include "detail/fs.hpp"
#include "detail/parser.hpp"
#include "detail/sanity.hpp"
#include "detail/text.hpp"

namespace impl = atf::tests;
namespace detail = atf::tests::detail;
#define IMPL_NAME "atf::tests"

// ------------------------------------------------------------------------
// The "atf_tp_writer" class.
// ------------------------------------------------------------------------

detail::atf_tp_writer::atf_tp_writer(std::ostream& os) :
    m_os(os),
    m_is_first(true)
{
    atf::parser::headers_map hm;
    atf::parser::attrs_map ct_attrs;
    ct_attrs["version"] = "1";
    hm["Content-Type"] = atf::parser::header_entry("Content-Type",
        "application/X-atf-tp", ct_attrs);
    atf::parser::write_headers(hm, m_os);
}

void
detail::atf_tp_writer::start_tc(const std::string& ident)
{
    if (!m_is_first)
        m_os << "\n";
    m_os << "ident: " << ident << "\n";
    m_os.flush();
}

void
detail::atf_tp_writer::end_tc(void)
{
    if (m_is_first)
        m_is_first = false;
}

void
detail::atf_tp_writer::tc_meta_data(const std::string& name,
                                    const std::string& value)
{
    PRE(name != "ident");
    m_os << name << ": " << value << "\n";
    m_os.flush();
}

// ------------------------------------------------------------------------
// Free helper functions.
// ------------------------------------------------------------------------

bool
detail::match(const std::string& regexp, const std::string& str)
{
    return atf::text::match(str, regexp);
}

// ------------------------------------------------------------------------
// The "tc" class.
// ------------------------------------------------------------------------

static std::map< atf_tc_t*, impl::tc* > wraps;
static std::map< const atf_tc_t*, const impl::tc* > cwraps;

struct impl::tc_impl : atf::noncopyable {
    std::string m_ident;
    atf_tc_t m_tc;
    bool m_has_cleanup;

    tc_impl(const std::string& ident, const bool has_cleanup) :
        m_ident(ident),
        m_has_cleanup(has_cleanup)
    {
    }

    static void
    wrap_head(atf_tc_t *tc)
    {
        std::map< atf_tc_t*, impl::tc* >::iterator iter = wraps.find(tc);
        INV(iter != wraps.end());
        (*iter).second->head();
    }

    static void
    wrap_body(const atf_tc_t *tc)
    {
        std::map< const atf_tc_t*, const impl::tc* >::const_iterator iter =
            cwraps.find(tc);
        INV(iter != cwraps.end());
        try {
            (*iter).second->body();
        } catch (const std::exception& e) {
            (*iter).second->fail("Caught unhandled exception: " + std::string(
                                     e.what()));
        } catch (...) {
            (*iter).second->fail("Caught unknown exception");
        }
    }

    static void
    wrap_cleanup(const atf_tc_t *tc)
    {
        std::map< const atf_tc_t*, const impl::tc* >::const_iterator iter =
            cwraps.find(tc);
        INV(iter != cwraps.end());
        (*iter).second->cleanup();
    }
};

impl::tc::tc(const std::string& ident, const bool has_cleanup) :
    pimpl(new tc_impl(ident, has_cleanup))
{
}

impl::tc::~tc(void)
{
    cwraps.erase(&pimpl->m_tc);
    wraps.erase(&pimpl->m_tc);

    atf_tc_fini(&pimpl->m_tc);
}

void
impl::tc::init(const vars_map& config)
{
    atf_error_t err;

    auto_array< const char * > array(new const char*[(config.size() * 2) + 1]);
    const char **ptr = array.get();
    for (vars_map::const_iterator iter = config.begin();
         iter != config.end(); iter++) {
         *ptr = (*iter).first.c_str();
         *(ptr + 1) = (*iter).second.c_str();
         ptr += 2;
    }
    *ptr = NULL;

    wraps[&pimpl->m_tc] = this;
    cwraps[&pimpl->m_tc] = this;

    err = atf_tc_init(&pimpl->m_tc, pimpl->m_ident.c_str(), pimpl->wrap_head,
        pimpl->wrap_body, pimpl->m_has_cleanup ? pimpl->wrap_cleanup : NULL,
        array.get());
    if (atf_is_error(err))
        throw_atf_error(err);
}

bool
impl::tc::has_config_var(const std::string& var)
    const
{
    return atf_tc_has_config_var(&pimpl->m_tc, var.c_str());
}

bool
impl::tc::has_md_var(const std::string& var)
    const
{
    return atf_tc_has_md_var(&pimpl->m_tc, var.c_str());
}

const std::string
impl::tc::get_config_var(const std::string& var)
    const
{
    return atf_tc_get_config_var(&pimpl->m_tc, var.c_str());
}

const std::string
impl::tc::get_config_var(const std::string& var, const std::string& defval)
    const
{
    return atf_tc_get_config_var_wd(&pimpl->m_tc, var.c_str(), defval.c_str());
}

const std::string
impl::tc::get_md_var(const std::string& var)
    const
{
    return atf_tc_get_md_var(&pimpl->m_tc, var.c_str());
}

const impl::vars_map
impl::tc::get_md_vars(void)
    const
{
    vars_map vars;

    char **array = atf_tc_get_md_vars(&pimpl->m_tc);
    try {
        char **ptr;
        for (ptr = array; *ptr != NULL; ptr += 2)
            vars[*ptr] = *(ptr + 1);
    } catch (...) {
        atf_utils_free_charpp(array);
        throw;
    }

    return vars;
}

void
impl::tc::set_md_var(const std::string& var, const std::string& val)
{
    atf_error_t err = atf_tc_set_md_var(&pimpl->m_tc, var.c_str(), val.c_str());
    if (atf_is_error(err))
        throw_atf_error(err);
}

void
impl::tc::run(const std::string& resfile)
    const
{
    atf_error_t err = atf_tc_run(&pimpl->m_tc, resfile.c_str());
    if (atf_is_error(err))
        throw_atf_error(err);
}

void
impl::tc::run_cleanup(void)
    const
{
    atf_error_t err = atf_tc_cleanup(&pimpl->m_tc);
    if (atf_is_error(err))
        throw_atf_error(err);
}

void
impl::tc::head(void)
{
}

void
impl::tc::cleanup(void)
    const
{
}

void
impl::tc::require_prog(const std::string& prog)
    const
{
    atf_tc_require_prog(prog.c_str());
}

void
impl::tc::pass(void)
{
    atf_tc_pass();
}

void
impl::tc::fail(const std::string& reason)
{
    atf_tc_fail("%s", reason.c_str());
}

void
impl::tc::fail_nonfatal(const std::string& reason)
{
    atf_tc_fail_nonfatal("%s", reason.c_str());
}

void
impl::tc::skip(const std::string& reason)
{
    atf_tc_skip("%s", reason.c_str());
}

void
impl::tc::check_errno(const char* file, const int line, const int exp_errno,
                      const char* expr_str, const bool result)
{
    atf_tc_check_errno(file, line, exp_errno, expr_str, result);
}

void
impl::tc::require_errno(const char* file, const int line, const int exp_errno,
                        const char* expr_str, const bool result)
{
    atf_tc_require_errno(file, line, exp_errno, expr_str, result);
}

void
impl::tc::expect_pass(void)
{
    atf_tc_expect_pass();
}

void
impl::tc::expect_fail(const std::string& reason)
{
    atf_tc_expect_fail("%s", reason.c_str());
}

void
impl::tc::expect_exit(const int exitcode, const std::string& reason)
{
    atf_tc_expect_exit(exitcode, "%s", reason.c_str());
}

void
impl::tc::expect_signal(const int signo, const std::string& reason)
{
    atf_tc_expect_signal(signo, "%s", reason.c_str());
}

void
impl::tc::expect_death(const std::string& reason)
{
    atf_tc_expect_death("%s", reason.c_str());
}

void
impl::tc::expect_timeout(const std::string& reason)
{
    atf_tc_expect_timeout("%s", reason.c_str());
}

// ------------------------------------------------------------------------
// The "tp" class.
// ------------------------------------------------------------------------

class tp : public atf::application::app {
public:
    typedef std::vector< impl::tc * > tc_vector;

private:
    static const char* m_description;

    bool m_lflag;
    atf::fs::path m_resfile;
    std::string m_srcdir_arg;
    atf::fs::path m_srcdir;

    atf::tests::vars_map m_vars;

    std::string specific_args(void) const;
    options_set specific_options(void) const;
    void process_option(int, const char*);

    void (*m_add_tcs)(tc_vector&);
    tc_vector m_tcs;

    void parse_vflag(const std::string&);
    void handle_srcdir(void);

    tc_vector init_tcs(void);

    enum tc_part {
        BODY,
        CLEANUP,
    };

    void list_tcs(void);
    impl::tc* find_tc(tc_vector, const std::string&);
    static std::pair< std::string, tc_part > process_tcarg(const std::string&);
    int run_tc(const std::string&);

public:
    tp(void (*)(tc_vector&));
    ~tp(void);

    int main(void);
};

const char* tp::m_description =
    "This is an independent atf test program.";

tp::tp(void (*add_tcs)(tc_vector&)) :
    app(m_description, "atf-test-program(1)", "atf(7)", false),
    m_lflag(false),
    m_resfile("/dev/stdout"),
    m_srcdir("."),
    m_add_tcs(add_tcs)
{
}

tp::~tp(void)
{
    for (tc_vector::iterator iter = m_tcs.begin();
         iter != m_tcs.end(); iter++) {
        impl::tc* tc = *iter;

        delete tc;
    }
}

std::string
tp::specific_args(void)
    const
{
    return "test_case";
}

tp::options_set
tp::specific_options(void)
    const
{
    using atf::application::option;
    options_set opts;
    opts.insert(option('l', "", "List test cases and their purpose"));
    opts.insert(option('r', "resfile", "The file to which the test program "
                                       "will write the results of the "
                                       "executed test case"));
    opts.insert(option('s', "srcdir", "Directory where the test's data "
                                      "files are located"));
    opts.insert(option('v', "var=value", "Sets the configuration variable "
                                         "`var' to `value'"));
    return opts;
}

void
tp::process_option(int ch, const char* arg)
{
    switch (ch) {
    case 'l':
        m_lflag = true;
        break;

    case 'r':
        m_resfile = atf::fs::path(arg);
        break;

    case 's':
        m_srcdir_arg = arg;
        break;

    case 'v':
        parse_vflag(arg);
        break;

    default:
        UNREACHABLE;
    }
}

void
tp::parse_vflag(const std::string& str)
{
    if (str.empty())
        throw std::runtime_error("-v requires a non-empty argument");

    std::vector< std::string > ws = atf::text::split(str, "=");
    if (ws.size() == 1 && str[str.length() - 1] == '=') {
        m_vars[ws[0]] = "";
    } else {
        if (ws.size() != 2)
            throw std::runtime_error("-v requires an argument of the form "
                                     "var=value");

        m_vars[ws[0]] = ws[1];
    }
}

void
tp::handle_srcdir(void)
{
    if (m_srcdir_arg.empty()) {
        m_srcdir = atf::fs::path(m_argv0).branch_path();
        if (m_srcdir.leaf_name() == ".libs")
            m_srcdir = m_srcdir.branch_path();
    } else
        m_srcdir = atf::fs::path(m_srcdir_arg);

    if (!atf::fs::exists(m_srcdir / m_prog_name))
        throw std::runtime_error("Cannot find the test program in the "
                                 "source directory `" + m_srcdir.str() + "'");

    if (!m_srcdir.is_absolute())
        m_srcdir = m_srcdir.to_absolute();

    m_vars["srcdir"] = m_srcdir.str();
}

tp::tc_vector
tp::init_tcs(void)
{
    m_add_tcs(m_tcs);
    for (tc_vector::iterator iter = m_tcs.begin();
         iter != m_tcs.end(); iter++) {
        impl::tc* tc = *iter;

        tc->init(m_vars);
    }
    return m_tcs;
}

//
// An auxiliary unary predicate that compares the given test case's
// identifier to the identifier stored in it.
//
class tc_equal_to_ident {
    const std::string& m_ident;

public:
    tc_equal_to_ident(const std::string& i) :
        m_ident(i)
    {
    }

    bool operator()(const impl::tc* tc)
    {
        return tc->get_md_var("ident") == m_ident;
    }
};

void
tp::list_tcs(void)
{
    tc_vector tcs = init_tcs();
    detail::atf_tp_writer writer(std::cout);

    for (tc_vector::const_iterator iter = tcs.begin();
         iter != tcs.end(); iter++) {
        const impl::vars_map vars = (*iter)->get_md_vars();

        {
            impl::vars_map::const_iterator iter2 = vars.find("ident");
            INV(iter2 != vars.end());
            writer.start_tc((*iter2).second);
        }

        for (impl::vars_map::const_iterator iter2 = vars.begin();
             iter2 != vars.end(); iter2++) {
            const std::string& key = (*iter2).first;
            if (key != "ident")
                writer.tc_meta_data(key, (*iter2).second);
        }

        writer.end_tc();
    }
}

impl::tc*
tp::find_tc(tc_vector tcs, const std::string& name)
{
    std::vector< std::string > ids;
    for (tc_vector::iterator iter = tcs.begin();
         iter != tcs.end(); iter++) {
        impl::tc* tc = *iter;

        if (tc->get_md_var("ident") == name)
            return tc;
    }
    throw atf::application::usage_error("Unknown test case `%s'",
                                        name.c_str());
}

std::pair< std::string, tp::tc_part >
tp::process_tcarg(const std::string& tcarg)
{
    const std::string::size_type pos = tcarg.find(':');
    if (pos == std::string::npos) {
        return std::make_pair(tcarg, BODY);
    } else {
        const std::string tcname = tcarg.substr(0, pos);

        const std::string partname = tcarg.substr(pos + 1);
        if (partname == "body")
            return std::make_pair(tcname, BODY);
        else if (partname == "cleanup")
            return std::make_pair(tcname, CLEANUP);
        else {
            using atf::application::usage_error;
            throw usage_error("Invalid test case part `%s'", partname.c_str());
        }
    }
}

int
tp::run_tc(const std::string& tcarg)
{
    const std::pair< std::string, tc_part > fields = process_tcarg(tcarg);

    impl::tc* tc = find_tc(init_tcs(), fields.first);

    if (!atf::env::has("__RUNNING_INSIDE_ATF_RUN") || atf::env::get(
        "__RUNNING_INSIDE_ATF_RUN") != "internal-yes-value")
    {
        std::cerr << m_prog_name << ": WARNING: Running test cases without "
            "atf-run(1) is unsupported\n";
        std::cerr << m_prog_name << ": WARNING: No isolation nor timeout "
            "control is being applied; you may get unexpected failures; see "
            "atf-test-case(4)\n";
    }

    try {
        switch (fields.second) {
        case BODY:
            tc->run(m_resfile.str());
            break;
        case CLEANUP:
            tc->run_cleanup();
            break;
        default:
            UNREACHABLE;
        }
        return EXIT_SUCCESS;
    } catch (const std::runtime_error& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}

int
tp::main(void)
{
    using atf::application::usage_error;

    int errcode;

    handle_srcdir();

    if (m_lflag) {
        if (m_argc > 0)
            throw usage_error("Cannot provide test case names with -l");

        list_tcs();
        errcode = EXIT_SUCCESS;
    } else {
        if (m_argc == 0)
            throw usage_error("Must provide a test case name");
        else if (m_argc > 1)
            throw usage_error("Cannot provide more than one test case name");
        INV(m_argc == 1);

        errcode = run_tc(m_argv[0]);
    }

    return errcode;
}

namespace atf {
    namespace tests {
        int run_tp(int, char* const*, void (*)(tp::tc_vector&));
    }
}

int
impl::run_tp(int argc, char* const* argv, void (*add_tcs)(tp::tc_vector&))
{
    return tp(add_tcs).run(argc, argv);
}
