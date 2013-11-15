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

ATF_CXX_LIBS = libatf-c++.la libatf-c.la

lib_LTLIBRARIES += libatf-c++.la
libatf_c___la_LIBADD = libatf-c.la
libatf_c___la_SOURCES = atf-c++/build.cpp \
                        atf-c++/build.hpp \
                        atf-c++/check.cpp \
                        atf-c++/check.hpp \
                        atf-c++/config.cpp \
                        atf-c++/config.hpp \
                        atf-c++/macros.hpp \
                        atf-c++/noncopyable.hpp \
                        atf-c++/tests.cpp \
                        atf-c++/tests.hpp \
                        atf-c++/utils.cpp \
                        atf-c++/utils.hpp
libatf_c___la_LDFLAGS = -version-info 0:0:0

include_HEADERS += atf-c++.hpp
atf_c___HEADERS = atf-c++/build.hpp \
                  atf-c++/check.hpp \
                  atf-c++/config.hpp \
                  atf-c++/macros.hpp \
                  atf-c++/noncopyable.hpp \
                  atf-c++/tests.hpp \
                  atf-c++/utils.hpp
atf_c__dir = $(includedir)/atf-c++

dist_man_MANS += atf-c++/atf-c++-api.3

atf_aclocal_DATA += atf-c++/atf-c++.m4
EXTRA_DIST += atf-c++/atf-c++.m4

atf_c__dirpkgconfigdir = $(atf_pkgconfigdir)
atf_c__dirpkgconfig_DATA = atf-c++/atf-c++.pc
CLEANFILES += atf-c++/atf-c++.pc
EXTRA_DIST += atf-c++/atf-c++.pc.in
atf-c++/atf-c++.pc: $(srcdir)/atf-c++/atf-c++.pc.in Makefile
	test -d atf-c++ || mkdir -p atf-c++
	sed -e 's#__ATF_VERSION__#$(PACKAGE_VERSION)#g' \
	    -e 's#__CXX__#$(CXX)#g' \
	    -e 's#__INCLUDEDIR__#$(includedir)#g' \
	    -e 's#__LIBDIR__#$(libdir)#g' \
	    <$(srcdir)/atf-c++/atf-c++.pc.in >atf-c++/atf-c++.pc.tmp
	mv atf-c++/atf-c++.pc.tmp atf-c++/atf-c++.pc

tests_atf_c___DATA = atf-c++/Atffile \
                     atf-c++/Kyuafile \
                     atf-c++/macros_hpp_test.cpp \
                     atf-c++/unused_test.cpp
tests_atf_c__dir = $(pkgtestsdir)/atf-c++
EXTRA_DIST += $(tests_atf_c___DATA)

tests_atf_c___PROGRAMS = atf-c++/atf_c++_test
atf_c___atf_c___test_SOURCES = atf-c++/atf_c++_test.cpp
atf_c___atf_c___test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)
tests_atf_c___PROGRAMS += atf-c++/build_test
atf_c___build_test_SOURCES = atf-c++/build_test.cpp atf-c/h_build.h
atf_c___build_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)
tests_atf_c___PROGRAMS += atf-c++/check_test
atf_c___check_test_SOURCES = atf-c++/check_test.cpp
atf_c___check_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)
tests_atf_c___PROGRAMS += atf-c++/config_test
atf_c___config_test_SOURCES = atf-c++/config_test.cpp
atf_c___config_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)
tests_atf_c___PROGRAMS += atf-c++/macros_test
atf_c___macros_test_SOURCES = atf-c++/macros_test.cpp
atf_c___macros_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)
tests_atf_c___SCRIPTS = atf-c++/pkg_config_test
CLEANFILES += atf-c++/pkg_config_test
EXTRA_DIST += atf-c++/pkg_config_test.sh
atf-c++/pkg_config_test: $(srcdir)/atf-c++/pkg_config_test.sh
	test -d atf-c++ || mkdir -p atf-c++
	@src="$(srcdir)/atf-c++/pkg_config_test.sh"; \
	dst="atf-c++/pkg_config_test"; $(BUILD_SH_TP)

tests_atf_c___PROGRAMS += atf-c++/tests_test
atf_c___tests_test_SOURCES = atf-c++/tests_test.cpp
atf_c___tests_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)
tests_atf_c___PROGRAMS += atf-c++/utils_test
atf_c___utils_test_SOURCES = atf-c++/utils_test.cpp
atf_c___utils_test_LDADD = atf-c++/detail/libtest_helpers.la $(ATF_CXX_LIBS)
include atf-c++/detail/Makefile.am.inc

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
