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

STORE_CFLAGS = $(MODEL_CFLAGS) $(UTILS_CFLAGS)
STORE_LIBS = libstore.a $(MODEL_LIBS) $(UTILS_LIBS)

noinst_LIBRARIES += libstore.a
libstore_a_CPPFLAGS  = -DKYUA_STOREDIR=\"$(storedir)\"
libstore_a_CPPFLAGS += $(UTILS_CFLAGS)
libstore_a_SOURCES  = store/dbtypes.cpp
libstore_a_SOURCES += store/dbtypes.hpp
libstore_a_SOURCES += store/exceptions.cpp
libstore_a_SOURCES += store/exceptions.hpp
libstore_a_SOURCES += store/layout.cpp
libstore_a_SOURCES += store/layout.hpp
libstore_a_SOURCES += store/layout_fwd.hpp
libstore_a_SOURCES += store/metadata.cpp
libstore_a_SOURCES += store/metadata.hpp
libstore_a_SOURCES += store/metadata_fwd.hpp
libstore_a_SOURCES += store/migrate.cpp
libstore_a_SOURCES += store/migrate.hpp
libstore_a_SOURCES += store/read_backend.cpp
libstore_a_SOURCES += store/read_backend.hpp
libstore_a_SOURCES += store/read_backend_fwd.hpp
libstore_a_SOURCES += store/read_transaction.cpp
libstore_a_SOURCES += store/read_transaction.hpp
libstore_a_SOURCES += store/read_transaction_fwd.hpp
libstore_a_SOURCES += store/write_backend.cpp
libstore_a_SOURCES += store/write_backend.hpp
libstore_a_SOURCES += store/write_backend_fwd.hpp
libstore_a_SOURCES += store/write_transaction.cpp
libstore_a_SOURCES += store/write_transaction.hpp
libstore_a_SOURCES += store/write_transaction_fwd.hpp

dist_store_DATA  = store/migrate_v1_v2.sql
dist_store_DATA += store/migrate_v2_v3.sql
dist_store_DATA += store/schema_v3.sql

if WITH_ATF
tests_storedir = $(pkgtestsdir)/store

tests_store_DATA  = store/Kyuafile
tests_store_DATA += store/schema_v1.sql
tests_store_DATA += store/schema_v2.sql
tests_store_DATA += store/testdata_v1.sql
tests_store_DATA += store/testdata_v2.sql
tests_store_DATA += store/testdata_v3_1.sql
tests_store_DATA += store/testdata_v3_2.sql
tests_store_DATA += store/testdata_v3_3.sql
tests_store_DATA += store/testdata_v3_4.sql
EXTRA_DIST += $(tests_store_DATA)

tests_store_PROGRAMS = store/dbtypes_test
store_dbtypes_test_SOURCES = store/dbtypes_test.cpp
store_dbtypes_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                              $(ATF_CXX_CFLAGS)
store_dbtypes_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/exceptions_test
store_exceptions_test_SOURCES = store/exceptions_test.cpp
store_exceptions_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                                 $(ATF_CXX_CFLAGS)
store_exceptions_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/layout_test
store_layout_test_SOURCES = store/layout_test.cpp
store_layout_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
store_layout_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/metadata_test
store_metadata_test_SOURCES = store/metadata_test.cpp
store_metadata_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                               $(ATF_CXX_CFLAGS)
store_metadata_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/migrate_test
store_migrate_test_SOURCES = store/migrate_test.cpp
store_migrate_test_CPPFLAGS = -DKYUA_STOREDIR=\"$(storedir)\"
store_migrate_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) $(ATF_CXX_CFLAGS)
store_migrate_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/read_backend_test
store_read_backend_test_SOURCES = store/read_backend_test.cpp
store_read_backend_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                                   $(ATF_CXX_CFLAGS)
store_read_backend_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/read_transaction_test
store_read_transaction_test_SOURCES = store/read_transaction_test.cpp
store_read_transaction_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                                       $(ATF_CXX_CFLAGS)
store_read_transaction_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/schema_inttest
store_schema_inttest_SOURCES = store/schema_inttest.cpp
store_schema_inttest_CPPFLAGS = -DKYUA_STORETESTDATADIR=\"$(tests_storedir)\"
store_schema_inttest_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                                $(ATF_CXX_CFLAGS)
store_schema_inttest_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/transaction_test
store_transaction_test_SOURCES = store/transaction_test.cpp
store_transaction_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                                  $(ATF_CXX_CFLAGS)
store_transaction_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/write_backend_test
store_write_backend_test_SOURCES = store/write_backend_test.cpp
store_write_backend_test_CPPFLAGS = -DKYUA_STOREDIR=\"$(storedir)\"
store_write_backend_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                                    $(ATF_CXX_CFLAGS)
store_write_backend_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/write_transaction_test
store_write_transaction_test_SOURCES = store/write_transaction_test.cpp
store_write_transaction_test_CXXFLAGS = $(STORE_CFLAGS) $(ENGINE_CFLAGS) \
                                        $(ATF_CXX_CFLAGS)
store_write_transaction_test_LDADD = $(STORE_LIBS) $(ENGINE_LIBS) \
                                     $(ATF_CXX_LIBS)
endif
