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

#if !defined(_ATF_CXX_MACROS_HPP_)
#define _ATF_CXX_MACROS_HPP_

#include <sstream>
#include <stdexcept>
#include <vector>

#include <atf-c++/tests.hpp>

// Do not define inline methods for the test case classes.  Doing so
// significantly increases the memory requirements of GNU G++ during
// compilation.

#define ATF_TEST_CASE_WITHOUT_HEAD(name) \
    namespace { \
    class atfu_tc_ ## name : public atf::tests::tc { \
        void body(void) const; \
    public: \
        atfu_tc_ ## name(void); \
    }; \
    static atfu_tc_ ## name* atfu_tcptr_ ## name; \
    atfu_tc_ ## name::atfu_tc_ ## name(void) : atf::tests::tc(#name, false) {} \
    }

#define ATF_TEST_CASE(name) \
    namespace { \
    class atfu_tc_ ## name : public atf::tests::tc { \
        void head(void); \
        void body(void) const; \
    public: \
        atfu_tc_ ## name(void); \
    }; \
    static atfu_tc_ ## name* atfu_tcptr_ ## name; \
    atfu_tc_ ## name::atfu_tc_ ## name(void) : atf::tests::tc(#name, false) {} \
    }

#define ATF_TEST_CASE_WITH_CLEANUP(name) \
    namespace { \
    class atfu_tc_ ## name : public atf::tests::tc { \
        void head(void); \
        void body(void) const; \
        void cleanup(void) const; \
    public: \
        atfu_tc_ ## name(void); \
    }; \
    static atfu_tc_ ## name* atfu_tcptr_ ## name; \
    atfu_tc_ ## name::atfu_tc_ ## name(void) : atf::tests::tc(#name, true) {} \
    }

#define ATF_TEST_CASE_NAME(name) atfu_tc_ ## name
#define ATF_TEST_CASE_USE(name) (atfu_tcptr_ ## name) = NULL

#define ATF_TEST_CASE_HEAD(name) \
    void \
    atfu_tc_ ## name::head(void)

#define ATF_TEST_CASE_BODY(name) \
    void \
    atfu_tc_ ## name::body(void) \
        const

#define ATF_TEST_CASE_CLEANUP(name) \
    void \
    atfu_tc_ ## name::cleanup(void) \
        const

#define ATF_FAIL(reason) atf::tests::tc::fail(reason)

#define ATF_SKIP(reason) atf::tests::tc::skip(reason)

#define ATF_PASS() atf::tests::tc::pass()

#define ATF_REQUIRE(x) \
    do { \
        if (!(x)) { \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " << #x << " not met"; \
            atf::tests::tc::fail(atfu_ss.str()); \
        } \
    } while (false)

#define ATF_REQUIRE_EQ(x, y) \
    do { \
        if ((x) != (y)) { \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " << #x << " != " << #y \
                     << " (" << (x) << " != " << (y) << ")"; \
            atf::tests::tc::fail(atfu_ss.str()); \
        } \
    } while (false)

#define ATF_REQUIRE_IN(element, collection) \
    ATF_REQUIRE((collection).find(element) != (collection).end())

#define ATF_REQUIRE_NOT_IN(element, collection) \
    ATF_REQUIRE((collection).find(element) == (collection).end())

#define ATF_REQUIRE_MATCH(regexp, string) \
    do { \
        if (!atf::tests::detail::match(regexp, string)) { \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": '" << string << "' does not " \
                    << "match regexp '" << regexp << "'"; \
            atf::tests::tc::fail(atfu_ss.str()); \
        } \
    } while (false)

#define ATF_REQUIRE_THROW(e, x) \
    do { \
        try { \
            x; \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " #x " did not throw " \
                        #e " as expected"; \
            atf::tests::tc::fail(atfu_ss.str()); \
        } catch (const e&) { \
        } catch (const std::exception& atfu_e) { \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " #x " threw an " \
                        "unexpected error (not " #e "): " << atfu_e.what(); \
            atf::tests::tc::fail(atfu_ss.str()); \
        } catch (...) { \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " #x " threw an " \
                        "unexpected error (not " #e ")"; \
            atf::tests::tc::fail(atfu_ss.str()); \
        } \
    } while (false)

#define ATF_REQUIRE_THROW_RE(type, regexp, x) \
    do { \
        try { \
            x; \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " #x " did not throw " \
                        #type " as expected"; \
            atf::tests::tc::fail(atfu_ss.str()); \
        } catch (const type& e) { \
            if (!atf::tests::detail::match(regexp, e.what())) { \
                std::ostringstream atfu_ss; \
                atfu_ss << "Line " << __LINE__ << ": " #x " threw " #type "(" \
                        << e.what() << "), but does not match '" << regexp \
                        << "'"; \
                atf::tests::tc::fail(atfu_ss.str()); \
            } \
        } catch (const std::exception& atfu_e) { \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " #x " threw an " \
                        "unexpected error (not " #type "): " << atfu_e.what(); \
            atf::tests::tc::fail(atfu_ss.str()); \
        } catch (...) { \
            std::ostringstream atfu_ss; \
            atfu_ss << "Line " << __LINE__ << ": " #x " threw an " \
                        "unexpected error (not " #type ")"; \
            atf::tests::tc::fail(atfu_ss.str()); \
        } \
    } while (false)

#define ATF_CHECK_ERRNO(exp_errno, bool_expr) \
    atf::tests::tc::check_errno(__FILE__, __LINE__, exp_errno, #bool_expr, \
                                bool_expr)

#define ATF_REQUIRE_ERRNO(exp_errno, bool_expr) \
    atf::tests::tc::require_errno(__FILE__, __LINE__, exp_errno, #bool_expr, \
                                  bool_expr)

#define ATF_INIT_TEST_CASES(tcs) \
    namespace atf { \
        namespace tests { \
            int run_tp(int, char* const*, \
                       void (*)(std::vector< atf::tests::tc * >&)); \
        } \
    } \
    \
    static void atfu_init_tcs(std::vector< atf::tests::tc * >&); \
    \
    int \
    main(int argc, char* const* argv) \
    { \
        return atf::tests::run_tp(argc, argv, atfu_init_tcs); \
    } \
    \
    static \
    void \
    atfu_init_tcs(std::vector< atf::tests::tc * >& tcs)

#define ATF_ADD_TEST_CASE(tcs, tcname) \
    do { \
        atfu_tcptr_ ## tcname = new atfu_tc_ ## tcname(); \
        (tcs).push_back(atfu_tcptr_ ## tcname); \
    } while (0);

#endif // !defined(_ATF_CXX_MACROS_HPP_)
