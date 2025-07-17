# Copyright 2012 Google Inc.
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

CXX ?= c++
CPPFLAGS ?=
CXXFLAGS ?= -Wall -O2
LDFLAGS ?=
LIBS ?=

LUTOK_CPPFLAGS = $$(pkg-config --cflags-only-I lutok)
LUTOK_CXXFLAGS = $$(pkg-config --cflags-only-other lutok)
LUTOK_LDFLAGS = $$(pkg-config --libs-only-L lutok) \
                $$(pkg-config --libs-only-other lutok)
LUTOK_LIBS = $$(pkg-config --libs-only-l lutok)

BUILD = $(CXX) \
        $(CPPFLAGS) $(LUTOK_CPPFLAGS) \
        $(CXXFLAGS) $(LUTOK_CXXFLAGS) \
        $(LDFLAGS) $(LUTOK_LDFLAGS) \
        -o $${target} $${source} \
        $(LIBS) $(LUTOK_LIBS)

PROGRAMS = bindings hello interpreter raii

.PHONY: all
all: $(PROGRAMS)

bindings: bindings.cpp
	@target=bindings source=bindings.cpp; echo $(BUILD); $(BUILD)

hello: hello.cpp
	@target=hello source=hello.cpp; echo $(BUILD); $(BUILD)

interpreter: interpreter.cpp
	@target=interpreter source=interpreter.cpp; echo $(BUILD); $(BUILD)

raii: raii.cpp
	@target=raii source=raii.cpp; echo $(BUILD); $(BUILD)

.PHONY: clean
clean:
	rm -f $(PROGRAMS)
