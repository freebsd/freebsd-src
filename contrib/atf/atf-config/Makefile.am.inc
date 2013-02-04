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

bin_PROGRAMS += atf-config/atf-config
atf_config_atf_config_SOURCES = atf-config/atf-config.cpp
atf_config_atf_config_LDADD = $(ATF_CXX_LIBS)
dist_man_MANS += atf-config/atf-config.1

tests_atf_config_DATA = atf-config/Atffile \
                        atf-config/Kyuafile
tests_atf_configdir = $(pkgtestsdir)/atf-config
EXTRA_DIST += $(tests_atf_config_DATA)

tests_atf_config_SCRIPTS = atf-config/integration_test
CLEANFILES += atf-config/integration_test
EXTRA_DIST += atf-config/integration_test.sh
atf-config/integration_test: $(srcdir)/atf-config/integration_test.sh
	test -d atf-config || mkdir -p atf-config
	@src="$(srcdir)/atf-config/integration_test.sh"; \
	dst="atf-config/integration_test"; $(BUILD_SH_TP)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
