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

BUILD_MANPAGE = \
	$(MKDIR_P) doc; \
	$(SHELL) $(srcdir)/doc/manbuild.sh \
	    -v "CONFDIR=$(kyua_confdir)" \
	    -v "DOCDIR=$(docdir)" \
	    -v "EGDIR=$(examplesdir)" \
	    -v "MISCDIR=$(miscdir)" \
	    -v "PACKAGE=$(PACKAGE_TARNAME)" \
	    -v "STOREDIR=$(storedir)" \
	    -v "TESTSDIR=$(testsdir)" \
	    -v "VERSION=$(PACKAGE_VERSION)" \
	    "$(srcdir)/doc/$${name}.in" "doc/$${name}"

DIST_MAN_DEPS = doc/manbuild.sh \
                doc/build-root.mdoc \
                doc/results-file-flag-read.mdoc \
                doc/results-file-flag-write.mdoc \
                doc/results-files.mdoc \
                doc/results-files-report-example.mdoc \
                doc/test-filters.mdoc \
                doc/test-isolation.mdoc
MAN_DEPS = $(DIST_MAN_DEPS) Makefile
EXTRA_DIST += $(DIST_MAN_DEPS)

man_MANS = doc/kyua-about.1
CLEANFILES += doc/kyua-about.1
EXTRA_DIST += doc/kyua-about.1.in
doc/kyua-about.1: $(srcdir)/doc/kyua-about.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-about.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-config.1
CLEANFILES += doc/kyua-config.1
EXTRA_DIST += doc/kyua-config.1.in
doc/kyua-config.1: $(srcdir)/doc/kyua-config.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-config.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-db-exec.1
CLEANFILES += doc/kyua-db-exec.1
EXTRA_DIST += doc/kyua-db-exec.1.in
doc/kyua-db-exec.1: $(srcdir)/doc/kyua-db-exec.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-db-exec.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-db-migrate.1
CLEANFILES += doc/kyua-db-migrate.1
EXTRA_DIST += doc/kyua-db-migrate.1.in
doc/kyua-db-migrate.1: $(srcdir)/doc/kyua-db-migrate.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-db-migrate.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-debug.1
CLEANFILES += doc/kyua-debug.1
EXTRA_DIST += doc/kyua-debug.1.in
doc/kyua-debug.1: $(srcdir)/doc/kyua-debug.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-debug.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-help.1
CLEANFILES += doc/kyua-help.1
EXTRA_DIST += doc/kyua-help.1.in
doc/kyua-help.1: $(srcdir)/doc/kyua-help.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-help.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-list.1
CLEANFILES += doc/kyua-list.1
EXTRA_DIST += doc/kyua-list.1.in
doc/kyua-list.1: $(srcdir)/doc/kyua-list.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-list.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-report-html.1
CLEANFILES += doc/kyua-report-html.1
EXTRA_DIST += doc/kyua-report-html.1.in
doc/kyua-report-html.1: $(srcdir)/doc/kyua-report-html.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-report-html.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-report-junit.1
CLEANFILES += doc/kyua-report-junit.1
EXTRA_DIST += doc/kyua-report-junit.1.in
doc/kyua-report-junit.1: $(srcdir)/doc/kyua-report-junit.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-report-junit.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-report.1
CLEANFILES += doc/kyua-report.1
EXTRA_DIST += doc/kyua-report.1.in
doc/kyua-report.1: $(srcdir)/doc/kyua-report.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-report.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-test.1
CLEANFILES += doc/kyua-test.1
EXTRA_DIST += doc/kyua-test.1.in
doc/kyua-test.1: $(srcdir)/doc/kyua-test.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua-test.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua.1
CLEANFILES += doc/kyua.1
EXTRA_DIST += doc/kyua.1.in
doc/kyua.1: $(srcdir)/doc/kyua.1.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua.conf.5
CLEANFILES += doc/kyua.conf.5
EXTRA_DIST += doc/kyua.conf.5.in
doc/kyua.conf.5: $(srcdir)/doc/kyua.conf.5.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyua.conf.5; $(BUILD_MANPAGE)

man_MANS += doc/kyuafile.5
CLEANFILES += doc/kyuafile.5
EXTRA_DIST += doc/kyuafile.5.in
doc/kyuafile.5: $(srcdir)/doc/kyuafile.5.in $(MAN_DEPS)
	$(AM_V_GEN)name=kyuafile.5; $(BUILD_MANPAGE)

if WITH_ATF
EXTRA_DIST += doc/Kyuafile

noinst_SCRIPTS += doc/manbuild_test
CLEANFILES += doc/manbuild_test
EXTRA_DIST += doc/manbuild_test.sh
doc/manbuild_test: $(srcdir)/doc/manbuild_test.sh Makefile
	$(AM_V_GEN)$(MKDIR_P) doc; \
	echo "#! $(ATF_SH)" >doc/manbuild_test.tmp; \
	echo "# AUTOMATICALLY GENERATED FROM Makefile" \
	    >>doc/manbuild_test.tmp; \
	sed -e 's,__MANBUILD__,$(abs_srcdir)/doc/manbuild.sh,g' \
	    <$(srcdir)/doc/manbuild_test.sh >>doc/manbuild_test.tmp; \
	mv doc/manbuild_test.tmp doc/manbuild_test; \
	chmod +x doc/manbuild_test
endif
