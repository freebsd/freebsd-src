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

atf_aclocal_DATA =
BUILT_SOURCES =
CLEANFILES =
CODE_COVERAGE_DIRS =
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

include admin/Makefile.am.inc
include atf-c/Makefile.am.inc
include atf-c++/Makefile.am.inc
include atf-sh/Makefile.am.inc
include bootstrap/Makefile.am.inc
include doc/Makefile.am.inc
include test-programs/Makefile.am.inc

#
# Top-level distfile documents.
#

doc_DATA = AUTHORS COPYING NEWS README.md
noinst_DATA += INSTALL.md README.md
EXTRA_DIST += $(doc_DATA) INSTALL.md README.md

#
# Supporting logic to run our custom testsuite.
#

TESTS_ENVIRONMENT = PATH=$(prefix)/bin:$${PATH} \
                    PKG_CONFIG_PATH=$(prefix)/lib/pkgconfig

if HAVE_ASAN
# NB: the trailing comma is intentional. I hate automake not allowing `:=` use on subsequent lines.
if HAVE_LSAN
ASAN_LEAK_OPTIONS = detect_leaks=1,
else
ASAN_LEAK_OPTIONS =
endif
# Some of the testcases trigger false positives as they pass
# `SIZE_MAX - 1`, etc, to malloc(3).
ASAN_OPTIONS = ${ASAN_LEAK_OPTIONS}allocator_may_return_null=1
TESTS_ENVIRONMENT+= ASAN_OPTIONS=${ASAN_OPTIONS}
endif
if HAVE_UBSAN
TESTS_ENVIRONMENT+= UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
endif

AM_CFLAGS=	$(pc_cflags)
AM_CXXFLAGS=	$(pc_cxxflags)
AM_LDFLAGS=	$(pc_ldflags)

# Allow the caller to override the configuration file to passed to our
# test runs below.
KYUA_TEST_CONFIG_FILE = none

testsdir = $(exec_prefix)/tests
pkgtestsdir = $(testsdir)/$(PACKAGE)

PHONY_TARGETS += installcheck-kyua
if HAVE_KYUA
INSTALLCHECK_TARGETS += installcheck-kyua

installcheck-kyua:
	set +e; cd $(pkgtestsdir) && $(TESTS_ENVIRONMENT) \
	    $(KYUA) --config='$(KYUA_TEST_CONFIG_FILE)' test; \
	ret=$$?; \
	echo $$ret; \
	if [ -n "$$JUNIT_OUTPUT" ]; then \
	    cd $(pkgtestsdir) && $(TESTS_ENVIRONMENT) \
		    $(KYUA) --config='$(KYUA_TEST_CONFIG_FILE)' \
		    report-junit --output="$$JUNIT_OUTPUT"; \
	    sed -i '.orig' 's/encoding="iso-8859-1"/encoding="utf-8"/' "$$JUNIT_OUTPUT"; \
	    rm -f "$$JUNIT_OUTPUT.orig"; \
	fi; \
	cd $(pkgtestsdir) && $(TESTS_ENVIRONMENT) \
		$(KYUA) --config='$(KYUA_TEST_CONFIG_FILE)' report --verbose; \
	exit $$ret

if CODE_COVERAGE
CODE_COVERAGE_EXCLUDES = '/usr/include/*' '/usr/local/include/*'
CODE_COVERAGE_OUTPUT_DIR ?= code_coverage
EXTRA_GENHTML_ARGS ?=
INSTALLCHECK_TARGETS+=	installcheck-kyua-lcov

installcheck-kyua-lcov: installcheck-kyua
	mkdir -p $(CODE_COVERAGE_OUTPUT_DIR)/html
	$(LCOV) --capture --directory $(srcdir) \
	    --output-file $(CODE_COVERAGE_OUTPUT_DIR)/lcov.info
	$(LCOV) --remove $(CODE_COVERAGE_OUTPUT_DIR)/lcov.info \
	    $(CODE_COVERAGE_EXCLUDES) \
	    --output-file $(CODE_COVERAGE_OUTPUT_DIR)/lcov-filtered.info
	$(GENHTML) -o $(CODE_COVERAGE_OUTPUT_DIR)/html $(EXTRA_GENHTML_ARGS) \
	    $(CODE_COVERAGE_OUTPUT_DIR)/lcov-filtered.info
endif

endif

if CODE_COVERAGE
PHONY_TARGETS += mostlyclean-code-coverage
mostlyclean-local: mostlyclean-code-coverage
mostlyclean-code-coverage:
	-for dir in $(CODE_COVERAGE_DIRS); do \
	    find $$dir \( -name \*.gcda -or -name \*.gcno \) -delete -print; \
	done
endif

installcheck-local: $(INSTALLCHECK_TARGETS)

pkgtests_DATA = Kyuafile
EXTRA_DIST += $(pkgtests_DATA)

# Use octal \043 for '#' to avoid Automake's escaped-comment warning
BUILD_SH_TP = \
	test -d "$$(dirname "$${dst}")" || mkdir -p "$$(dirname "$${dst}")"; \
	printf '\043! %s/atf-sh\n' "$(bindir)" >"$${dst}"; \
	if [ -n "$${substs}" ]; then \
	    cat $${src} | sed $${substs} >>"$${dst}"; \
	else \
	    cat $${src} >>"$${dst}"; \
	fi; \
	chmod +x "$${dst}"

#
# Custom targets.
#

PHONY_TARGETS += clean-all
clean-all:
	GIT="$(GIT)" $(SH) $(srcdir)/admin/clean-all.sh

.PHONY: $(PHONY_TARGETS)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
