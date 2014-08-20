# Makefile.am:
#   Source file for Makefile.in (and hence Makefile)
#

PNGLIB_BASENAME= libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@

# libpng does not follow GNU file name conventions

# "color-tests" requires automake 1.11.1 or later.  Enable it if you like,
# to get red "FAIL" and green "PASS" notations during tests.
# AUTOMAKE_OPTIONS = foreign color-tests
AUTOMAKE_OPTIONS = foreign

# test programs - run on make check, make distcheck
TESTS_ENVIRONMENT= srcdir=$(srcdir)
check_PROGRAMS= pngtest
pngtest_SOURCES = pngtest.c
pngtest_LDADD = libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@.la
TESTS = test-pngtest.sh

# Only do the following if the contrib directory is present.
check_PROGRAMS+= pngvalid
pngvalid_SOURCES = contrib/libtests/pngvalid.c
pngvalid_LDADD = libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@.la
TESTS += test-pngvalid-simple.sh test-pngvalid-full.sh

# man pages
dist_man_MANS= libpng.3 libpngpf.3 png.5

# generate the -config scripts if required
binconfigs= libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@-config
EXTRA_SCRIPTS= libpng-config libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@-config
bin_SCRIPTS= @binconfigs@

# rules to build libpng, only build the old library on request
lib_LTLIBRARIES=libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@.la
# EXTRA_LTLIBRARIES= libpng.la
libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_SOURCES = png.c pngerror.c\
	pngget.c pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c\
	pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c pngwutil.c\
	png.h pngconf.h pngdebug.h pnginfo.h pngpriv.h pngstruct.h pngusr.dfa

if PNG_ARM_NEON
libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_SOURCES += arm/arm_init.c\
	arm/filter_neon.S
endif

nodist_libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_SOURCES = pnglibconf.h

libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_CPPFLAGS = @LIBPNG_DEFINES@

libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_LDFLAGS = -no-undefined -export-dynamic \
	-version-number @PNGLIB_MAJOR@@PNGLIB_MINOR@:@PNGLIB_RELEASE@:0

if HAVE_LD_VERSION_SCRIPT
#   Versioned symbols and restricted exports
if HAVE_SOLARIS_LD
  libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_LDFLAGS += -Wl,-M -Wl,libpng.vers
else
  libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_LDFLAGS += -Wl,--version-script=libpng.vers
endif

  libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_DEPENDENCIES = libpng.vers
else
#   Only restricted exports when possible
  libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_LDFLAGS += -export-symbols libpng.sym
  libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_DEPENDENCIES = libpng.sym
endif

#distribute headers in /usr/include/libpng/*
pkgincludedir= $(includedir)/$(PNGLIB_BASENAME)
pkginclude_HEADERS= png.h pngconf.h
nodist_pkginclude_HEADERS= pnglibconf.h

# pkg-config stuff, note that libpng.pc is always required in order
# to get the correct library
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@.pc

#extra source distribution files.
EXTRA_DIST= \
	ANNOUNCE CHANGES INSTALL LICENSE README TODO \
	pngtest.png pngbar.png pngnow.png pngbar.jpg autogen.sh \
	${srcdir}/contrib ${srcdir}/projects ${srcdir}/scripts \
	$(TESTS) \
	CMakeLists.txt example.c libpng-manual.txt

