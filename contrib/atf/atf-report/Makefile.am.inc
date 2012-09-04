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

bin_PROGRAMS += atf-report/atf-report
atf_report_atf_report_SOURCES = atf-report/atf-report.cpp \
                                atf-report/reader.cpp \
                                atf-report/reader.hpp
atf_report_atf_report_LDADD = $(ATF_CXX_LIBS)
dist_man_MANS += atf-report/atf-report.1

cssdir = $(atf_cssdir)
css_DATA = atf-report/tests-results.css
EXTRA_DIST += $(css_DATA)

dtddir = $(atf_dtddir)
dtd_DATA = atf-report/tests-results.dtd
EXTRA_DIST += $(dtd_DATA)

xsldir = $(atf_xsldir)
xsl_DATA = atf-report/tests-results.xsl
EXTRA_DIST += $(xsl_DATA)

tests_atf_report_DATA = atf-report/Atffile \
                        atf-report/Kyuafile
tests_atf_reportdir = $(pkgtestsdir)/atf-report
EXTRA_DIST += $(tests_atf_report_DATA)

tests_atf_report_PROGRAMS = atf-report/fail_helper
atf_report_fail_helper_SOURCES = atf-report/fail_helper.cpp
atf_report_fail_helper_LDADD = $(ATF_CXX_LIBS)

tests_atf_report_PROGRAMS += atf-report/misc_helpers
atf_report_misc_helpers_SOURCES = atf-report/misc_helpers.cpp
atf_report_misc_helpers_LDADD = $(ATF_CXX_LIBS)

tests_atf_report_PROGRAMS += atf-report/pass_helper
atf_report_pass_helper_SOURCES = atf-report/pass_helper.cpp
atf_report_pass_helper_LDADD = $(ATF_CXX_LIBS)

tests_atf_report_SCRIPTS = atf-report/integration_test
CLEANFILES += atf-report/integration_test
EXTRA_DIST += atf-report/integration_test.sh
atf-report/integration_test: $(srcdir)/atf-report/integration_test.sh
	test -d atf-report || mkdir -p atf-report
	@src="$(srcdir)/atf-report/integration_test.sh"; \
	dst="atf-report/integration_test"; $(BUILD_SH_TP)

tests_atf_report_PROGRAMS += atf-report/reader_test
atf_report_reader_test_SOURCES = atf-report/reader_test.cpp \
                                 atf-report/reader.cpp
atf_report_reader_test_CPPFLAGS = -I$(srcdir)/atf-c++/detail
atf_report_reader_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
