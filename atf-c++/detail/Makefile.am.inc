#
# Automated Testing Framework (atf)
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

libatf_c___la_SOURCES += atf-c++/detail/application.cpp \
                         atf-c++/detail/application.hpp \
                         atf-c++/detail/auto_array.hpp \
                         atf-c++/detail/env.cpp \
                         atf-c++/detail/env.hpp \
                         atf-c++/detail/exceptions.cpp \
                         atf-c++/detail/exceptions.hpp \
                         atf-c++/detail/expand.cpp \
                         atf-c++/detail/expand.hpp \
                         atf-c++/detail/fs.cpp \
                         atf-c++/detail/fs.hpp \
                         atf-c++/detail/parser.cpp \
                         atf-c++/detail/parser.hpp \
                         atf-c++/detail/process.cpp \
                         atf-c++/detail/process.hpp \
                         atf-c++/detail/sanity.hpp \
                         atf-c++/detail/text.cpp \
                         atf-c++/detail/text.hpp \
                         atf-c++/detail/ui.cpp \
                         atf-c++/detail/ui.hpp

tests_atf_c___detail_DATA = atf-c++/detail/Atffile \
                            atf-c++/detail/Kyuafile
tests_atf_c___detaildir = $(pkgtestsdir)/atf-c++/detail
EXTRA_DIST += $(tests_atf_c___detail_DATA)

noinst_LTLIBRARIES += atf-c++/detail/libtest_helpers.la
atf_c___detail_libtest_helpers_la_SOURCES = atf-c++/detail/test_helpers.cpp \
                                            atf-c++/detail/test_helpers.hpp

tests_atf_c___detail_PROGRAMS = atf-c++/detail/application_test
atf_c___detail_application_test_SOURCES = atf-c++/detail/application_test.cpp
atf_c___detail_application_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/auto_array_test
atf_c___detail_auto_array_test_SOURCES = atf-c++/detail/auto_array_test.cpp
atf_c___detail_auto_array_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/env_test
atf_c___detail_env_test_SOURCES = atf-c++/detail/env_test.cpp
atf_c___detail_env_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/exceptions_test
atf_c___detail_exceptions_test_SOURCES = atf-c++/detail/exceptions_test.cpp
atf_c___detail_exceptions_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/expand_test
atf_c___detail_expand_test_SOURCES = atf-c++/detail/expand_test.cpp
atf_c___detail_expand_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/fs_test
atf_c___detail_fs_test_SOURCES = atf-c++/detail/fs_test.cpp
atf_c___detail_fs_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/parser_test
atf_c___detail_parser_test_SOURCES = atf-c++/detail/parser_test.cpp
atf_c___detail_parser_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/process_test
atf_c___detail_process_test_SOURCES = atf-c++/detail/process_test.cpp
atf_c___detail_process_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/sanity_test
atf_c___detail_sanity_test_SOURCES = atf-c++/detail/sanity_test.cpp
atf_c___detail_sanity_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/text_test
atf_c___detail_text_test_SOURCES = atf-c++/detail/text_test.cpp
atf_c___detail_text_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_c___detail_PROGRAMS += atf-c++/detail/ui_test
atf_c___detail_ui_test_SOURCES = atf-c++/detail/ui_test.cpp
atf_c___detail_ui_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
