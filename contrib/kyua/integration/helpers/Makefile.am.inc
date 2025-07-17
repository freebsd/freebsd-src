# Copyright 2011 The Kyua Authors.
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

if WITH_ATF
tests_integration_helpersdir = $(pkgtestsdir)/integration/helpers

tests_integration_helpers_PROGRAMS = integration/helpers/bad_test_program
integration_helpers_bad_test_program_SOURCES = \
    integration/helpers/bad_test_program.cpp

tests_integration_helpers_PROGRAMS += integration/helpers/bogus_test_cases
integration_helpers_bogus_test_cases_SOURCES = \
    integration/helpers/bogus_test_cases.cpp
integration_helpers_bogus_test_cases_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_bogus_test_cases_LDADD = $(ATF_CXX_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/config
integration_helpers_config_SOURCES = integration/helpers/config.cpp
integration_helpers_config_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_config_LDADD = $(ATF_CXX_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/dump_env
integration_helpers_dump_env_SOURCES = integration/helpers/dump_env.cpp
integration_helpers_dump_env_CXXFLAGS = $(UTILS_CFLAGS)
integration_helpers_dump_env_LDADD = $(UTILS_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/expect_all_pass
integration_helpers_expect_all_pass_SOURCES = \
    integration/helpers/expect_all_pass.cpp
integration_helpers_expect_all_pass_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_expect_all_pass_LDADD = $(ATF_CXX_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/expect_some_fail
integration_helpers_expect_some_fail_SOURCES = \
    integration/helpers/expect_some_fail.cpp
integration_helpers_expect_some_fail_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_expect_some_fail_LDADD = $(ATF_CXX_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/interrupts
integration_helpers_interrupts_SOURCES = integration/helpers/interrupts.cpp
integration_helpers_interrupts_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_interrupts_LDADD = $(ATF_CXX_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/metadata
integration_helpers_metadata_SOURCES = integration/helpers/metadata.cpp
integration_helpers_metadata_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_metadata_LDADD = $(ATF_CXX_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/race
integration_helpers_race_SOURCES = integration/helpers/race.cpp
integration_helpers_race_CXXFLAGS = $(UTILS_CFLAGS)
integration_helpers_race_LDADD = $(UTILS_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/simple_all_pass
integration_helpers_simple_all_pass_SOURCES = \
    integration/helpers/simple_all_pass.cpp
integration_helpers_simple_all_pass_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_simple_all_pass_LDADD = $(ATF_CXX_LIBS)

tests_integration_helpers_PROGRAMS += integration/helpers/simple_some_fail
integration_helpers_simple_some_fail_SOURCES = \
    integration/helpers/simple_some_fail.cpp
integration_helpers_simple_some_fail_CXXFLAGS = $(ATF_CXX_CFLAGS)
integration_helpers_simple_some_fail_LDADD = $(ATF_CXX_LIBS)
endif
