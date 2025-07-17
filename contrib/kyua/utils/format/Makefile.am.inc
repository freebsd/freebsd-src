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

libutils_a_SOURCES += utils/format/containers.hpp
libutils_a_SOURCES += utils/format/containers.ipp
libutils_a_SOURCES += utils/format/exceptions.cpp
libutils_a_SOURCES += utils/format/exceptions.hpp
libutils_a_SOURCES += utils/format/formatter.cpp
libutils_a_SOURCES += utils/format/formatter.hpp
libutils_a_SOURCES += utils/format/formatter_fwd.hpp
libutils_a_SOURCES += utils/format/formatter.ipp
libutils_a_SOURCES += utils/format/macros.hpp

if WITH_ATF
tests_utils_formatdir = $(pkgtestsdir)/utils/format

tests_utils_format_DATA = utils/format/Kyuafile
EXTRA_DIST += $(tests_utils_format_DATA)

tests_utils_format_PROGRAMS = utils/format/containers_test
utils_format_containers_test_SOURCES = utils/format/containers_test.cpp
utils_format_containers_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_format_containers_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_format_PROGRAMS += utils/format/exceptions_test
utils_format_exceptions_test_SOURCES = utils/format/exceptions_test.cpp
utils_format_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_format_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_format_PROGRAMS += utils/format/formatter_test
utils_format_formatter_test_SOURCES = utils/format/formatter_test.cpp
utils_format_formatter_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_format_formatter_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
