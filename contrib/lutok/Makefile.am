# Copyright 2010 Google Inc.
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

doc_DATA = AUTHORS COPYING NEWS README
noinst_DATA = INSTALL README
EXTRA_DIST = $(doc_DATA) INSTALL README

LUTOK_CFLAGS = -I$(srcdir)/include $(LUA_CFLAGS)
LUTOK_LIBS = liblutok.la $(LUA_LIBS)

pkginclude_HEADERS  = c_gate.hpp
pkginclude_HEADERS += debug.hpp
pkginclude_HEADERS += exceptions.hpp
pkginclude_HEADERS += operations.hpp
pkginclude_HEADERS += stack_cleaner.hpp
pkginclude_HEADERS += state.hpp
pkginclude_HEADERS += state.ipp
pkginclude_HEADERS += test_utils.hpp

EXTRA_DIST += include/lutok/README
EXTRA_DIST += include/lutok/c_gate.hpp
EXTRA_DIST += include/lutok/debug.hpp
EXTRA_DIST += include/lutok/exceptions.hpp
EXTRA_DIST += include/lutok/operations.hpp
EXTRA_DIST += include/lutok/stack_cleaner.hpp
EXTRA_DIST += include/lutok/state.hpp
EXTRA_DIST += include/lutok/state.ipp

lib_LTLIBRARIES = liblutok.la
liblutok_la_SOURCES  = c_gate.cpp
liblutok_la_SOURCES += c_gate.hpp
liblutok_la_SOURCES += debug.cpp
liblutok_la_SOURCES += debug.hpp
liblutok_la_SOURCES += exceptions.cpp
liblutok_la_SOURCES += exceptions.hpp
liblutok_la_SOURCES += operations.cpp
liblutok_la_SOURCES += operations.hpp
liblutok_la_SOURCES += stack_cleaner.cpp
liblutok_la_SOURCES += stack_cleaner.hpp
liblutok_la_SOURCES += state.cpp
liblutok_la_SOURCES += state.hpp
liblutok_la_SOURCES += state.ipp
liblutok_la_SOURCES += test_utils.hpp
liblutok_la_CPPFLAGS = $(LUTOK_CFLAGS)
liblutok_la_LDFLAGS = -version-info 3:0:0
liblutok_la_LIBADD = $(LUA_LIBS)

pkgconfig_DATA = lutok.pc
CLEANFILES = lutok.pc
EXTRA_DIST += lutok.pc.in
lutok.pc: $(srcdir)/lutok.pc.in Makefile
	$(AM_V_GEN)sed -e 's#__INCLUDEDIR__#$(includedir)#g' \
	    -e 's#__LIBDIR__#$(libdir)#g' \
	    -e 's#__LUA_CFLAGS__#$(LUA_CFLAGS)#g' \
	    -e 's#__LUA_LIBS__#$(LUA_LIBS)#g' \
	    -e 's#__VERSION__#$(PACKAGE_VERSION)#g' \
	    <$(srcdir)/lutok.pc.in >lutok.pc.tmp; \
	mv lutok.pc.tmp lutok.pc

CLEAN_TARGETS =
DIST_HOOKS =
PHONY_TARGETS =

examplesdir = $(docdir)/examples
examples_DATA  = examples/Makefile
examples_DATA += examples/bindings.cpp
examples_DATA += examples/hello.cpp
examples_DATA += examples/interpreter.cpp
examples_DATA += examples/raii.cpp
EXTRA_DIST += $(examples_DATA)

if WITH_ATF
tests_DATA = Kyuafile
EXTRA_DIST += $(tests_DATA)

tests_PROGRAMS = c_gate_test
c_gate_test_SOURCES = c_gate_test.cpp test_utils.hpp
c_gate_test_CXXFLAGS = $(LUTOK_CFLAGS) $(ATF_CXX_CFLAGS)
c_gate_test_LDADD = $(LUTOK_LIBS) $(ATF_CXX_LIBS)

tests_PROGRAMS += debug_test
debug_test_SOURCES = debug_test.cpp test_utils.hpp
debug_test_CXXFLAGS = $(LUTOK_CFLAGS) $(ATF_CXX_CFLAGS)
debug_test_LDADD = $(LUTOK_LIBS) $(ATF_CXX_LIBS)

tests_SCRIPTS = examples_test
CLEANFILES += examples_test
EXTRA_DIST += examples_test.sh
examples_test: $(srcdir)/examples_test.sh
	$(AM_V_GEN)sed -e 's,__ATF_SH__,$(ATF_SH),g' \
	    -e 's,__CXX__,$(CXX),g' \
	    -e 's,__EXAMPLESDIR__,$(examplesdir),g' \
	    -e 's,__LIBDIR__,$(libdir),g' \
	    <$(srcdir)/examples_test.sh >examples_test.tmp; \
	chmod +x examples_test.tmp; \
	rm -f examples_test; \
	mv examples_test.tmp examples_test