SCRIPT_CLEANFILES=scripts/*.out scripts/*.chk scripts/pnglibconf.dfn

CLEANFILES= dfn.c dfn?.out *.out.? scripts/*.out.? pngout.png \
	libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@.pc \
	libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@-config libpng.vers libpng.sym \
	check.new pnglibconf.* symbols.new pngtest-log.txt \
	$(SCRIPT_CLEANFILES)

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess config.h.in \
config.sub configure depcomp install-sh ltmain.sh missing

# DFNCPP is normally just CPP - the C preprocessor - but on Solaris and maybe
# other operating systems (NeXT?) the C preprocessor selected by configure
# checks input tokens for validity - effectively it performs part of the ANSI-C
# parsing - and therefore fails with the .df files.  configure.ac has special
# checks for this and sets DFNCPP appropriately.
DFNCPP = @DFNCPP@

SUFFIXES = .chk .dfn .out

$(PNGLIB_BASENAME).pc: libpng.pc
	cp libpng.pc $@

$(PNGLIB_BASENAME)-config: libpng-config
	cp libpng-config $@

scripts/sym.out scripts/vers.out: png.h pngconf.h pnglibconf.h
scripts/symbols.out: png.h pngconf.h $(srcdir)/scripts/pnglibconf.h.prebuilt

libpng.sym: scripts/sym.out
	rm -f $@
	cp $? $@
libpng.vers: scripts/vers.out
	rm -f $@
	cp $? $@
pnglibconf.h: pnglibconf.out
	rm -f $@
	cp $? $@
$(srcdir)/scripts/pnglibconf.h.prebuilt:
	@echo "Attempting to build $@" >&2
	@echo "This is a machine generated file, but if you want to make" >&2
	@echo "a new one simply make 'scripts/pnglibconf.out' and copy that" >&2
	@exit 1

# The following is necessary to ensure that the local pnglibconf.h is used, not
# an installed one (this can happen immediately after on a clean system if
# 'make test' is the first thing the user does.)
contrib/libtests/pngvalid.o pngtest.o: pnglibconf.h

# We must use -DPNG_NO_USE_READ_MACROS here even when the library may actually
# be built with PNG_USE_READ_MACROS; this prevents the read macros from
# interfering with the symbol file format.
SYMBOL_CFLAGS = -DPNGLIB_LIBNAME='PNG@PNGLIB_MAJOR@@PNGLIB_MINOR@_0'\
		-DPNGLIB_VERSION='@PNGLIB_VERSION@'\
		-DSYMBOL_PREFIX='$(SYMBOL_PREFIX)'\
		-DPNG_NO_USE_READ_MACROS -DPNG_BUILDING_SYMBOL_TABLE

.dfn.out:
	rm -f $@ $@.c $@.?
	test -d scripts || mkdir scripts || test -d scripts
	echo '#include "$<"' >$@.c
	$(DFNCPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) @LIBPNG_DEFINES@\
	    $(CPPFLAGS) $(SYMBOL_CFLAGS) $@.c > $@.1
	$(AWK) -f "${srcdir}/scripts/dfn.awk" out="$@.2" $@.1 1>&2
	mv $@.2 $@

# The .dfn file for pnglibconf.h is machine generated
pnglibconf.dfn: scripts/pnglibconf.dfa scripts/options.awk pngconf.h pngusr.dfa $(DFA_XTRA)
	rm -f $@ pnglibconf.pre $@.?
	$(AWK) -f ${srcdir}/scripts/options.awk out="pnglibconf.pre"\
	    version=search ${srcdir}/pngconf.h ${srcdir}/scripts/pnglibconf.dfa\
	    ${srcdir}/pngusr.dfa $(DFA_XTRA) 1>&2
	$(AWK) -f ${srcdir}/scripts/options.awk out="$@.1" pnglibconf.pre 1>&2
	mv $@.1 $@

# Symbol checks (.def and .out files should match)
scripts/symbols.chk: scripts/checksym.awk scripts/symbols.def scripts/symbols.out
.out.chk:
	rm -f $@ symbols.new
	$(AWK) -f ${srcdir}/scripts/checksym.awk ${srcdir}/scripts/${*F}.def\
	    $< >&2
	mv symbols.new $@

# used on demand to regenerate the standard header, CPPFLAGS should
# be empty - no non-standard defines
scripts/pnglibconf.dfn: scripts/pnglibconf.dfa scripts/options.awk pngconf.h
	rm -f $@ dfn?.out
	test -z "$(CPPFLAGS)"
	echo "com @PNGLIB_VERSION@ STANDARD API DEFINITION" |\
	$(AWK) -f ${srcdir}/scripts/options.awk out=dfn1.out logunsupported=1\
		version=search ${srcdir}/pngconf.h -\
		${srcdir}/scripts/pnglibconf.dfa 1>&2
	$(AWK) -f ${srcdir}/scripts/options.awk out=dfn2.out dfn1.out 1>&2
	rm dfn1.out
	mv dfn2.out $@

$(libpng@PNGLIB_MAJOR@@PNGLIB_MINOR@_la_OBJECTS): png.h pngconf.h \
	pnglibconf.h pngpriv.h pngdebug.h pnginfo.h pngstruct.h

test: check-am

# Extra checks
check: scripts/symbols.chk

# Don't distribute the generated script files
dist-hook:
	cd '$(top_distdir)'; rm -f $(SCRIPT_CLEANFILES)

# Make links between installed files with release-specific names and the generic
# file names.  If this install rule is run the generic names will be deleted and
# recreated - this has obvious issues for systems with multiple installations.

install-header-links:
	@set -ex; cd '$(DESTDIR)$(includedir)'; for f in $(HEADERS); do \
	   rm -f "$$f"; $(LN_S) "$(PNGLIB_BASENAME)/$$f" "$$f"; done

uninstall-header-links:
	cd '$(DESTDIR)$(includedir)'; rm -f $(HEADERS)

install-libpng-pc:
	@set -ex; cd '$(DESTDIR)$(pkgconfigdir)'; rm -f libpng.pc; \
	   $(LN_S) '$(PNGLIB_BASENAME).pc' libpng.pc

uninstall-libpng-pc:
	rm -f '$(DESTDIR)$(pkgconfigdir)/libpng.pc'

# EXT_LIST is a list of the possibly library directory extensions, this exists
# because we can't find a good way of discovering the file extensions that are
# actually installed on a given system, so instead we check for every extension
# we have seen.

EXT_LIST = a dll.a so so.@PNGLIB_MAJOR@@PNGLIB_MINOR@.@PNGLIB_RELEASE@ la sl dylib

install-library-links:
	@set -x; cd '$(DESTDIR)$(libdir)';\
	for ext in $(EXT_LIST); do\
	   rm -f "libpng.$$ext";\
           if test -f "$(PNGLIB_BASENAME).$$ext"; then\
              $(LN_S) "$(PNGLIB_BASENAME).$$ext" "libpng.$$ext" || exit 1;\
           fi;\
	done

uninstall-library-links:
	@set -x; cd '$(DESTDIR)$(libdir)'; for ext in $(EXT_LIST); do\
	   rm -f "libpng.$$ext"; done

install-libpng-config:
	@set -ex; cd '$(DESTDIR)$(bindir)'; rm -f libpng-config; \
	   $(LN_S) '$(PNGLIB_BASENAME)-config' libpng-config

uninstall-libpng-config:
	rm -f '$(DESTDIR)$(bindir)/libpng-config'

if DO_INSTALL_LINKS
# If --enable-unversioned-links is specified the header and lib file links
# will be automatically made on a 'make install':

install-data-hook: install-header-links
uninstall-hook: uninstall-header-links
install-exec-hook: install-library-links
uninstall-hook: uninstall-library-links
endif

if DO_INSTALL_LIBPNG_PC
# Likewise, --install-pc causes libpng.pc to be constructed:

install-data-hook: install-libpng-pc
uninstall-hook: uninstall-libpng-pc
endif

if DO_INSTALL_LIBPNG_CONFIG
# And --install-config:

install-exec-hook: install-libpng-config
uninstall-hook: uninstall-libpng-config
endif

# The following addition ensures that 'make all' always builds the test programs
# too.  It used to, but some change either in libpng or configure stopped this
# working.
all-am: $(check_PROGRAMS)
