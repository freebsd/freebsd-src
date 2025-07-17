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

UTILS_CFLAGS += $(LUTOK_CFLAGS)
UTILS_LIBS += $(LUTOK_LIBS)

libutils_a_CPPFLAGS += $(LUTOK_CFLAGS)
libutils_a_SOURCES += utils/config/exceptions.cpp
libutils_a_SOURCES += utils/config/exceptions.hpp
libutils_a_SOURCES += utils/config/keys.cpp
libutils_a_SOURCES += utils/config/keys.hpp
libutils_a_SOURCES += utils/config/keys_fwd.hpp
libutils_a_SOURCES += utils/config/lua_module.cpp
libutils_a_SOURCES += utils/config/lua_module.hpp
libutils_a_SOURCES += utils/config/nodes.cpp
libutils_a_SOURCES += utils/config/nodes.hpp
libutils_a_SOURCES += utils/config/nodes.ipp
libutils_a_SOURCES += utils/config/nodes_fwd.hpp
libutils_a_SOURCES += utils/config/parser.cpp
libutils_a_SOURCES += utils/config/parser.hpp
libutils_a_SOURCES += utils/config/parser_fwd.hpp
libutils_a_SOURCES += utils/config/tree.cpp
libutils_a_SOURCES += utils/config/tree.hpp
libutils_a_SOURCES += utils/config/tree.ipp
libutils_a_SOURCES += utils/config/tree_fwd.hpp

if WITH_ATF
tests_utils_configdir = $(pkgtestsdir)/utils/config

tests_utils_config_DATA = utils/config/Kyuafile
EXTRA_DIST += $(tests_utils_config_DATA)

tests_utils_config_PROGRAMS = utils/config/exceptions_test
utils_config_exceptions_test_SOURCES = utils/config/exceptions_test.cpp
utils_config_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_config_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_config_PROGRAMS += utils/config/keys_test
utils_config_keys_test_SOURCES = utils/config/keys_test.cpp
utils_config_keys_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_config_keys_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_config_PROGRAMS += utils/config/lua_module_test
utils_config_lua_module_test_SOURCES = utils/config/lua_module_test.cpp
utils_config_lua_module_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_config_lua_module_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_config_PROGRAMS += utils/config/nodes_test
utils_config_nodes_test_SOURCES = utils/config/nodes_test.cpp
utils_config_nodes_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_config_nodes_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_config_PROGRAMS += utils/config/parser_test
utils_config_parser_test_SOURCES = utils/config/parser_test.cpp
utils_config_parser_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_config_parser_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_config_PROGRAMS += utils/config/tree_test
utils_config_tree_test_SOURCES = utils/config/tree_test.cpp
utils_config_tree_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_config_tree_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
