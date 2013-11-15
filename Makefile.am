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

atf_aclocal_DATA =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
bin_PROGRAMS =
dist_man_MANS =
include_HEADERS =
lib_LTLIBRARIES =
libexec_PROGRAMS =
man_MANS =
noinst_DATA =
noinst_LTLIBRARIES =
INSTALLCHECK_TARGETS =
PHONY_TARGETS =

ACLOCAL_AMFLAGS = -I m4
AM_DISTCHECK_CONFIGURE_FLAGS = --enable-tools

include admin/Makefile.am.inc
include atf-c/Makefile.am.inc
include atf-c++/Makefile.am.inc
include atf-sh/Makefile.am.inc
include bootstrap/Makefile.am.inc
include doc/Makefile.am.inc
include test-programs/Makefile.am.inc

if ENABLE_TOOLS
include atf-report/Makefile.am.inc
include atf-config/Makefile.am.inc
include atf-run/Makefile.am.inc
include atf-version/Makefile.am.inc
endif

#
# Top-level distfile documents.
#

doc_DATA = AUTHORS COPYING NEWS README
noinst_DATA += INSTALL README
EXTRA_DIST += $(doc_DATA) INSTALL README

#
# Supporting logic to run our custom testsuite.
#

TESTS_ENVIRONMENT = PATH=$(prefix)/bin:$${PATH} \
                    PKG_CONFIG_PATH=$(prefix)/lib/pkgconfig

testsdir = $(exec_prefix)/tests
pkgtestsdir = $(testsdir)/$(PACKAGE)

if ENABLE_TOOLS
INSTALLCHECK_TARGETS += installcheck-atf
PHONY_TARGETS += installcheck-atf
installcheck-atf:
	logfile=$$(pwd)/installcheck.log; \
	fifofile=$$(pwd)/installcheck.fifo; \
	cd $(pkgtestsdir); \
	rm -f $${fifofile}; \
	mkfifo $${fifofile}; \
	cat $${fifofile} | tee $${logfile} | $(TESTS_ENVIRONMENT) atf-report & \
	$(TESTS_ENVIRONMENT) atf-run >>$${fifofile}; \
	res=$${?}; \
	wait; \
	rm $${fifofile}; \
	echo; \
	echo "The verbatim output of atf-run has been saved to" \
	     "installcheck.log; exit was $${res}"; \
	test $${res} -eq 0
CLEANFILES += installcheck.fifo installcheck.log
endif

PHONY_TARGETS += installcheck-kyua
if HAVE_KYUA
if !ENABLE_TOOLS
INSTALLCHECK_TARGETS += installcheck-kyua
endif
installcheck-kyua:
	cd $(pkgtestsdir) && $(TESTS_ENVIRONMENT) $(KYUA) test
endif

installcheck-local: $(INSTALLCHECK_TARGETS)

pkgtests_DATA = Kyuafile
if ENABLE_TOOLS
pkgtests_DATA += Atffile
endif
EXTRA_DIST += $(pkgtests_DATA)

BUILD_SH_TP = \
	echo "Creating $${dst}"; \
	echo "\#! $(bindir)/atf-sh" >$${dst}; \
	cat $${src} >>$${dst}; \
	chmod +x $${dst}

#
# Custom targets.
#

dist-hook: forbid-dist
if ENABLE_TOOLS
forbid-dist:
	@true
else
forbid-dist:
	@echo "Sorry; cannot make dist without the tools enabled."
	@echo "Please reconfigure with --enable-tools."
	@false
endif

PHONY_TARGETS += clean-all
clean-all:
	GIT="$(GIT)" $(SH) $(srcdir)/admin/clean-all.sh

PHONY_TARGETS += release
release:
	$(SH) $(srcdir)/admin/release.sh $(PACKAGE_VERSION) $(DIST_ARCHIVES)

PHONY_TARGETS += release-test
release-test:
	$(SH) $(srcdir)/admin/release-test.sh $(DIST_ARCHIVES)

.PHONY: $(PHONY_TARGETS)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
