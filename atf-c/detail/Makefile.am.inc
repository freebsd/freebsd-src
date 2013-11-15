#
# Automated Testing Framework (atf)
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

libatf_c_la_SOURCES += atf-c/detail/dynstr.c \
                       atf-c/detail/dynstr.h \
                       atf-c/detail/env.c \
                       atf-c/detail/env.h \
                       atf-c/detail/fs.c \
                       atf-c/detail/fs.h \
                       atf-c/detail/list.c \
                       atf-c/detail/list.h \
                       atf-c/detail/map.c \
                       atf-c/detail/map.h \
                       atf-c/detail/process.c \
                       atf-c/detail/process.h \
                       atf-c/detail/sanity.c \
                       atf-c/detail/sanity.h \
                       atf-c/detail/text.c \
                       atf-c/detail/text.h \
                       atf-c/detail/tp_main.c \
                       atf-c/detail/user.c \
                       atf-c/detail/user.h

tests_atf_c_detail_DATA = atf-c/detail/Atffile \
                          atf-c/detail/Kyuafile
tests_atf_c_detaildir = $(pkgtestsdir)/atf-c/detail
EXTRA_DIST += $(tests_atf_c_detail_DATA)

noinst_LTLIBRARIES += atf-c/detail/libtest_helpers.la
atf_c_detail_libtest_helpers_la_SOURCES = atf-c/detail/test_helpers.c \
                                          atf-c/detail/test_helpers.h
atf_c_detail_libtest_helpers_la_CPPFLAGS = -I$(srcdir)/atf-c

tests_atf_c_detail_PROGRAMS = atf-c/detail/dynstr_test
atf_c_detail_dynstr_test_SOURCES = atf-c/detail/dynstr_test.c
atf_c_detail_dynstr_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/env_test
atf_c_detail_env_test_SOURCES = atf-c/detail/env_test.c
atf_c_detail_env_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/fs_test
atf_c_detail_fs_test_SOURCES = atf-c/detail/fs_test.c
atf_c_detail_fs_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/list_test
atf_c_detail_list_test_SOURCES = atf-c/detail/list_test.c
atf_c_detail_list_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/map_test
atf_c_detail_map_test_SOURCES = atf-c/detail/map_test.c
atf_c_detail_map_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/process_helpers
atf_c_detail_process_helpers_SOURCES = atf-c/detail/process_helpers.c

tests_atf_c_detail_PROGRAMS += atf-c/detail/process_test
atf_c_detail_process_test_SOURCES = atf-c/detail/process_test.c
atf_c_detail_process_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/sanity_test
atf_c_detail_sanity_test_SOURCES = atf-c/detail/sanity_test.c
atf_c_detail_sanity_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/text_test
atf_c_detail_text_test_SOURCES = atf-c/detail/text_test.c
atf_c_detail_text_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

tests_atf_c_detail_PROGRAMS += atf-c/detail/user_test
atf_c_detail_user_test_SOURCES = atf-c/detail/user_test.c
atf_c_detail_user_test_LDADD = atf-c/detail/libtest_helpers.la libatf-c.la

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
