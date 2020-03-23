# Copyright 2014 The Kyua Authors.
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

MODEL_CFLAGS = $(UTILS_CFLAGS)
MODEL_LIBS = libmodel.a $(UTILS_LIBS)

noinst_LIBRARIES += libmodel.a
libmodel_a_CPPFLAGS = $(UTILS_CFLAGS)
libmodel_a_SOURCES  = model/context.cpp
libmodel_a_SOURCES += model/context.hpp
libmodel_a_SOURCES += model/context_fwd.hpp
libmodel_a_SOURCES += model/exceptions.cpp
libmodel_a_SOURCES += model/exceptions.hpp
libmodel_a_SOURCES += model/metadata.cpp
libmodel_a_SOURCES += model/metadata.hpp
libmodel_a_SOURCES += model/metadata_fwd.hpp
libmodel_a_SOURCES += model/test_case.cpp
libmodel_a_SOURCES += model/test_case.hpp
libmodel_a_SOURCES += model/test_case_fwd.hpp
libmodel_a_SOURCES += model/test_program.cpp
libmodel_a_SOURCES += model/test_program.hpp
libmodel_a_SOURCES += model/test_program_fwd.hpp
libmodel_a_SOURCES += model/test_result.cpp
libmodel_a_SOURCES += model/test_result.hpp
libmodel_a_SOURCES += model/test_result_fwd.hpp
libmodel_a_SOURCES += model/types.hpp

if WITH_ATF
tests_modeldir = $(pkgtestsdir)/model

tests_model_DATA = model/Kyuafile
EXTRA_DIST += $(tests_model_DATA)

tests_model_PROGRAMS = model/context_test
model_context_test_SOURCES = model/context_test.cpp
model_context_test_CXXFLAGS = $(MODEL_CFLAGS) $(ATF_CXX_CFLAGS)
model_context_test_LDADD = $(MODEL_LIBS) $(ATF_CXX_LIBS)

tests_model_PROGRAMS += model/exceptions_test
model_exceptions_test_SOURCES = model/exceptions_test.cpp
model_exceptions_test_CXXFLAGS = $(MODEL_CFLAGS) $(ATF_CXX_CFLAGS)
model_exceptions_test_LDADD = $(MODEL_LIBS) $(ATF_CXX_LIBS)

tests_model_PROGRAMS += model/metadata_test
model_metadata_test_SOURCES = model/metadata_test.cpp
model_metadata_test_CXXFLAGS = $(MODEL_CFLAGS) $(ATF_CXX_CFLAGS)
model_metadata_test_LDADD = $(MODEL_LIBS) $(ATF_CXX_LIBS)

tests_model_PROGRAMS += model/test_case_test
model_test_case_test_SOURCES = model/test_case_test.cpp
model_test_case_test_CXXFLAGS = $(MODEL_CFLAGS) $(ATF_CXX_CFLAGS)
model_test_case_test_LDADD = $(MODEL_LIBS) $(ATF_CXX_LIBS)

tests_model_PROGRAMS += model/test_program_test
model_test_program_test_SOURCES = model/test_program_test.cpp
model_test_program_test_CXXFLAGS = $(MODEL_CFLAGS) $(ATF_CXX_CFLAGS)
model_test_program_test_LDADD = $(MODEL_LIBS) $(ATF_CXX_LIBS)

tests_model_PROGRAMS += model/test_result_test
model_test_result_test_SOURCES = model/test_result_test.cpp
model_test_result_test_CXXFLAGS = $(MODEL_CFLAGS) $(ATF_CXX_CFLAGS)
model_test_result_test_LDADD = $(MODEL_LIBS) $(ATF_CXX_LIBS)

endif
