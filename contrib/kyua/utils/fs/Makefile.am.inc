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

UTILS_CFLAGS += $(LUTOK_CFLAGS)
UTILS_LIBS += $(LUTOK_LIBS)

libutils_a_CPPFLAGS += $(LUTOK_CFLAGS)
libutils_a_SOURCES += utils/fs/auto_cleaners.cpp
libutils_a_SOURCES += utils/fs/auto_cleaners.hpp
libutils_a_SOURCES += utils/fs/auto_cleaners_fwd.hpp
libutils_a_SOURCES += utils/fs/directory.cpp
libutils_a_SOURCES += utils/fs/directory.hpp
libutils_a_SOURCES += utils/fs/directory_fwd.hpp
libutils_a_SOURCES += utils/fs/exceptions.cpp
libutils_a_SOURCES += utils/fs/exceptions.hpp
libutils_a_SOURCES += utils/fs/lua_module.cpp
libutils_a_SOURCES += utils/fs/lua_module.hpp
libutils_a_SOURCES += utils/fs/operations.cpp
libutils_a_SOURCES += utils/fs/operations.hpp
libutils_a_SOURCES += utils/fs/path.cpp
libutils_a_SOURCES += utils/fs/path.hpp
libutils_a_SOURCES += utils/fs/path_fwd.hpp

if WITH_ATF
tests_utils_fsdir = $(pkgtestsdir)/utils/fs

tests_utils_fs_DATA = utils/fs/Kyuafile
EXTRA_DIST += $(tests_utils_fs_DATA)

tests_utils_fs_PROGRAMS = utils/fs/auto_cleaners_test
utils_fs_auto_cleaners_test_SOURCES = utils/fs/auto_cleaners_test.cpp
utils_fs_auto_cleaners_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_fs_auto_cleaners_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_fs_PROGRAMS += utils/fs/directory_test
utils_fs_directory_test_SOURCES = utils/fs/directory_test.cpp
utils_fs_directory_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_fs_directory_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_fs_PROGRAMS += utils/fs/exceptions_test
utils_fs_exceptions_test_SOURCES = utils/fs/exceptions_test.cpp
utils_fs_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_fs_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_fs_PROGRAMS += utils/fs/lua_module_test
utils_fs_lua_module_test_SOURCES = utils/fs/lua_module_test.cpp
utils_fs_lua_module_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_fs_lua_module_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_fs_PROGRAMS += utils/fs/operations_test
utils_fs_operations_test_SOURCES = utils/fs/operations_test.cpp
utils_fs_operations_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_fs_operations_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_fs_PROGRAMS += utils/fs/path_test
utils_fs_path_test_SOURCES = utils/fs/path_test.cpp
utils_fs_path_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_fs_path_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
