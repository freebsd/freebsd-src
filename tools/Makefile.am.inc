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

TOOLS_CPPFLAGS = -I$(top_srcdir)/tools -I$(top_builddir)/tools
TOOLS_LDADD = tools/libtools.a

noinst_LIBRARIES = tools/libtools.a
tools_libtools_a_SOURCES = tools/application.cpp \
                           tools/application.hpp \
                           tools/atffile.cpp \
                           tools/atffile.hpp \
                           tools/auto_array.hpp \
                           tools/config.cpp \
                           tools/config.hpp \
                           tools/config_file.cpp \
                           tools/config_file.hpp \
                           tools/env.cpp \
                           tools/env.hpp \
                           tools/exceptions.cpp \
                           tools/exceptions.hpp \
                           tools/expand.cpp \
                           tools/expand.hpp \
                           tools/fs.cpp \
                           tools/fs.hpp \
                           tools/io.cpp \
                           tools/io.hpp \
                           tools/parser.cpp \
                           tools/parser.hpp \
                           tools/process.cpp \
                           tools/process.hpp \
                           tools/reader.cpp \
                           tools/reader.hpp \
                           tools/requirements.cpp \
                           tools/requirements.hpp \
                           tools/signals.cpp \
                           tools/signals.hpp \
                           tools/test-program.cpp \
                           tools/test-program.hpp \
                           tools/test_helpers.hpp \
                           tools/text.cpp \
                           tools/text.hpp \
                           tools/timers.cpp \
                           tools/timers.hpp \
                           tools/ui.cpp \
                           tools/ui.hpp \
                           tools/user.cpp \
                           tools/user.hpp
nodist_tools_libtools_a_SOURCES = tools/defs.hpp
tools_libtools_a_CPPFLAGS = "-DATF_ARCH=\"$(atf_arch)\"" \
                            "-DATF_BUILD_CC=\"$(ATF_BUILD_CC)\"" \
                            "-DATF_BUILD_CFLAGS=\"$(ATF_BUILD_CFLAGS)\"" \
                            "-DATF_BUILD_CPP=\"$(ATF_BUILD_CPP)\"" \
                            "-DATF_BUILD_CPPFLAGS=\"$(ATF_BUILD_CPPFLAGS)\"" \
                            "-DATF_BUILD_CXX=\"$(ATF_BUILD_CXX)\"" \
                            "-DATF_BUILD_CXXFLAGS=\"$(ATF_BUILD_CXXFLAGS)\"" \
                            "-DATF_CONFDIR=\"$(atf_confdir)\"" \
                            "-DATF_INCLUDEDIR=\"$(includedir)\"" \
                            "-DATF_LIBDIR=\"$(libdir)\"" \
                            "-DATF_LIBEXECDIR=\"$(libexecdir)\"" \
                            "-DATF_MACHINE=\"$(atf_machine)\"" \
                            "-DATF_PKGDATADIR=\"$(pkgdatadir)\"" \
                            "-DATF_SHELL=\"$(ATF_SHELL)\"" \
                            "-DATF_WORKDIR=\"$(ATF_WORKDIR)\"" \
                            $(TOOLS_CPPFLAGS)

# XXX For some reason, the nodist line above does not work as expected.
# Work this problem around.
DIST_HOOKS += kill-defs-hpp
kill-defs-hpp:
	rm -f $(distdir)/tools/defs.hpp



bin_PROGRAMS += tools/atf-config
tools_atf_config_SOURCES = tools/atf-config.cpp
tools_atf_config_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_atf_config_LDADD = $(TOOLS_LDADD)
dist_man_MANS += tools/atf-config.1

bin_PROGRAMS += tools/atf-report
tools_atf_report_SOURCES = tools/atf-report.cpp
tools_atf_report_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_atf_report_LDADD = $(TOOLS_LDADD)
dist_man_MANS += tools/atf-report.1

