# Copyright 2012 The Kyua Authors.
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

libutils_a_SOURCES += utils/text/exceptions.cpp
libutils_a_SOURCES += utils/text/exceptions.hpp
libutils_a_SOURCES += utils/text/operations.cpp
libutils_a_SOURCES += utils/text/operations.hpp
libutils_a_SOURCES += utils/text/operations.ipp
libutils_a_SOURCES += utils/text/regex.cpp
libutils_a_SOURCES += utils/text/regex.hpp
libutils_a_SOURCES += utils/text/regex_fwd.hpp
libutils_a_SOURCES += utils/text/table.cpp
libutils_a_SOURCES += utils/text/table.hpp
libutils_a_SOURCES += utils/text/table_fwd.hpp
libutils_a_SOURCES += utils/text/templates.cpp
libutils_a_SOURCES += utils/text/templates.hpp
libutils_a_SOURCES += utils/text/templates_fwd.hpp

if WITH_ATF
tests_utils_textdir = $(pkgtestsdir)/utils/text

tests_utils_text_DATA = utils/text/Kyuafile
EXTRA_DIST += $(tests_utils_text_DATA)

tests_utils_text_PROGRAMS = utils/text/exceptions_test
utils_text_exceptions_test_SOURCES = utils/text/exceptions_test.cpp
utils_text_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_text_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_text_PROGRAMS += utils/text/operations_test
utils_text_operations_test_SOURCES = utils/text/operations_test.cpp
utils_text_operations_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_text_operations_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_text_PROGRAMS += utils/text/regex_test
utils_text_regex_test_SOURCES = utils/text/regex_test.cpp
utils_text_regex_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_text_regex_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_text_PROGRAMS += utils/text/table_test
utils_text_table_test_SOURCES = utils/text/table_test.cpp
utils_text_table_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_text_table_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_text_PROGRAMS += utils/text/templates_test
utils_text_templates_test_SOURCES = utils/text/templates_test.cpp
utils_text_templates_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_text_templates_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
