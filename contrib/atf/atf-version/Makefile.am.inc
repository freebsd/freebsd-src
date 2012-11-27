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

bin_PROGRAMS += atf-version/atf-version
atf_version_atf_version_SOURCES = atf-version/atf-version.cpp
nodist_atf_version_atf_version_SOURCES = atf-version/revision.h
atf_version_atf_version_CPPFLAGS = -Iatf-version
atf_version_atf_version_LDADD = $(ATF_CXX_LIBS)
dist_man_MANS += atf-version/atf-version.1

EXTRA_DIST += atf-version/generate-revision.sh

BUILT_SOURCES += atf-version/revision.h
CLEANFILES += atf-version/revision.h
atf-version/revision.h: atf-version/revision.h.stamp
	@test -d atf-version || mkdir -p atf-version
	@cmp -s atf-version/revision.h atf-version/revision.h.stamp || \
	    cp -p atf-version/revision.h.stamp atf-version/revision.h

CLEANFILES += atf-version/revision.h.stamp
PHONY_TARGETS += atf-version/revision.h.stamp
atf-version/revision.h.stamp:
	@test -d atf-version || mkdir -p atf-version
	@$(top_srcdir)/atf-version/generate-revision.sh \
	    -g "$(GIT)" -r $(top_srcdir) -o atf-version/revision.h.stamp \
	    -v $(PACKAGE_VERSION)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