tests_PROGRAMS += exceptions_test
exceptions_test_SOURCES = exceptions_test.cpp
exceptions_test_CXXFLAGS = $(LUTOK_CFLAGS) $(ATF_CXX_CFLAGS)
exceptions_test_LDADD = $(LUTOK_LIBS) $(ATF_CXX_LIBS)

tests_PROGRAMS += operations_test
operations_test_SOURCES = operations_test.cpp test_utils.hpp
operations_test_CXXFLAGS = $(LUTOK_CFLAGS) $(ATF_CXX_CFLAGS)
operations_test_LDADD = $(LUTOK_LIBS) $(ATF_CXX_LIBS)

tests_PROGRAMS += stack_cleaner_test
stack_cleaner_test_SOURCES = stack_cleaner_test.cpp test_utils.hpp
stack_cleaner_test_CXXFLAGS = $(LUTOK_CFLAGS) $(ATF_CXX_CFLAGS)
stack_cleaner_test_LDADD = $(LUTOK_LIBS) $(ATF_CXX_LIBS)

tests_PROGRAMS += state_test
state_test_SOURCES = state_test.cpp test_utils.hpp
state_test_CXXFLAGS = $(LUTOK_CFLAGS) $(ATF_CXX_CFLAGS)
state_test_LDADD = $(LUTOK_LIBS) $(ATF_CXX_LIBS)

if HAVE_KYUA
check-local: check-kyua
PHONY_TARGETS += check-kyua
check-kyua:
	$(TESTS_ENVIRONMENT) kyua test \
	    --kyuafile='$(top_srcdir)/Kyuafile' --build-root='$(top_builddir)'

installcheck-local: installcheck-kyua
PHONY_TARGETS += installcheck-kyua
installcheck-kyua:
	cd $(testsdir) && $(TESTS_ENVIRONMENT) kyua test
endif
else
DIST_HOOKS += dist-no-atf
PHONY_TARGETS += dist-no-atf
dist-no-atf:
	@echo "Sorry; cannot build a distfile without atf"
	@false
endif

if WITH_DOXYGEN
# This should probably be html-local, but it seems better to generate the
# documentation in all cases to get warnings about missing documentation every
# time the code is modified.  (And, after all, the documentation is not
# installed so generating it unconditionally is not a big problem.)
all-local: api-docs/api-docs.tag

api-docs/api-docs.tag: $(builddir)/Doxyfile $(SOURCES)
	$(AM_V_GEN)rm -rf api-docs; \
	mkdir -p api-docs; \
	${DOXYGEN} $(builddir)/Doxyfile 2>&1 | tee api-docs/warnings; \
	rm -f api-docs/html/installdox
api-docs/html: api-docs/api-docs.tag

CLEAN_TARGETS += clean-api-docs
clean-api-docs:
	rm -rf api-docs

EXTRA_DIST += api-docs/html
else
DIST_HOOKS += dist-no-doxygen
PHONY_TARGETS += dist-no-doxygen
dist-no-doxygen:
	@echo "Sorry; cannot build a distfile without Doxygen"
	@false
endif

install-data-local: install-api-docs
install-api-docs: install-docDATA
	@echo "Installing HTML documentation into $(DESTDIR)$(htmldir)"
	@if [ -d api-docs/html ]; then \
	    test -z "$(htmldir)" || $(MKDIR_P) "$(DESTDIR)$(htmldir)"; \
	    ( cd api-docs/html && tar cf - . ) | \
                ( cd "$(DESTDIR)$(htmldir)" && tar xf - ); \
	elif [ -d "$(srcdir)/api-docs/html" ]; then \
	    test -z "$(htmldir)" || $(MKDIR_P) "$(DESTDIR)$(htmldir)"; \
	    ( cd "$(srcdir)/api-docs/html" && tar cf - . ) | \
                ( cd "$(DESTDIR)$(htmldir)" && tar xf - ); \
	else \
	    echo "Doxygen not installed and prebuilt documents not found"; \
	fi

uninstall-local: uninstall-api-docs
uninstall-api-docs:
	find "$(DESTDIR)$(htmldir)" -type d -exec chmod 755 {} \;
	rm -rf "$(DESTDIR)$(htmldir)"

clean-local: $(CLEAN_TARGETS)

PHONY_TARGETS += clean-all
clean-all:
	GIT="$(GIT)" $(SH) $(srcdir)/admin/clean-all.sh

dist-hook: $(DIST_HOOKS)

.PHONY: ${PHONY_TARGETS}
