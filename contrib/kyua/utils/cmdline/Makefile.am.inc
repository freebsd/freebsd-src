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

libutils_a_SOURCES += utils/cmdline/base_command.cpp
libutils_a_SOURCES += utils/cmdline/base_command.hpp
libutils_a_SOURCES += utils/cmdline/base_command_fwd.hpp
libutils_a_SOURCES += utils/cmdline/base_command.ipp
libutils_a_SOURCES += utils/cmdline/commands_map.hpp
libutils_a_SOURCES += utils/cmdline/commands_map_fwd.hpp
libutils_a_SOURCES += utils/cmdline/commands_map.ipp
libutils_a_SOURCES += utils/cmdline/exceptions.cpp
libutils_a_SOURCES += utils/cmdline/exceptions.hpp
libutils_a_SOURCES += utils/cmdline/globals.cpp
libutils_a_SOURCES += utils/cmdline/globals.hpp
libutils_a_SOURCES += utils/cmdline/options.cpp
libutils_a_SOURCES += utils/cmdline/options.hpp
libutils_a_SOURCES += utils/cmdline/options_fwd.hpp
libutils_a_SOURCES += utils/cmdline/parser.cpp
libutils_a_SOURCES += utils/cmdline/parser.hpp
libutils_a_SOURCES += utils/cmdline/parser_fwd.hpp
libutils_a_SOURCES += utils/cmdline/parser.ipp
libutils_a_SOURCES += utils/cmdline/ui.cpp
libutils_a_SOURCES += utils/cmdline/ui.hpp
libutils_a_SOURCES += utils/cmdline/ui_fwd.hpp
# The following two files are only supposed to be used from test code.  They
# should not be bundled into libutils.a, but doing so simplifies the build
# significantly.
libutils_a_SOURCES += utils/cmdline/ui_mock.hpp
libutils_a_SOURCES += utils/cmdline/ui_mock.cpp

if WITH_ATF
tests_utils_cmdlinedir = $(pkgtestsdir)/utils/cmdline

tests_utils_cmdline_DATA = utils/cmdline/Kyuafile
EXTRA_DIST += $(tests_utils_cmdline_DATA)

tests_utils_cmdline_PROGRAMS = utils/cmdline/base_command_test
utils_cmdline_base_command_test_SOURCES = utils/cmdline/base_command_test.cpp
utils_cmdline_base_command_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_cmdline_base_command_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_cmdline_PROGRAMS += utils/cmdline/commands_map_test
utils_cmdline_commands_map_test_SOURCES = utils/cmdline/commands_map_test.cpp
utils_cmdline_commands_map_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_cmdline_commands_map_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_cmdline_PROGRAMS += utils/cmdline/exceptions_test
utils_cmdline_exceptions_test_SOURCES = utils/cmdline/exceptions_test.cpp
utils_cmdline_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_cmdline_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_cmdline_PROGRAMS += utils/cmdline/globals_test
utils_cmdline_globals_test_SOURCES = utils/cmdline/globals_test.cpp
utils_cmdline_globals_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_cmdline_globals_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_cmdline_PROGRAMS += utils/cmdline/options_test
utils_cmdline_options_test_SOURCES = utils/cmdline/options_test.cpp
utils_cmdline_options_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_cmdline_options_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_cmdline_PROGRAMS += utils/cmdline/parser_test
utils_cmdline_parser_test_SOURCES = utils/cmdline/parser_test.cpp
utils_cmdline_parser_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_cmdline_parser_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_cmdline_PROGRAMS += utils/cmdline/ui_test
utils_cmdline_ui_test_SOURCES = utils/cmdline/ui_test.cpp
utils_cmdline_ui_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_cmdline_ui_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
