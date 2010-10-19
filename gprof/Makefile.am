## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

SUFFIXES = .m

SUBDIRS = po

BASEDIR = $(srcdir)/..
BFDDIR = $(BASEDIR)/bfd
INCDIR	= $(BASEDIR)/include

WARN_CFLAGS = @WARN_CFLAGS@
NO_WERROR = @NO_WERROR@
AM_CFLAGS = $(WARN_CFLAGS)

MKDEP = gcc -MM

INCLUDES = -D_GNU_SOURCE -DDEBUG -I../bfd -I$(srcdir)/../include -I$(srcdir)/../bfd -I$(srcdir)/../intl -I../intl -I. -DLOCALEDIR="\"$(datadir)/locale\""

bin_PROGRAMS = gprof

## Convenience var listing pure sources.
sources = basic_blocks.c call_graph.c cg_arcs.c cg_dfn.c \
	cg_print.c corefile.c gmon_io.c gprof.c hertz.c hist.c source.c \
	search_list.c symtab.c sym_ids.c utils.c \
	i386.c alpha.c vax.c tahoe.c sparc.c mips.c
gprof_SOURCES = $(sources) flat_bl.c bsd_callg_bl.c fsf_callg_bl.c
gprof_DEPENDENCIES = ../bfd/libbfd.la ../libiberty/libiberty.a $(INTLDEPS)
gprof_LDADD = ../bfd/libbfd.la ../libiberty/libiberty.a $(INTLLIBS)

noinst_HEADERS = \
	basic_blocks.h call_graph.h cg_arcs.h cg_dfn.h cg_print.h \
	corefile.h gmon.h gmon_io.h gmon_out.h gprof.h hertz.h hist.h \
	search_list.h source.h sym_ids.h symtab.h utils.h

BUILT_SOURCES = flat_bl.c bsd_callg_bl.c fsf_callg_bl.c
EXTRA_DIST    = $(BUILT_SOURCES) bbconv.pl $(man_MANS)

diststuff: $(BUILT_SOURCES) info $(man_MANS)

.m.c:
	awk -f $(srcdir)/gen-c-prog.awk > ./$*.c \
	    FUNCTION=`(echo $*|sed -e 's,.*/,,g' -e 's/_bl//')`_blurb \
	    FILE=$*.m $(srcdir)/$*.m

POTFILES = $(sources) $(noinst_HEADERS)
po/POTFILES.in: @MAINT@ Makefile
	for f in $(POTFILES); do echo $$f; done | LC_COLLATE= sort > tmp \
	  && mv tmp $(srcdir)/po/POTFILES.in

MANCONF  = -Dman

TEXI2POD = perl $(srcdir)/../etc/texi2pod.pl

POD2MAN = pod2man --center="GNU" --release="binutils-$(VERSION)" --section=1

info_TEXINFOS = gprof.texi
man_MANS = gprof.1

$(srcdir)/gprof.info: gprof.texi config.texi

config.texi:
	echo "@set top_srcdir $(top_srcdir)" >> ./config.texi

# Build the man page from the texinfo file
# The sed command removes the no-adjust Nroff command so that
# the man output looks standard.
gprof.1: $(srcdir)/gprof.texi config.texi
	touch $@
	-$(TEXI2POD) $(MANCONF) -Dgprof < $(srcdir)/gprof.texi > gprof.pod
	-($(POD2MAN) gprof.pod | \
	       sed -e '/^.if n .na/d' > $@.T$$$$ && \
	       mv -f $@.T$$$$ $@) || \
	       (rm -f $@.T$$$$ && exit 1)
	rm -f gprof.pod

.PHONY: install-html install-html-am install-html-recursive

html__strip_dir = `echo $$p | sed -e 's|^.*/||'`;

install-html: install-html-recursive  install-html-am

