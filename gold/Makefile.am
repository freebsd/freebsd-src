# Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS =

SUBDIRS = po testsuite

tooldir = $(exec_prefix)/$(target_alias)

ACLOCAL_AMFLAGS = -I ../bfd -I ../config

AM_CXXFLAGS = $(WARN_CXXFLAGS) $(LFS_CXXFLAGS)

INCLUDES = -D_GNU_SOURCE \
	-I$(srcdir) -I$(srcdir)/../include -I$(srcdir)/../elfcpp \
	-DLOCALEDIR="\"$(datadir)/locale\"" \
	@INCINTL@

YFLAGS = -d

noinst_PROGRAMS = ld-new
noinst_LIBRARIES = libgold.a

CCFILES = \
	archive.cc \
	common.cc \
	defstd.cc \
	dirsearch.cc \
	dynobj.cc \
	fileread.cc \
	gold.cc \
	gold-threads.cc \
	layout.cc \
	merge.cc \
	object.cc \
	options.cc \
	output.cc \
	readsyms.cc \
	reloc.cc \
	resolve.cc \
	script.cc \
	symtab.cc \
	stringpool.cc \
	target-select.cc \
	workqueue.cc

HFILES = \
	archive.h \
	common.h \
	defstd.h \
	dirsearch.h \
	dynobj.h \
	fileread.h \
	gold.h \
	gold-threads.h \
	layout.h \
	merge.h \
	object.h \
	options.h \
	output.h \
	readsyms.h \
	reloc.h \
	reloc-types.h \
	script.h \
	script-c.h \
	stringpool.h \
	symtab.h \
	target.h \
	target-reloc.h \
	target-select.h \
	workqueue.h

TARGETFILES = \
	i386.cc

YFILES = \
	yyscript.y

EXTRA_DIST = yyscript.c yyscript.h

libgold_a_SOURCES = $(CCFILES) $(HFILES) $(YFILES)

ld_new_SOURCES = main.cc $(TARGETFILES)
ld_new_DEPENDENCIES = libgold.a $(LIBINTL_DEP)
ld_new_LDADD = libgold.a $(LIBINTL)

# Use an explicit dependency for the bison generated header file.
script.$(OBJEXT): yyscript.h

# We have to build libgold.a before we run the tests.
check: libgold.a

.PHONY: install-exec-local

install-exec-local: ld-new$(EXEEXT)
	$(mkinstalldirs) $(DESTDIR)$(bindir) $(DESTDIR)$(tooldir)/bin
	n=`echo ld | sed '$(transform)'; \
	$(INSTALL_PROGRAM) ld-new$(EXEEXT) $(DESTDIR)$(bindir)/$${n}$(EXEEXT); \
	if test "$(bindir)" != "$(tooldir)/bin"; then \
	  rm -f $(DESTDIR)$(tooldir)/bin/ld$(EXEEXT); \
	  ln $(DESTDIR)$(bindir)/$${n}$(EXEEXT) $(DESTDIR)$(tooldir)/bin/ld$(EXEEXT) >/dev/null 2>/dev/null \
	    || $(INSTALL_PROGRAM) ld-new$(EXEEXT) $(DESTDIR)$(tooldir)/bin/ld$(EXEEXT); \
	fi

# We want install to imply install-info as per GNU standards, despite
# the cygnus option.
install-data-local: install-info

POTFILES= $(CCFILES) $(HFILES) $(TARGETFILES)

po/POTFILES.in: @MAINT@ Makefile
	for f in $(POTFILES); do echo $$f; done | LC_COLLATE= sort > tmp \
	  && mv tmp $(srcdir)/po/POTFILES.in
