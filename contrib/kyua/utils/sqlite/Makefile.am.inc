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

UTILS_CFLAGS += $(SQLITE3_CFLAGS)
UTILS_LIBS += $(SQLITE3_LIBS)

libutils_a_CPPFLAGS += $(SQLITE3_CFLAGS)
libutils_a_SOURCES += utils/sqlite/c_gate.cpp
libutils_a_SOURCES += utils/sqlite/c_gate.hpp
libutils_a_SOURCES += utils/sqlite/c_gate_fwd.hpp
libutils_a_SOURCES += utils/sqlite/database.cpp
libutils_a_SOURCES += utils/sqlite/database.hpp
libutils_a_SOURCES += utils/sqlite/database_fwd.hpp
libutils_a_SOURCES += utils/sqlite/exceptions.cpp
libutils_a_SOURCES += utils/sqlite/exceptions.hpp
libutils_a_SOURCES += utils/sqlite/statement.cpp
libutils_a_SOURCES += utils/sqlite/statement.hpp
libutils_a_SOURCES += utils/sqlite/statement_fwd.hpp
libutils_a_SOURCES += utils/sqlite/statement.ipp
libutils_a_SOURCES += utils/sqlite/transaction.cpp
libutils_a_SOURCES += utils/sqlite/transaction.hpp
libutils_a_SOURCES += utils/sqlite/transaction_fwd.hpp

if WITH_ATF
tests_utils_sqlitedir = $(pkgtestsdir)/utils/sqlite

tests_utils_sqlite_DATA = utils/sqlite/Kyuafile
EXTRA_DIST += $(tests_utils_sqlite_DATA)

tests_utils_sqlite_PROGRAMS = utils/sqlite/c_gate_test
utils_sqlite_c_gate_test_SOURCES = utils/sqlite/c_gate_test.cpp \
                                   utils/sqlite/test_utils.hpp
utils_sqlite_c_gate_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_sqlite_c_gate_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_sqlite_PROGRAMS += utils/sqlite/database_test
utils_sqlite_database_test_SOURCES = utils/sqlite/database_test.cpp \
                                     utils/sqlite/test_utils.hpp
utils_sqlite_database_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_sqlite_database_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_sqlite_PROGRAMS += utils/sqlite/exceptions_test
utils_sqlite_exceptions_test_SOURCES = utils/sqlite/exceptions_test.cpp
utils_sqlite_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_sqlite_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_sqlite_PROGRAMS += utils/sqlite/statement_test
utils_sqlite_statement_test_SOURCES = utils/sqlite/statement_test.cpp \
                                      utils/sqlite/test_utils.hpp
utils_sqlite_statement_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_sqlite_statement_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_sqlite_PROGRAMS += utils/sqlite/transaction_test
utils_sqlite_transaction_test_SOURCES = utils/sqlite/transaction_test.cpp
utils_sqlite_transaction_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_sqlite_transaction_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
