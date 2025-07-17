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

DRIVERS_CFLAGS = $(ENGINE_CFLAGS) $(STORE_CFLAGS) $(MODEL_CFLAGS) \
                 $(UTILS_CFLAGS)
DRIVERS_LIBS = libdrivers.a $(ENGINE_LIBS) $(STORE_LIBS) $(MODEL_LIBS) \
               $(UTILS_LIBS)

noinst_LIBRARIES += libdrivers.a
libdrivers_a_CPPFLAGS = $(DRIVERS_CFLAGS)
libdrivers_a_SOURCES  = drivers/debug_test.cpp
libdrivers_a_SOURCES += drivers/debug_test.hpp
libdrivers_a_SOURCES += drivers/list_tests.cpp
libdrivers_a_SOURCES += drivers/list_tests.hpp
libdrivers_a_SOURCES += drivers/report_junit.cpp
libdrivers_a_SOURCES += drivers/report_junit.hpp
libdrivers_a_SOURCES += drivers/run_tests.cpp
libdrivers_a_SOURCES += drivers/run_tests.hpp
libdrivers_a_SOURCES += drivers/scan_results.cpp
libdrivers_a_SOURCES += drivers/scan_results.hpp

if WITH_ATF
tests_driversdir = $(pkgtestsdir)/drivers

tests_drivers_DATA = drivers/Kyuafile
EXTRA_DIST += $(tests_drivers_DATA)

tests_drivers_PROGRAMS = drivers/list_tests_helpers
drivers_list_tests_helpers_SOURCES = drivers/list_tests_helpers.cpp
drivers_list_tests_helpers_CXXFLAGS = $(ATF_CXX_CFLAGS)
drivers_list_tests_helpers_LDADD = $(ATF_CXX_LIBS)

tests_drivers_PROGRAMS += drivers/list_tests_test
drivers_list_tests_test_SOURCES = drivers/list_tests_test.cpp
drivers_list_tests_test_CXXFLAGS = $(DRIVERS_CFLAGS) $(ATF_CXX_CFLAGS)
drivers_list_tests_test_LDADD = $(DRIVERS_LIBS) $(ATF_CXX_LIBS)

tests_drivers_PROGRAMS += drivers/report_junit_test
drivers_report_junit_test_SOURCES = drivers/report_junit_test.cpp
drivers_report_junit_test_CXXFLAGS = $(DRIVERS_CFLAGS) $(ATF_CXX_CFLAGS)
drivers_report_junit_test_LDADD = $(DRIVERS_LIBS) $(ATF_CXX_LIBS)

tests_drivers_PROGRAMS += drivers/scan_results_test
drivers_scan_results_test_SOURCES = drivers/scan_results_test.cpp
drivers_scan_results_test_CXXFLAGS = $(DRIVERS_CFLAGS) $(ATF_CXX_CFLAGS)
drivers_scan_results_test_LDADD = $(DRIVERS_LIBS) $(ATF_CXX_LIBS)
endif
