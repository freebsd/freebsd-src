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

libutils_a_SOURCES += utils/process/child.cpp
libutils_a_SOURCES += utils/process/child.hpp
libutils_a_SOURCES += utils/process/child.ipp
libutils_a_SOURCES += utils/process/child_fwd.hpp
libutils_a_SOURCES += utils/process/deadline_killer.cpp
libutils_a_SOURCES += utils/process/deadline_killer.hpp
libutils_a_SOURCES += utils/process/deadline_killer_fwd.hpp
libutils_a_SOURCES += utils/process/exceptions.cpp
libutils_a_SOURCES += utils/process/exceptions.hpp
libutils_a_SOURCES += utils/process/executor.cpp
libutils_a_SOURCES += utils/process/executor.hpp
libutils_a_SOURCES += utils/process/executor.ipp
libutils_a_SOURCES += utils/process/executor_fwd.hpp
libutils_a_SOURCES += utils/process/fdstream.cpp
libutils_a_SOURCES += utils/process/fdstream.hpp
libutils_a_SOURCES += utils/process/fdstream_fwd.hpp
libutils_a_SOURCES += utils/process/isolation.cpp
libutils_a_SOURCES += utils/process/isolation.hpp
libutils_a_SOURCES += utils/process/operations.cpp
libutils_a_SOURCES += utils/process/operations.hpp
libutils_a_SOURCES += utils/process/operations_fwd.hpp
libutils_a_SOURCES += utils/process/status.cpp
libutils_a_SOURCES += utils/process/status.hpp
libutils_a_SOURCES += utils/process/status_fwd.hpp
libutils_a_SOURCES += utils/process/system.cpp
libutils_a_SOURCES += utils/process/system.hpp
libutils_a_SOURCES += utils/process/systembuf.cpp
libutils_a_SOURCES += utils/process/systembuf.hpp
libutils_a_SOURCES += utils/process/systembuf_fwd.hpp

if WITH_ATF
tests_utils_processdir = $(pkgtestsdir)/utils/process

tests_utils_process_DATA = utils/process/Kyuafile
EXTRA_DIST += $(tests_utils_process_DATA)

tests_utils_process_PROGRAMS = utils/process/child_test
utils_process_child_test_SOURCES = utils/process/child_test.cpp
utils_process_child_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_child_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/deadline_killer_test
utils_process_deadline_killer_test_SOURCES = \
    utils/process/deadline_killer_test.cpp
utils_process_deadline_killer_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_deadline_killer_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/exceptions_test
utils_process_exceptions_test_SOURCES = utils/process/exceptions_test.cpp
utils_process_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/executor_test
utils_process_executor_test_SOURCES = utils/process/executor_test.cpp
utils_process_executor_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_executor_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/executor_pid_test
utils_process_executor_pid_test_SOURCES = utils/process/executor_pid_test.cpp
utils_process_executor_pid_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_executor_pid_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/fdstream_test
utils_process_fdstream_test_SOURCES = utils/process/fdstream_test.cpp
utils_process_fdstream_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_fdstream_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/isolation_test
utils_process_isolation_test_SOURCES = utils/process/isolation_test.cpp
utils_process_isolation_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_isolation_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/helpers
utils_process_helpers_SOURCES = utils/process/helpers.cpp

tests_utils_process_PROGRAMS += utils/process/operations_test
utils_process_operations_test_SOURCES = utils/process/operations_test.cpp
utils_process_operations_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_operations_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/status_test
utils_process_status_test_SOURCES = utils/process/status_test.cpp
utils_process_status_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_status_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_process_PROGRAMS += utils/process/systembuf_test
utils_process_systembuf_test_SOURCES = utils/process/systembuf_test.cpp
utils_process_systembuf_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_process_systembuf_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
