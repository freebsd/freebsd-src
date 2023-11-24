# Copyright 2010 The Kyua Authors.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

UTILS_CFLAGS =
UTILS_LIBS = libutils.a

noinst_LIBRARIES += libutils.a
libutils_a_CPPFLAGS = -DGDB=\"$(GDB)\"
libutils_a_SOURCES  = utils/auto_array.hpp
libutils_a_SOURCES += utils/auto_array.ipp
libutils_a_SOURCES += utils/auto_array_fwd.hpp
libutils_a_SOURCES += utils/datetime.cpp
libutils_a_SOURCES += utils/datetime.hpp
libutils_a_SOURCES += utils/datetime_fwd.hpp
libutils_a_SOURCES += utils/env.hpp
libutils_a_SOURCES += utils/env.cpp
libutils_a_SOURCES += utils/memory.hpp
libutils_a_SOURCES += utils/memory.cpp
libutils_a_SOURCES += utils/noncopyable.hpp
libutils_a_SOURCES += utils/optional.hpp
libutils_a_SOURCES += utils/optional_fwd.hpp
libutils_a_SOURCES += utils/optional.ipp
libutils_a_SOURCES += utils/passwd.cpp
libutils_a_SOURCES += utils/passwd.hpp
libutils_a_SOURCES += utils/passwd_fwd.hpp
libutils_a_SOURCES += utils/sanity.cpp
libutils_a_SOURCES += utils/sanity.hpp
libutils_a_SOURCES += utils/sanity_fwd.hpp
libutils_a_SOURCES += utils/stacktrace.cpp
libutils_a_SOURCES += utils/stacktrace.hpp
libutils_a_SOURCES += utils/stream.cpp
libutils_a_SOURCES += utils/stream.hpp
libutils_a_SOURCES += utils/units.cpp
libutils_a_SOURCES += utils/units.hpp
libutils_a_SOURCES += utils/units_fwd.hpp
nodist_libutils_a_SOURCES = utils/defs.hpp

EXTRA_DIST += utils/test_utils.ipp

if WITH_ATF
tests_utilsdir = $(pkgtestsdir)/utils

tests_utils_DATA = utils/Kyuafile
EXTRA_DIST += $(tests_utils_DATA)

tests_utils_PROGRAMS = utils/auto_array_test
utils_auto_array_test_SOURCES = utils/auto_array_test.cpp
utils_auto_array_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_auto_array_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/datetime_test
utils_datetime_test_SOURCES = utils/datetime_test.cpp
utils_datetime_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_datetime_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/env_test
utils_env_test_SOURCES = utils/env_test.cpp
utils_env_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_env_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/memory_test
utils_memory_test_SOURCES = utils/memory_test.cpp
utils_memory_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_memory_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/optional_test
utils_optional_test_SOURCES = utils/optional_test.cpp
utils_optional_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_optional_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/passwd_test
utils_passwd_test_SOURCES = utils/passwd_test.cpp
utils_passwd_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_passwd_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/sanity_test
utils_sanity_test_SOURCES = utils/sanity_test.cpp
utils_sanity_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_sanity_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/stacktrace_helper
utils_stacktrace_helper_SOURCES = utils/stacktrace_helper.cpp

tests_utils_PROGRAMS += utils/stacktrace_test
utils_stacktrace_test_SOURCES = utils/stacktrace_test.cpp
utils_stacktrace_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_stacktrace_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/stream_test
utils_stream_test_SOURCES = utils/stream_test.cpp
utils_stream_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_stream_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/units_test
utils_units_test_SOURCES = utils/units_test.cpp
utils_units_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_units_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif

include utils/cmdline/Makefile.am.inc
include utils/config/Makefile.am.inc
include utils/format/Makefile.am.inc
include utils/fs/Makefile.am.inc
include utils/logging/Makefile.am.inc
include utils/process/Makefile.am.inc
include utils/signals/Makefile.am.inc
include utils/sqlite/Makefile.am.inc
include utils/text/Makefile.am.inc
