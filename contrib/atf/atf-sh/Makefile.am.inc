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

libexec_PROGRAMS += atf-sh/atf-check
atf_sh_atf_check_SOURCES = atf-sh/atf-check.cpp
atf_sh_atf_check_LDADD = $(ATF_CXX_LIBS)
dist_man_MANS += atf-sh/atf-check.1

bin_PROGRAMS += atf-sh/atf-sh
atf_sh_atf_sh_SOURCES = atf-sh/atf-sh.cpp
atf_sh_atf_sh_LDADD = $(ATF_CXX_LIBS)
dist_man_MANS += atf-sh/atf-sh.1

atf_sh_DATA = atf-sh/libatf-sh.subr
atf_shdir = $(pkgdatadir)
EXTRA_DIST += $(atf_sh_DATA)

dist_man_MANS += atf-sh/atf-sh-api.3

atf_aclocal_DATA += atf-sh/atf-sh.m4
EXTRA_DIST += atf-sh/atf-sh.m4

atf_shpkgconfigdir = $(atf_pkgconfigdir)
atf_shpkgconfig_DATA = atf-sh/atf-sh.pc
CLEANFILES += atf-sh/atf-sh.pc
EXTRA_DIST += atf-sh/atf-sh.pc.in
atf-sh/atf-sh.pc: $(srcdir)/atf-sh/atf-sh.pc.in Makefile
	test -d atf-sh || mkdir -p atf-sh
	sed -e 's#__ATF_VERSION__#$(PACKAGE_VERSION)#g' \
	    -e 's#__EXEC_PREFIX__#$(exec_prefix)#g' \
	    <$(srcdir)/atf-sh/atf-sh.pc.in >atf-sh/atf-sh.pc.tmp
	mv atf-sh/atf-sh.pc.tmp atf-sh/atf-sh.pc

tests_atf_sh_DATA = atf-sh/Atffile \
                    atf-sh/Kyuafile
tests_atf_shdir = $(pkgtestsdir)/atf-sh
EXTRA_DIST += $(tests_atf_sh_DATA)

tests_atf_sh_SCRIPTS = atf-sh/misc_helpers
CLEANFILES += atf-sh/misc_helpers
EXTRA_DIST += atf-sh/misc_helpers.sh
atf-sh/misc_helpers: $(srcdir)/atf-sh/misc_helpers.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/misc_helpers.sh"; \
	dst="atf-sh/misc_helpers"; $(BUILD_SH_TP)

tests_atf_sh_SCRIPTS += atf-sh/atf_check_test
CLEANFILES += atf-sh/atf_check_test
EXTRA_DIST += atf-sh/atf_check_test.sh
atf-sh/atf_check_test: $(srcdir)/atf-sh/atf_check_test.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/atf_check_test.sh"; \
	dst="atf-sh/atf_check_test"; $(BUILD_SH_TP)

tests_atf_sh_SCRIPTS += atf-sh/atf-check_test
CLEANFILES += atf-sh/atf-check_test
EXTRA_DIST += atf-sh/atf-check_test.sh
atf-sh/atf-check_test: $(srcdir)/atf-sh/atf-check_test.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/atf-check_test.sh"; \
	dst="atf-sh/atf-check_test"; $(BUILD_SH_TP)

tests_atf_sh_SCRIPTS += atf-sh/config_test
CLEANFILES += atf-sh/config_test
EXTRA_DIST += atf-sh/config_test.sh
atf-sh/config_test: $(srcdir)/atf-sh/config_test.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/config_test.sh"; \
	dst="atf-sh/config_test"; $(BUILD_SH_TP)

tests_atf_sh_SCRIPTS += atf-sh/integration_test
CLEANFILES += atf-sh/integration_test
EXTRA_DIST += atf-sh/integration_test.sh
atf-sh/integration_test: $(srcdir)/atf-sh/integration_test.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/integration_test.sh"; \
	dst="atf-sh/integration_test"; $(BUILD_SH_TP)

tests_atf_sh_SCRIPTS += atf-sh/normalize_test
CLEANFILES += atf-sh/normalize_test
EXTRA_DIST += atf-sh/normalize_test.sh
atf-sh/normalize_test: $(srcdir)/atf-sh/normalize_test.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/normalize_test.sh"; \
	dst="atf-sh/normalize_test"; $(BUILD_SH_TP)

tests_atf_sh_SCRIPTS += atf-sh/tc_test
CLEANFILES += atf-sh/tc_test
EXTRA_DIST += atf-sh/tc_test.sh
atf-sh/tc_test: $(srcdir)/atf-sh/tc_test.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/tc_test.sh"; \
	dst="atf-sh/tc_test"; $(BUILD_SH_TP)

tests_atf_sh_SCRIPTS += atf-sh/tp_test
CLEANFILES += atf-sh/tp_test
EXTRA_DIST += atf-sh/tp_test.sh
atf-sh/tp_test: $(srcdir)/atf-sh/tp_test.sh
	test -d atf-sh || mkdir -p atf-sh
	@src="$(srcdir)/atf-sh/tp_test.sh"; \
	dst="atf-sh/tp_test"; $(BUILD_SH_TP)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