bin_PROGRAMS += tools/atf-run
tools_atf_run_CPPFLAGS = $(TOOLS_CPPFLAGS) "-DGDB=\"$(GDB)\""
tools_atf_run_SOURCES = tools/atf-run.cpp
tools_atf_run_LDADD = $(TOOLS_LDADD)
dist_man_MANS += tools/atf-run.1

bin_PROGRAMS += tools/atf-version
tools_atf_version_SOURCES = tools/atf-version.cpp
nodist_tools_atf_version_SOURCES = tools/revision.h
tools_atf_version_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_atf_version_LDADD = $(TOOLS_LDADD)
dist_man_MANS += tools/atf-version.1

EXTRA_DIST += tools/generate-revision.sh

BUILT_SOURCES += tools/revision.h
CLEANFILES += tools/revision.h
tools/revision.h: tools/revision.h.stamp
	@test -d tools || mkdir -p tools
	@cmp -s tools/revision.h tools/revision.h.stamp || \
	    cp -p tools/revision.h.stamp tools/revision.h

CLEANFILES += tools/revision.h.stamp
PHONY_TARGETS += tools/revision.h.stamp
tools/revision.h.stamp:
	@test -d tools || mkdir -p tools
	@$(top_srcdir)/tools/generate-revision.sh \
	    -g "$(GIT)" -r $(top_srcdir) -o tools/revision.h.stamp \
	    -v $(PACKAGE_VERSION)



man_MANS += tools/atf.7
CLEANFILES += tools/atf.7
EXTRA_DIST += tools/atf.7.in

dist_man_MANS += tools/atf-formats.5

tools/atf.7: $(srcdir)/tools/atf.7.in
	test -d tools || mkdir -p tools
	sed -e 's#__DOCDIR__#$(docdir)#g' \
	    -e 's#__TESTSDIR__#$(testsdir)#g' \
	    <$(srcdir)/tools/atf.7.in >tools/atf.7.tmp
	mv tools/atf.7.tmp tools/atf.7



cssdir = $(atf_cssdir)
css_DATA = tools/tests-results.css
EXTRA_DIST += $(css_DATA)

dtddir = $(atf_dtddir)
dtd_DATA = tools/tests-results.dtd
EXTRA_DIST += $(dtd_DATA)

egdir = $(atf_egdir)
eg_DATA = tools/sample/atf-run.hooks
eg_DATA += tools/sample/common.conf
EXTRA_DIST += $(eg_DATA)

hooksdir = $(pkgdatadir)
hooks_DATA = tools/share/atf-run.hooks
EXTRA_DIST += $(hooks_DATA)

xsldir = $(atf_xsldir)
xsl_DATA = tools/tests-results.xsl
EXTRA_DIST += $(xsl_DATA)



tests_tools_DATA = tools/Atffile tools/Kyuafile
tests_toolsdir = $(pkgtestsdir)/tools
EXTRA_DIST += $(tests_tools_DATA)

tests_tools_SCRIPTS = tools/atf-config_test
CLEANFILES += tools/atf-config_test
EXTRA_DIST += tools/atf-config_test.sh
tools/atf-config_test: $(srcdir)/tools/atf-config_test.sh
	@test -d tools || mkdir -p tools
	@src="$(srcdir)/tools/atf-config_test.sh"; \
	dst="tools/atf-config_test"; $(BUILD_SH_TP)

tests_tools_SCRIPTS += tools/atf-report_test
CLEANFILES += tools/atf-report_test
EXTRA_DIST += tools/atf-report_test.sh
tools/atf-report_test: $(srcdir)/tools/atf-report_test.sh
	@test -d tools || mkdir -p tools
	@src="$(srcdir)/tools/atf-report_test.sh"; \
	dst="tools/atf-report_test"; $(BUILD_SH_TP)

