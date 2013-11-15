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

check_PROGRAMS = bootstrap/h_app_empty
bootstrap_h_app_empty_SOURCES = bootstrap/h_app_empty.cpp
bootstrap_h_app_empty_LDADD = $(ATF_CXX_LIBS)

check_PROGRAMS += bootstrap/h_app_opts_args
bootstrap_h_app_opts_args_SOURCES = bootstrap/h_app_opts_args.cpp
bootstrap_h_app_opts_args_LDADD = $(ATF_CXX_LIBS)

check_PROGRAMS += bootstrap/h_tp_basic_c
bootstrap_h_tp_basic_c_SOURCES = bootstrap/h_tp_basic_c.c
bootstrap_h_tp_basic_c_LDADD = libatf-c.la

check_PROGRAMS += bootstrap/h_tp_basic_cpp
bootstrap_h_tp_basic_cpp_SOURCES = bootstrap/h_tp_basic_cpp.cpp
bootstrap_h_tp_basic_cpp_LDADD = $(ATF_CXX_LIBS)

check_SCRIPTS = bootstrap/h_tp_basic_sh
CLEANFILES += bootstrap/h_tp_basic_sh
EXTRA_DIST += bootstrap/h_tp_basic_sh.sh
bootstrap/h_tp_basic_sh: $(srcdir)/bootstrap/h_tp_basic_sh.sh
	test -d bootstrap || mkdir -p bootstrap
	@src=$(srcdir)/bootstrap/h_tp_basic_sh.sh; dst=$@; $(BUILD_SH_TP)

check_SCRIPTS += bootstrap/h_tp_atf_check_sh
CLEANFILES += bootstrap/h_tp_atf_check_sh
EXTRA_DIST += bootstrap/h_tp_atf_check_sh.sh
bootstrap/h_tp_atf_check_sh: \
		$(srcdir)/bootstrap/h_tp_atf_check_sh.sh
	test -d bootstrap || mkdir -p bootstrap
	@src=$(srcdir)/bootstrap/h_tp_atf_check_sh.sh; dst=$@; $(BUILD_SH_TP)

check_SCRIPTS += bootstrap/h_tp_fail
CLEANFILES += bootstrap/h_tp_fail
EXTRA_DIST += bootstrap/h_tp_fail.sh
bootstrap/h_tp_fail: $(srcdir)/bootstrap/h_tp_fail.sh
	test -d bootstrap || mkdir -p bootstrap
	@src=$(srcdir)/bootstrap/h_tp_fail.sh; dst=$@; $(BUILD_SH_TP)

check_SCRIPTS += bootstrap/h_tp_pass
CLEANFILES += bootstrap/h_tp_pass
EXTRA_DIST += bootstrap/h_tp_pass.sh
bootstrap/h_tp_pass: $(srcdir)/bootstrap/h_tp_pass.sh
	test -d bootstrap || mkdir -p bootstrap
	@src=$(srcdir)/bootstrap/h_tp_pass.sh; dst=$@; $(BUILD_SH_TP)

DISTCLEANFILES = \
		bootstrap/atconfig \
		testsuite.lineno \
		testsuite.log

distclean-local:
	-rm -rf testsuite.dir

EXTRA_DIST +=	bootstrap/testsuite \
		bootstrap/package.m4 \
		bootstrap/testsuite.at \
		$(testsuite_incs)

testsuite_incs=	$(srcdir)/bootstrap/t_application_help.at \
		$(srcdir)/bootstrap/t_application_opts_args.at \
		$(srcdir)/bootstrap/t_atf_config.at \
		$(srcdir)/bootstrap/t_atf_run.at \
		$(srcdir)/bootstrap/t_subr_atf_check.at \
		$(srcdir)/bootstrap/t_test_program_compare.at \
		$(srcdir)/bootstrap/t_test_program_filter.at \
		$(srcdir)/bootstrap/t_test_program_list.at \
		$(srcdir)/bootstrap/t_test_program_run.at

@target_srcdir@bootstrap/package.m4: $(top_srcdir)/configure.ac
	{ \
	echo '# Signature of the current package.'; \
	echo 'm4_define(AT_PACKAGE_NAME,      @PACKAGE_NAME@)'; \
	echo 'm4_define(AT_PACKAGE_TARNAME,   @PACKAGE_TARNAME@)'; \
	echo 'm4_define(AT_PACKAGE_VERSION,   @PACKAGE_VERSION@)'; \
	echo 'm4_define(AT_PACKAGE_STRING,    @PACKAGE_STRING@)'; \
	echo 'm4_define(AT_PACKAGE_BUGREPORT, @PACKAGE_BUGREPORT@)'; \
	echo 'm4_define(ENABLE_TOOLS,         @ENABLE_TOOLS@)'; \
	} >$(srcdir)/bootstrap/package.m4

@target_srcdir@bootstrap/testsuite: $(srcdir)/bootstrap/testsuite.at \
                                    $(testsuite_incs) \
                                    @target_srcdir@bootstrap/package.m4
	autom4te --language=Autotest -I $(srcdir) \
	    -I $(srcdir)/bootstrap \
	    $(srcdir)/bootstrap/testsuite.at -o $@.tmp
	mv $@.tmp $@

INSTALLCHECK_TARGETS += installcheck-bootstrap
PHONY_TARGETS += installcheck-bootstrap
installcheck-bootstrap: @target_srcdir@bootstrap/testsuite check
	$(TESTS_ENVIRONMENT) $(srcdir)/bootstrap/testsuite

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
