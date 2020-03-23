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

if WITH_ATF
tests_bootstrapdir = $(pkgtestsdir)/bootstrap

tests_bootstrap_DATA = bootstrap/Kyuafile
EXTRA_DIST += $(tests_bootstrap_DATA)

DISTCLEANFILES = bootstrap/atconfig \
                 bootstrap/testsuite.lineno \
                 bootstrap/testsuite.log

distclean-local: distclean-testsuite
distclean-testsuite:
	-rm -rf bootstrap/testsuite.dir

EXTRA_DIST += bootstrap/Kyuafile \
              bootstrap/testsuite \
              bootstrap/package.m4 \
              bootstrap/testsuite.at

tests_bootstrap_PROGRAMS = bootstrap/atf_helpers
bootstrap_atf_helpers_SOURCES = bootstrap/atf_helpers.cpp
bootstrap_atf_helpers_CXXFLAGS = $(ATF_CXX_CFLAGS)
bootstrap_atf_helpers_LDADD = $(ATF_CXX_LIBS)

tests_bootstrap_PROGRAMS += bootstrap/plain_helpers
bootstrap_plain_helpers_SOURCES = bootstrap/plain_helpers.cpp
bootstrap_plain_helpers_CXXFLAGS = $(UTILS_CFLAGS)

tests_bootstrap_SCRIPTS = bootstrap/testsuite
@target_srcdir@bootstrap/package.m4: $(top_srcdir)/configure.ac
	$(AM_V_GEN){ \
	echo '# Signature of the current package.'; \
	echo 'm4_define(AT_PACKAGE_NAME,      @PACKAGE_NAME@)'; \
	echo 'm4_define(AT_PACKAGE_TARNAME,   @PACKAGE_TARNAME@)'; \
	echo 'm4_define(AT_PACKAGE_VERSION,   @PACKAGE_VERSION@)'; \
	echo 'm4_define(AT_PACKAGE_STRING,    @PACKAGE_STRING@)'; \
	echo 'm4_define(AT_PACKAGE_BUGREPORT, @PACKAGE_BUGREPORT@)'; \
	} >$(srcdir)/bootstrap/package.m4

@target_srcdir@bootstrap/testsuite: $(srcdir)/bootstrap/testsuite.at \
                                    @target_srcdir@bootstrap/package.m4
	$(AM_V_GEN)autom4te --language=Autotest -I $(srcdir) \
	    -I $(srcdir)/bootstrap \
	    $(srcdir)/bootstrap/testsuite.at -o $@.tmp; \
	mv $@.tmp $@

CHECK_LOCAL += check-bootstrap
PHONY_TARGETS += check-bootstrap
check-bootstrap: @target_srcdir@bootstrap/testsuite $(check_PROGRAMS) \
                 $(CHECK_BOOTSTRAP_DEPS)
	cd bootstrap && $(CHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    ./testsuite

if !TARGET_SRCDIR_EMPTY
CHECK_BOOTSTRAP_DEPS += copy-bootstrap-testsuite
CHECK_KYUA_DEPS += copy-bootstrap-testsuite
PHONY_TARGETS += copy-bootstrap-testsuite
copy-bootstrap-testsuite:
	    cp -f @target_srcdir@bootstrap/testsuite bootstrap/testsuite
CLEANFILES += bootstrap/testsuite
endif
endif
