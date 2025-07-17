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

libutils_a_SOURCES += utils/signals/exceptions.cpp
libutils_a_SOURCES += utils/signals/exceptions.hpp
libutils_a_SOURCES += utils/signals/interrupts.cpp
libutils_a_SOURCES += utils/signals/interrupts.hpp
libutils_a_SOURCES += utils/signals/interrupts_fwd.hpp
libutils_a_SOURCES += utils/signals/misc.cpp
libutils_a_SOURCES += utils/signals/misc.hpp
libutils_a_SOURCES += utils/signals/programmer.cpp
libutils_a_SOURCES += utils/signals/programmer.hpp
libutils_a_SOURCES += utils/signals/programmer_fwd.hpp
libutils_a_SOURCES += utils/signals/timer.cpp
libutils_a_SOURCES += utils/signals/timer.hpp
libutils_a_SOURCES += utils/signals/timer_fwd.hpp

if WITH_ATF
tests_utils_signalsdir = $(pkgtestsdir)/utils/signals

tests_utils_signals_DATA = utils/signals/Kyuafile
EXTRA_DIST += $(tests_utils_signals_DATA)

tests_utils_signals_PROGRAMS = utils/signals/exceptions_test
utils_signals_exceptions_test_SOURCES = utils/signals/exceptions_test.cpp
utils_signals_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_signals_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_signals_PROGRAMS += utils/signals/interrupts_test
utils_signals_interrupts_test_SOURCES = utils/signals/interrupts_test.cpp
utils_signals_interrupts_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_signals_interrupts_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_signals_PROGRAMS += utils/signals/misc_test
utils_signals_misc_test_SOURCES = utils/signals/misc_test.cpp
utils_signals_misc_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_signals_misc_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_signals_PROGRAMS += utils/signals/programmer_test
utils_signals_programmer_test_SOURCES = utils/signals/programmer_test.cpp
utils_signals_programmer_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_signals_programmer_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_signals_PROGRAMS += utils/signals/timer_test
utils_signals_timer_test_SOURCES = utils/signals/timer_test.cpp
utils_signals_timer_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_signals_timer_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
