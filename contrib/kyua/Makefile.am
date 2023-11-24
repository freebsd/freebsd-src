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

ACLOCAL_AMFLAGS = -I m4

CHECK_BOOTSTRAP_DEPS =
CHECK_KYUA_DEPS =
CHECK_LOCAL =
CLEAN_TARGETS =
DIST_HOOKS =
PHONY_TARGETS =
CLEANFILES =

EXTRA_DIST =
noinst_DATA =
noinst_LIBRARIES =
noinst_SCRIPTS =

doc_DATA = AUTHORS CONTRIBUTING.md CONTRIBUTORS LICENSE NEWS.md
noinst_DATA += INSTALL.md README.md
EXTRA_DIST += $(doc_DATA) INSTALL.md README.md

if WITH_ATF
tests_topdir = $(pkgtestsdir)

tests_top_DATA = Kyuafile
EXTRA_DIST += $(tests_top_DATA)
endif

include admin/Makefile.am.inc
include bootstrap/Makefile.am.inc
include cli/Makefile.am.inc
include doc/Makefile.am.inc
include drivers/Makefile.am.inc
include engine/Makefile.am.inc
include examples/Makefile.am.inc
include integration/Makefile.am.inc
include misc/Makefile.am.inc
include model/Makefile.am.inc
include store/Makefile.am.inc
include utils/Makefile.am.inc

bin_PROGRAMS = kyua
kyua_SOURCES = main.cpp
kyua_CXXFLAGS = $(CLI_CFLAGS) $(ENGINE_CFLAGS) $(UTILS_CFLAGS)
kyua_LDADD = $(CLI_LIBS) $(ENGINE_LIBS) $(UTILS_LIBS)

CHECK_ENVIRONMENT = KYUA_CONFDIR="/non-existent" \
                    KYUA_DOCDIR="$(abs_top_srcdir)" \
                    KYUA_EXAMPLESDIR="$(abs_top_srcdir)/examples" \
                    KYUA_MISCDIR="$(abs_top_srcdir)/misc" \
                    KYUA_STOREDIR="$(abs_top_srcdir)/store" \
                    KYUA_STORETESTDATADIR="$(abs_top_srcdir)/store" \
                    PATH="$(abs_top_builddir):$${PATH}"
INSTALLCHECK_ENVIRONMENT = KYUA_CONFDIR="/non-existent" \
                           PATH="$(prefix)/bin:$${PATH}"

# Generate local-kyua, a wrapper shell script to run the just-built 'kyua'
# binary by pointing it to the possibly not-yet-installed data files in the
# build tree.
noinst_SCRIPTS += local-kyua
CLEANFILES += local-kyua local-kyua.tmp
local-kyua: Makefile
	$(AM_V_GEN)echo '#!/bin/sh' >local-kyua.tmp; \
	echo 'env $(CHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT)' \
	    '"$(abs_top_builddir)/kyua" \
	    --config='$(KYUA_CONFIG_FILE_FOR_CHECK)' \
	    "$${@}"' >>local-kyua.tmp; \
	chmod +x local-kyua.tmp; \
	mv -f local-kyua.tmp local-kyua

if WITH_ATF
CHECK_LOCAL += dump-ulimits check-kyua
PHONY_TARGETS += check-kyua
check-kyua: $(CHECK_KYUA_DEPS)
	@failed=no; \
	./local-kyua test \
	    --kyuafile='$(top_srcdir)/Kyuafile' --build-root='$(top_builddir)' \
	    || failed=yes; \
	if [ "$${failed}" = yes ]; then \
	    ./local-kyua report --results-file='$(abs_top_srcdir)' \
	        --verbose --results-filter=broken,failed; \
	    exit 1; \
	fi

installcheck-local: dump-ulimits installcheck-kyua
PHONY_TARGETS += installcheck-kyua
installcheck-kyua:
	@failed=no; \
	cd $(pkgtestsdir) && $(INSTALLCHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    kyua --config='$(KYUA_CONFIG_FILE_FOR_CHECK)' test \
	    || failed=yes; \
	if [ "$${failed}" = yes ]; then \
	    cd $(pkgtestsdir) && $(INSTALLCHECK_ENVIRONMENT) \
	        $(TESTS_ENVIRONMENT) \
	        kyua --config='$(KYUA_CONFIG_FILE_FOR_CHECK)' report \
	        --verbose --results-filter=broken,failed; \
	    exit 1; \
	fi

# TODO(jmmv): kyua should probably be recording this information itself as part
# of the execution context, just as we record environment variables.
PHONY_TARGETS += dump-ulimits
dump-ulimits:
	@echo "Resource limits:"
	@{ \
	    ulimit -a | sed -e 's,$$, (soft),'; \
	    ulimit -a -H | sed -e 's,$$, (hard),'; \
	} | sort | sed -e 's,^,    ,'
	@echo
else
DIST_HOOKS += forbid-dist
PHONY_TARGETS += forbid-dist
forbid-dist:
	@echo "Sorry; cannot make dist without atf."
	@false
endif
check-local: $(CHECK_LOCAL)

if WITH_DOXYGEN
# Runs doxygen on the source tree and validates the contents of the docstrings.
# We do not do this by default, even if doxygen has been enabled, because this
# step takes a long time.  Instead, we just rely on a Travis CI build to catch
# inconsistencies.
PHONY_TARGETS += check-api-docs
check-api-docs: api-docs/api-docs.tag
	@$(AWK) -f $(srcdir)/admin/check-api-docs.awk api-docs/doxygen.out

api-docs/api-docs.tag: $(builddir)/Doxyfile $(SOURCES)
	@$(MKDIR_P) api-docs
	@rm -f api-docs/doxygen.out api-docs/doxygen.out.tmp
	$(AM_V_GEN)$(DOXYGEN) $(builddir)/Doxyfile \
	    >api-docs/doxygen.out.tmp 2>&1 && \
	    mv api-docs/doxygen.out.tmp api-docs/doxygen.out

CLEAN_TARGETS += clean-api-docs
clean-api-docs:
	rm -rf api-docs
endif

# Replace Automake's builtin check-news functionality so that we can validate
# the NEWS.md file instead of NEWS.
DIST_HOOKS += check-news
PHONY_TARGETS += check-news
check-news:
	@case "$$(sed 15q "$(srcdir)/NEWS.md")" in \
	*"$(VERSION)"*) : ;; \
	*) \
	    echo "NEWS.md not updated; not releasing" 1>&2; \
	    exit 1 \
	    ;; \
	esac

clean-local: $(CLEAN_TARGETS)
dist-hook: $(DIST_HOOKS)

PHONY_TARGETS += clean-all
clean-all:
	GIT="$(GIT)" $(SH) $(srcdir)/admin/clean-all.sh

.PHONY: $(PHONY_TARGETS)