install-html-am: $(HTMLS)
	@$(NORMAL_INSTALL)
	test -z "$(htmldir)" || $(mkdir_p) "$(DESTDIR)$(htmldir)"
	@list='$(HTMLS)'; for p in $$list; do \
	  if test -f "$$p" || test -d "$$p"; then d=""; else d="$(srcdir)/"; fi; \
	  f=$(html__strip_dir) \
	  if test -d "$$d$$p"; then \
	    echo " $(mkdir_p) '$(DESTDIR)$(htmldir)/$$f'"; \
	    $(mkdir_p) "$(DESTDIR)$(htmldir)/$$f" || exit 1; \
	    echo " $(INSTALL_DATA) '$$d$$p'/* '$(DESTDIR)$(htmldir)/$$f'"; \
	    $(INSTALL_DATA) "$$d$$p"/* "$(DESTDIR)$(htmldir)/$$f"; \
	  else \
	    echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(htmldir)/$$f'"; \
	    $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(htmldir)/$$f"; \
	  fi; \
	done

install-html-recursive:
	@failcom='exit 1'; \
	for f in x $$MAKEFLAGS; do \
	  case $$f in \
	    *=* | --[!k]*);; \
	    *k*) failcom='fail=yes';; \
	  esac; \
	done; \
	dot_seen=no; \
	target=`echo $@ | sed s/-recursive//`; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then \
	    dot_seen=yes; \
	    local_target="$$target-am"; \
	  else \
	    local_target="$$target"; \
	  fi; \
	  (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $$local_target) \
	  || eval $$failcom; \
	done; \
	if test "$$dot_seen" = "no"; then \
	  $(MAKE) $(AM_MAKEFLAGS) "$$target-am" || exit 1; \
	fi; test -z "$$fail"

# We want install to imply install-info as per GNU standards, despite the
# cygnus option.
install-data-local: install-info

# Targets to rebuild dependencies in this Makefile.
# Have to get rid of DEP1 here so that "$?" later includes all sources.
DEP: dep.sed $(gprof_SOURCES) $(noinst_HEADERS) gconfig.h
	rm -f DEP1
	$(MAKE) MKDEP="$(MKDEP)" DEP1
	sed -f dep.sed < DEP1 > DEPA
	echo '# IF YOU PUT ANYTHING HERE IT WILL GO AWAY' >> DEPA
	if grep ' /' DEPA > /dev/null 2> /dev/null; then \
	  echo 'make DEP failed!'; exit 1; \
	else \
	  mv -f DEPA $@; \
	fi

DEP1: $(gprof_SOURCES)
	echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > DEP2
	echo '# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.' >> DEP2
	$(MKDEP) $(INCLUDES) $(CFLAGS) $? >> DEP2
	mv -f DEP2 $@

dep.sed: dep-in.sed config.status
	objdir=`pwd`; \
	sed <$(srcdir)/dep-in.sed >dep.sed	\
		-e 's!@INCDIR@!$(INCDIR)!'	\
		-e 's!@BFDDIR@!$(BFDDIR)!'	\
		-e 's!@SRCDIR@!$(srcdir)!'	\
		-e "s!@OBJDIR@!$${objdir}!"	\
		-e 's!@TOPDIR@!'`echo $(srcdir) | sed -e s,/gprof$$,,`'!'

dep: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < Makefile > tmp-Makefile
	cat DEP >> tmp-Makefile
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile Makefile

dep-in: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.in > tmp-Makefile.in
	cat DEP >> tmp-Makefile.in
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile.in $(srcdir)/Makefile.in

dep-am: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.am > tmp-Makefile.am
	cat DEP >> tmp-Makefile.am
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile.am $(srcdir)/Makefile.am

.PHONY: dep dep-in dep-am

CLEANFILES = dep.sed DEP DEPA DEP1 DEP2

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
basic_blocks.o: basic_blocks.c $(INCDIR)/libiberty.h \
  $(INCDIR)/ansidecl.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h ../bfd/bfd.h \
  $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h basic_blocks.h \
  corefile.h gmon_io.h gmon_out.h search_list.h source.h \
  symtab.h sym_ids.h
call_graph.o: call_graph.c gprof.h $(BFDDIR)/sysdep.h \
  $(INCDIR)/ansidecl.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h ../bfd/bfd.h $(INCDIR)/symcat.h \
  gconfig.h $(INCDIR)/bin-bugs.h search_list.h source.h \
  symtab.h cg_arcs.h call_graph.h corefile.h gmon_io.h \
  gmon_out.h sym_ids.h
cg_arcs.o: cg_arcs.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h ../bfd/bfd.h $(INCDIR)/symcat.h \
  gconfig.h $(INCDIR)/bin-bugs.h search_list.h source.h \
  symtab.h call_graph.h cg_arcs.h cg_dfn.h cg_print.h \
  utils.h sym_ids.h
cg_dfn.o: cg_dfn.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h ../bfd/bfd.h $(INCDIR)/symcat.h \
  gconfig.h $(INCDIR)/bin-bugs.h search_list.h source.h \
  symtab.h cg_arcs.h cg_dfn.h utils.h
cg_print.o: cg_print.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h ../bfd/bfd.h $(INCDIR)/symcat.h \
  gconfig.h $(INCDIR)/bin-bugs.h search_list.h source.h \
  symtab.h cg_arcs.h cg_print.h hist.h utils.h corefile.h
corefile.o: corefile.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h ../bfd/bfd.h $(INCDIR)/symcat.h \
  gconfig.h $(INCDIR)/bin-bugs.h search_list.h source.h \
  symtab.h corefile.h
gmon_io.o: gmon_io.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h basic_blocks.h \
  corefile.h call_graph.h gmon_io.h gmon_out.h gmon.h \
  hertz.h hist.h $(INCDIR)/libiberty.h
gprof.o: gprof.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h ../bfd/bfd.h $(INCDIR)/symcat.h \
  gconfig.h $(INCDIR)/bin-bugs.h search_list.h source.h \
  symtab.h basic_blocks.h call_graph.h cg_arcs.h cg_print.h \
  corefile.h gmon_io.h hertz.h hist.h sym_ids.h $(INCDIR)/demangle.h
hertz.o: hertz.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  hertz.h
hist.o: hist.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h $(INCDIR)/fopen-same.h \
  $(INCDIR)/filenames.h ../bfd/bfd.h $(INCDIR)/symcat.h \
  gconfig.h $(INCDIR)/bin-bugs.h search_list.h source.h \
  symtab.h corefile.h gmon_io.h gmon_out.h hist.h sym_ids.h \
  utils.h
source.o: source.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  $(INCDIR)/libiberty.h search_list.h source.h
search_list.o: search_list.c $(INCDIR)/libiberty.h \
  $(INCDIR)/ansidecl.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h ../bfd/bfd.h \
  $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h search_list.h
symtab.o: symtab.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h corefile.h
sym_ids.o: sym_ids.c $(INCDIR)/libiberty.h $(INCDIR)/ansidecl.h \
  $(INCDIR)/safe-ctype.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h ../bfd/bfd.h \
  $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h search_list.h \
  source.h symtab.h cg_arcs.h sym_ids.h
utils.o: utils.c $(INCDIR)/demangle.h $(INCDIR)/libiberty.h \
  $(INCDIR)/ansidecl.h gprof.h $(BFDDIR)/sysdep.h ../bfd/config.h \
  $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h ../bfd/bfd.h \
  $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h search_list.h \
  source.h symtab.h cg_arcs.h utils.h
i386.o: i386.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h corefile.h \
  hist.h
alpha.o: alpha.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h corefile.h \
  hist.h
vax.o: vax.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h corefile.h \
  hist.h
tahoe.o: tahoe.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h corefile.h \
  hist.h
sparc.o: sparc.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h corefile.h \
  hist.h
mips.o: mips.c gprof.h $(BFDDIR)/sysdep.h $(INCDIR)/ansidecl.h \
  ../bfd/config.h $(INCDIR)/fopen-same.h $(INCDIR)/filenames.h \
  ../bfd/bfd.h $(INCDIR)/symcat.h gconfig.h $(INCDIR)/bin-bugs.h \
  search_list.h source.h symtab.h cg_arcs.h corefile.h \
  hist.h
flat_bl.o: flat_bl.c $(INCDIR)/ansidecl.h
bsd_callg_bl.o: bsd_callg_bl.c $(INCDIR)/ansidecl.h
fsf_callg_bl.o: fsf_callg_bl.c $(INCDIR)/ansidecl.h
# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