tests_tools_SCRIPTS += tools/atf-run_test
CLEANFILES += tools/atf-run_test
EXTRA_DIST += tools/atf-run_test.sh
tools/atf-run_test: $(srcdir)/tools/atf-run_test.sh
	@test -d tools || mkdir -p tools
	@src="$(srcdir)/tools/atf-run_test.sh"; \
	dst="tools/atf-run_test"; $(BUILD_SH_TP)

tests_tools_PROGRAMS = tools/application_test
tools_application_test_SOURCES = tools/application_test.cpp
tools_application_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_application_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/atffile_test
tools_atffile_test_SOURCES = tools/atffile_test.cpp
tools_atffile_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_atffile_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/auto_array_test
tools_auto_array_test_SOURCES = tools/auto_array_test.cpp
tools_auto_array_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_auto_array_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/bad_metadata_helper
tools_bad_metadata_helper_SOURCES = tools/bad_metadata_helper.c
tools_bad_metadata_helper_LDADD = libatf-c.la

tests_tools_PROGRAMS += tools/config_test
tools_config_test_SOURCES = tools/config_test.cpp
tools_config_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_config_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/config_file_test
tools_config_file_test_SOURCES = tools/config_file_test.cpp
tools_config_file_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_config_file_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/env_test
tools_env_test_SOURCES = tools/env_test.cpp
tools_env_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_env_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/expand_test
tools_expand_test_SOURCES = tools/expand_test.cpp
tools_expand_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_expand_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/expect_helpers
tools_expect_helpers_SOURCES = tools/expect_helpers.c
tools_expect_helpers_LDADD = libatf-c.la

tests_tools_PROGRAMS += tools/fail_helper
tools_fail_helper_SOURCES = tools/fail_helper.cpp
tools_fail_helper_LDADD = $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/fs_test
tools_fs_test_SOURCES = tools/fs_test.cpp
tools_fs_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_fs_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/io_test
tools_io_test_SOURCES = tools/io_test.cpp
tools_io_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_io_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/misc_helpers
tools_misc_helpers_SOURCES = tools/misc_helpers.cpp
tools_misc_helpers_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_misc_helpers_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/parser_test
tools_parser_test_SOURCES = tools/parser_test.cpp
tools_parser_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_parser_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/process_test
tools_process_test_SOURCES = tools/process_test.cpp
tools_process_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_process_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/pass_helper
tools_pass_helper_SOURCES = tools/pass_helper.cpp
tools_pass_helper_LDADD = $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/process_helpers
tools_process_helpers_SOURCES = tools/process_helpers.c

tests_tools_PROGRAMS += tools/reader_test
tools_reader_test_SOURCES = tools/reader_test.cpp
tools_reader_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_reader_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/requirements_test
tools_requirements_test_SOURCES = tools/requirements_test.cpp
tools_requirements_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_requirements_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/several_tcs_helper
tools_several_tcs_helper_SOURCES = tools/several_tcs_helper.c
tools_several_tcs_helper_LDADD = libatf-c.la

tests_tools_PROGRAMS += tools/signals_test
tools_signals_test_SOURCES = tools/signals_test.cpp tools/signals.cpp
tools_signals_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_signals_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/test_program_test
tools_test_program_test_SOURCES = tools/test_program_test.cpp
tools_test_program_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_test_program_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/text_test
tools_text_test_SOURCES = tools/text_test.cpp tools/text.cpp
tools_text_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_text_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/ui_test
tools_ui_test_SOURCES = tools/ui_test.cpp tools/ui.cpp
tools_ui_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_ui_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/user_test
tools_user_test_SOURCES = tools/user_test.cpp tools/user.cpp
tools_user_test_CPPFLAGS = $(TOOLS_CPPFLAGS)
tools_user_test_LDADD = $(TOOLS_LDADD) $(ATF_CXX_LIBS)

tests_tools_PROGRAMS += tools/zero_tcs_helper
tools_zero_tcs_helper_SOURCES = tools/zero_tcs_helper.c
tools_zero_tcs_helper_LDADD = libatf-c.la

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
