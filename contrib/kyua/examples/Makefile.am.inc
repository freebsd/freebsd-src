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

dist_examples_DATA  = examples/Kyuafile.top
dist_examples_DATA += examples/kyua.conf

if WITH_ATF
tests_examplesdir = $(pkgtestsdir)/examples

tests_examples_DATA = examples/Kyuafile
EXTRA_DIST += $(tests_examples_DATA)

tests_examples_PROGRAMS = examples/syntax_test
examples_syntax_test_SOURCES = examples/syntax_test.cpp
examples_syntax_test_CPPFLAGS = -DKYUA_EXAMPLESDIR="\"$(examplesdir)\""
examples_syntax_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                $(ATF_CXX_CFLAGS)
examples_syntax_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                             $(ATF_CXX_LIBS)
endif
