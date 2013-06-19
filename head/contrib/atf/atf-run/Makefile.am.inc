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

bin_PROGRAMS += atf-run/atf-run
atf_run_atf_run_CPPFLAGS = "-DGDB=\"$(GDB)\""
atf_run_atf_run_SOURCES = atf-run/atf-run.cpp \
                          atf-run/atffile.cpp \
                          atf-run/atffile.hpp \
                          atf-run/config.cpp \
                          atf-run/config.hpp \
                          atf-run/fs.cpp \
                          atf-run/fs.hpp \
                          atf-run/io.cpp \
                          atf-run/io.hpp \
                          atf-run/requirements.cpp \
                          atf-run/requirements.hpp \
                          atf-run/signals.cpp \
                          atf-run/signals.hpp \
                          atf-run/test-program.cpp \
                          atf-run/test-program.hpp \
                          atf-run/timer.cpp \
                          atf-run/timer.hpp \
                          atf-run/user.cpp \
                          atf-run/user.hpp
atf_run_atf_run_LDADD = $(ATF_CXX_LIBS)
dist_man_MANS += atf-run/atf-run.1

tests_atf_run_DATA = atf-run/Atffile \
                     atf-run/Kyuafile
tests_atf_rundir = $(pkgtestsdir)/atf-run
EXTRA_DIST += $(tests_atf_run_DATA)

tests_atf_run_PROGRAMS = atf-run/atffile_test
atf_run_atffile_test_SOURCES = atf-run/atffile_test.cpp \
                               atf-run/atffile.cpp
atf_run_atffile_test_CPPFLAGS = -I$(srcdir)/atf-c++/detail
atf_run_atffile_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/bad_metadata_helper
atf_run_bad_metadata_helper_SOURCES = atf-run/bad_metadata_helper.c
atf_run_bad_metadata_helper_LDADD = libatf-c.la

tests_atf_run_PROGRAMS += atf-run/config_test
atf_run_config_test_SOURCES = atf-run/config_test.cpp \
                              atf-run/config.cpp
atf_run_config_test_CPPFLAGS = -I$(srcdir)/atf-c++/detail
atf_run_config_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/expect_helpers
atf_run_expect_helpers_SOURCES = atf-run/expect_helpers.c
atf_run_expect_helpers_LDADD = libatf-c.la

tests_atf_run_PROGRAMS += atf-run/fs_test
atf_run_fs_test_SOURCES = atf-run/fs_test.cpp \
                          atf-run/fs.cpp \
                          atf-run/user.cpp
atf_run_fs_test_LDADD = $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/io_test
atf_run_io_test_SOURCES = atf-run/io_test.cpp \
                          atf-run/io.cpp \
                          atf-run/signals.cpp
atf_run_io_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/misc_helpers
atf_run_misc_helpers_SOURCES = atf-run/misc_helpers.cpp
atf_run_misc_helpers_LDADD = $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/pass_helper
atf_run_pass_helper_SOURCES = atf-run/pass_helper.cpp
atf_run_pass_helper_LDADD = $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/several_tcs_helper
atf_run_several_tcs_helper_SOURCES = atf-run/several_tcs_helper.c
atf_run_several_tcs_helper_LDADD = libatf-c.la

tests_atf_run_PROGRAMS += atf-run/requirements_test
atf_run_requirements_test_SOURCES = atf-run/requirements_test.cpp \
                                    atf-run/requirements.cpp \
                                    atf-run/user.cpp
atf_run_requirements_test_LDADD = $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/signals_test
atf_run_signals_test_SOURCES = atf-run/signals_test.cpp atf-run/signals.cpp
atf_run_signals_test_LDADD = $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/test_program_test
atf_run_test_program_test_SOURCES = atf-run/test_program_test.cpp \
                                    atf-run/fs.cpp \
                                    atf-run/io.cpp \
                                    atf-run/requirements.cpp \
                                    atf-run/signals.cpp \
                                    atf-run/test-program.cpp \
                                    atf-run/timer.cpp \
                                    atf-run/user.cpp
atf_run_test_program_test_CPPFLAGS = -I$(srcdir)/atf-c++/detail
atf_run_test_program_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/user_test
atf_run_user_test_SOURCES = atf-run/user_test.cpp atf-run/user.cpp
atf_run_user_test_LDADD = $(ATF_CXX_LIBS)

tests_atf_run_PROGRAMS += atf-run/zero_tcs_helper
atf_run_zero_tcs_helper_SOURCES = atf-run/zero_tcs_helper.c
atf_run_zero_tcs_helper_LDADD = libatf-c.la

tests_atf_run_SCRIPTS = atf-run/integration_test
CLEANFILES += atf-run/integration_test
EXTRA_DIST += atf-run/integration_test.sh
atf-run/integration_test: $(srcdir)/atf-run/integration_test.sh
	test -d atf-run || mkdir -p atf-run
	@src="$(srcdir)/atf-run/integration_test.sh"; \
	dst="atf-run/integration_test"; $(BUILD_SH_TP)

hooksdir = $(pkgdatadir)
hooks_DATA = atf-run/share/atf-run.hooks
EXTRA_DIST += $(hooks_DATA)

egdir = $(atf_egdir)
eg_DATA = atf-run/sample/atf-run.hooks
eg_DATA += atf-run/sample/common.conf
EXTRA_DIST += $(eg_DATA)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
