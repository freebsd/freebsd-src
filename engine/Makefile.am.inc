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

ENGINE_CFLAGS = $(STORE_CFLAGS) $(MODEL_CFLAGS) $(UTILS_CFLAGS)
ENGINE_LIBS = libengine.a $(STORE_LIBS) $(MODEL_LIBS) $(UTILS_LIBS)

noinst_LIBRARIES += libengine.a
libengine_a_CPPFLAGS  = $(STORE_CFLAGS) $(UTILS_CFLAGS)
libengine_a_SOURCES  = engine/atf.cpp
libengine_a_SOURCES += engine/atf.hpp
libengine_a_SOURCES += engine/atf_list.cpp
libengine_a_SOURCES += engine/atf_list.hpp
libengine_a_SOURCES += engine/atf_result.cpp
libengine_a_SOURCES += engine/atf_result.hpp
libengine_a_SOURCES += engine/atf_result_fwd.hpp
libengine_a_SOURCES += engine/config.cpp
libengine_a_SOURCES += engine/config.hpp
libengine_a_SOURCES += engine/config_fwd.hpp
libengine_a_SOURCES += engine/exceptions.cpp
libengine_a_SOURCES += engine/exceptions.hpp
libengine_a_SOURCES += engine/filters.cpp
libengine_a_SOURCES += engine/filters.hpp
libengine_a_SOURCES += engine/filters_fwd.hpp
libengine_a_SOURCES += engine/kyuafile.cpp
libengine_a_SOURCES += engine/kyuafile.hpp
libengine_a_SOURCES += engine/kyuafile_fwd.hpp
libengine_a_SOURCES += engine/plain.cpp
libengine_a_SOURCES += engine/plain.hpp
libengine_a_SOURCES += engine/requirements.cpp
libengine_a_SOURCES += engine/requirements.hpp
libengine_a_SOURCES += engine/scanner.cpp
libengine_a_SOURCES += engine/scanner.hpp
libengine_a_SOURCES += engine/scanner_fwd.hpp
libengine_a_SOURCES += engine/tap.cpp
libengine_a_SOURCES += engine/tap.hpp
libengine_a_SOURCES += engine/tap_parser.cpp
libengine_a_SOURCES += engine/tap_parser.hpp
libengine_a_SOURCES += engine/tap_parser_fwd.hpp
libengine_a_SOURCES += engine/scheduler.cpp
libengine_a_SOURCES += engine/scheduler.hpp
libengine_a_SOURCES += engine/scheduler_fwd.hpp

if WITH_ATF
tests_enginedir = $(pkgtestsdir)/engine

tests_engine_DATA = engine/Kyuafile
EXTRA_DIST += $(tests_engine_DATA)

tests_engine_PROGRAMS = engine/atf_helpers
engine_atf_helpers_SOURCES = engine/atf_helpers.cpp
engine_atf_helpers_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_helpers_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/atf_test
engine_atf_test_SOURCES = engine/atf_test.cpp
engine_atf_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/atf_list_test
engine_atf_list_test_SOURCES = engine/atf_list_test.cpp
engine_atf_list_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_list_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/atf_result_test
engine_atf_result_test_SOURCES = engine/atf_result_test.cpp
engine_atf_result_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_result_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/config_test
engine_config_test_SOURCES = engine/config_test.cpp
engine_config_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_config_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/exceptions_test
engine_exceptions_test_SOURCES = engine/exceptions_test.cpp
engine_exceptions_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_exceptions_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/filters_test
engine_filters_test_SOURCES = engine/filters_test.cpp
engine_filters_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_filters_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/kyuafile_test
engine_kyuafile_test_SOURCES = engine/kyuafile_test.cpp
engine_kyuafile_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_kyuafile_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/plain_helpers
engine_plain_helpers_SOURCES = engine/plain_helpers.cpp
engine_plain_helpers_CXXFLAGS = $(UTILS_CFLAGS)
engine_plain_helpers_LDADD = $(UTILS_LIBS)

tests_engine_PROGRAMS += engine/plain_test
engine_plain_test_SOURCES = engine/plain_test.cpp
engine_plain_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_plain_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/requirements_test
engine_requirements_test_SOURCES = engine/requirements_test.cpp
engine_requirements_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                                    $(ATF_CXX_CFLAGS)
engine_requirements_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                                 $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/scanner_test
engine_scanner_test_SOURCES = engine/scanner_test.cpp
engine_scanner_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_scanner_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/tap_helpers
engine_tap_helpers_SOURCES = engine/tap_helpers.cpp
engine_tap_helpers_CXXFLAGS = $(UTILS_CFLAGS)
engine_tap_helpers_LDADD = $(UTILS_LIBS)

tests_engine_PROGRAMS += engine/tap_test
engine_tap_test_SOURCES = engine/tap_test.cpp
engine_tap_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_tap_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/tap_parser_test
engine_tap_parser_test_SOURCES = engine/tap_parser_test.cpp
engine_tap_parser_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_tap_parser_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/scheduler_test
engine_scheduler_test_SOURCES = engine/scheduler_test.cpp
engine_scheduler_test_CXXFLAGS = $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
engine_scheduler_test_LDADD = $(ENGINE_LIBS) $(ATF_CXX_LIBS)
endif
