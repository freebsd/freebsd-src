## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

INCDIR = $(srcdir)/../include
CSEARCH = -I. -I$(srcdir) -I$(INCDIR)
MKDEP = gcc -MM

SUBDIRS = doc po

docdir = doc

lib_LTLIBRARIES = libbfd.la

# bfd.h goes here, for now
BFD_H = bfd.h

# Jim Kingdon notes:
# Writing S-records should be included in all (or at least most)
# *-*-coff, *-*-aout, etc., configurations, because people will want to
# be able to use objcopy to create S-records.  (S-records are not useful
# for the debugger, so if you are downloading things as S-records you
# need two copies of the executable, one to download and one for the
# debugger).
BFD_LIBS = \
	archive.lo archures.lo bfd.lo cache.lo coffgen.lo corefile.lo \
	format.lo init.lo libbfd.lo opncls.lo reloc.lo \
	section.lo syms.lo targets.lo hash.lo linker.lo \
	srec.lo binary.lo tekhex.lo ihex.lo stabs.lo stab-syms.lo

BFD_LIBS_CFILES = \
	archive.c archures.c bfd.c cache.c coffgen.c corefile.c \
	format.c init.c libbfd.c opncls.c reloc.c \
	section.c syms.c targets.c hash.c linker.c \
	srec.c binary.c tekhex.c ihex.c stabs.c stab-syms.c

# This list is alphabetized to make it easier to keep in sync
# with the decls and initializer in archures.c.
ALL_MACHINES = \
	cpu-a29k.lo \
	cpu-alpha.lo \
	cpu-arc.lo \
	cpu-arm.lo \
	cpu-avr.lo \
	cpu-d10v.lo \
	cpu-d30v.lo \
	cpu-fr30.lo \
	cpu-h8300.lo \
	cpu-h8500.lo \
	cpu-hppa.lo \
	cpu-i370.lo \
	cpu-i386.lo \
	cpu-i860.lo \
	cpu-i960.lo \
	cpu-m32r.lo \
	cpu-m68k.lo \
	cpu-m88k.lo \
	cpu-m10200.lo \
	cpu-m10300.lo \
	cpu-mcore.lo \
	cpu-mips.lo \
	cpu-ns32k.lo \
	cpu-pj.lo \
	cpu-powerpc.lo \
	cpu-rs6000.lo \
	cpu-sh.lo \
	cpu-sparc.lo \
	cpu-tic30.lo \
	cpu-tic80.lo \
	cpu-v850.lo \
	cpu-vax.lo \
	cpu-we32k.lo \
	cpu-w65.lo \
	cpu-z8k.lo

ALL_MACHINES_CFILES = \
	cpu-a29k.c \
	cpu-alpha.c \
	cpu-arc.c \
	cpu-arm.c \
	cpu-avr.c \
	cpu-d10v.c \
	cpu-d30v.c \
	cpu-fr30.c \
	cpu-h8300.c \
	cpu-h8500.c \
	cpu-hppa.c \
	cpu-i370.c \
	cpu-i386.c \
	cpu-i860.c \
	cpu-i960.c \
	cpu-m32r.c \
	cpu-m68k.c \
	cpu-m88k.c \
	cpu-m10200.c \
	cpu-m10300.c \
	cpu-mcore.c \
	cpu-mips.c \
	cpu-ns32k.c \
	cpu-pj.c \
	cpu-powerpc.c \
	cpu-rs6000.c \
	cpu-sh.c \
	cpu-sparc.c \
	cpu-tic30.c \
	cpu-tic80.c \
	cpu-v850.c \
	cpu-vax.c \
	cpu-we32k.c \
	cpu-w65.c \
	cpu-z8k.c

# The .o files needed by all of the 32 bit vectors that are configured into
# target_vector in targets.c if configured with --enable-targets=all.
BFD32_BACKENDS = \
	aout-adobe.lo \
	aout-arm.lo \
	aout-ns32k.lo \
	aout-sparcle.lo \
	aout-tic30.lo \
	aout0.lo \
	aout32.lo \
	armnetbsd.lo \
	bout.lo \
	cf-i386lynx.lo \
	cf-m68klynx.lo \
	cf-sparclynx.lo \
	coff-a29k.lo \
	coff-apollo.lo \
	coff-arm.lo \
	coff-aux.lo \
	coff-h8300.lo \
	coff-h8500.lo \
	coff-i386.lo \
	coff-go32.lo \
	coff-i860.lo \
	coff-i960.lo \
	coff-m68k.lo \
	coff-m88k.lo \
	coff-mips.lo \
	coff-pmac.lo \
	coff-rs6000.lo \
	coff-sh.lo \
	coff-sparc.lo \
	coff-stgo32.lo \
	coff-svm68k.lo \
	coff-tic30.lo \
	coff-tic80.lo \
	coff-u68k.lo \
	coff-we32k.lo \
	coff-w65.lo \
	coff-z8k.lo \
	cofflink.lo \
	dwarf1.lo \
	dwarf2.lo \
	ecoff.lo \
	ecofflink.lo \
	elf.lo \
	elf32-arc.lo \
	elfarm-oabi.lo \
	elfarm-nabi.lo \
	elf32-avr.lo \
	elf32-d10v.lo \
	elf32-d30v.lo \
	elf32-fr30.lo \
	elf32-gen.lo \
	elf32-i370.lo \
	elf32-i386.lo \
	elf32-i860.lo \
	elf32-i960.lo \
	elf32-m32r.lo \
	elf32-m68k.lo \
	elf32-m88k.lo \
	elf-m10200.lo \
	elf-m10300.lo \
	elf32-mcore.lo \
	elf32-mips.lo \
	elf32-pj.lo \
	elf32-ppc.lo \
	elf32-sh.lo \
	elf32-sparc.lo \
	elf32-v850.lo \
	elf32.lo \
	elflink.lo \
	epoc-pe-arm.lo \
	epoc-pei-arm.lo \
	hp300bsd.lo \
	hp300hpux.lo \
	som.lo \
	i386aout.lo \
	i386bsd.lo \
	i386dynix.lo \
	i386freebsd.lo \
	i386linux.lo \
	i386lynx.lo \
	i386msdos.lo \
	i386netbsd.lo \
	i386mach3.lo \
	i386os9k.lo \
	ieee.lo \
	m68k4knetbsd.lo \
	m68klinux.lo \
	m68klynx.lo \
	m68knetbsd.lo \
	m88kmach3.lo \
	mipsbsd.lo \
	newsos3.lo \
	nlm.lo \
	nlm32-i386.lo \
	nlm32-sparc.lo \
	nlm32-ppc.lo \
	nlm32.lo \
	ns32knetbsd.lo \
	oasys.lo \
	pc532-mach.lo \
	pe-arm.lo \
	pei-arm.lo \
	pe-i386.lo \
	pei-i386.lo \
	pe-mcore.lo \
	pei-mcore.lo \
	pe-ppc.lo \
	pei-ppc.lo \
	pe-sh.lo \
	pei-sh.lo \
	pe-mips.lo \
	pei-mips.lo \
	peigen.lo \
	ppcboot.lo \
	reloc16.lo \
	riscix.lo \
	sparclinux.lo \
	sparclynx.lo \
	sparcnetbsd.lo \
	sunos.lo \
	vaxnetbsd.lo \
	versados.lo \
	vms.lo \
	vms-gsd.lo \
	vms-hdr.lo \
	vms-misc.lo \
	vms-tir.lo \
	xcofflink.lo

BFD32_BACKENDS_CFILES = \
	aout-adobe.c \
	aout-arm.c \
	aout-ns32k.c \
	aout-sparcle.c \
	aout-tic30.c \
	aout0.c \
	aout32.c \
	armnetbsd.c \
	bout.c \
	cf-i386lynx.c \
	cf-m68klynx.c \
	cf-sparclynx.c \
	coff-a29k.c \
	coff-apollo.c \
	coff-arm.c \
	coff-aux.c \
	coff-h8300.c \
	coff-h8500.c \
	coff-i386.c \
	coff-i860.c \
	coff-go32.c \
	coff-i960.c \
	coff-m68k.c \
	coff-m88k.c \
	coff-mips.c \
	coff-pmac.c \
	coff-rs6000.c \
	coff-sh.c \
	coff-sparc.c \
	coff-stgo32.c \
	coff-svm68k.c \
	coff-tic30.c \
	coff-tic80.c \
	coff-u68k.c \
	coff-we32k.c \
	coff-w65.c \
	coff-z8k.c \
	cofflink.c \
	dwarf1.c \
	dwarf2.c \
	ecoff.c \
	ecofflink.c \
	elf.c \
	elf32-arc.c \
	elfarm-oabi.c \
	elfarm-nabi.c \
	elf32-avr.c \
	elf32-d10v.c \
	elf32-d30v.c \
	elf32-fr30.c \
	elf32-gen.c \
	elf32-i370.c \
	elf32-i386.c \
	elf32-i860.c \
	elf32-i960.c \
	elf32-m32r.c \
	elf32-m68k.c \
	elf32-m88k.c \
	elf-m10200.c \
	elf-m10300.c \
	elf32-mcore.c \
	elf32-mips.c \
	elf32-pj.c \
	elf32-ppc.c \
	elf32-sh.c \
	elf32-sparc.c \
	elf32-v850.c \
	elf32.c \
	elflink.c \
	epoc-pe-arm.c \
	epoc-pei-arm.c \
	hp300bsd.c \
	hp300hpux.c \
	som.c \
	i386aout.c \
	i386bsd.c \
	i386dynix.c \
	i386freebsd.c \
	i386linux.c \
	i386lynx.c \
	i386msdos.c \
	i386netbsd.c \
	i386mach3.c \
	i386os9k.c \
	ieee.c \
	m68k4knetbsd.c \
	m68klinux.c \
	m68klynx.c \
	m68knetbsd.c \
	m88kmach3.c \
	mipsbsd.c \
	newsos3.c \
	nlm.c \
	nlm32-i386.c \
	nlm32-sparc.c \
	nlm32-ppc.c \
	nlm32.c \
	ns32knetbsd.c \
	oasys.c \
	pc532-mach.c \
	pe-arm.c \
	pei-arm.c \
	pe-i386.c \
	pei-i386.c \
	pe-mcore.c \
	pei-mcore.c \
	pe-ppc.c \
	pei-ppc.c \
	pe-sh.c \
	pei-sh.c \
	pe-mips.c \
	pei-mips.c \
	peigen.c \
	ppcboot.c \
	reloc16.c \
	riscix.c \
	sparclinux.c \
	sparclynx.c \
	sparcnetbsd.c \
	sunos.c \
	vaxnetbsd.c \
	versados.c \
	vms.c \
	vms-gsd.c \
	vms-hdr.c \
	vms-misc.c \
	vms-tir.c \
	xcofflink.c

# The .o files needed by all of the 64 bit vectors that are configured into
# target_vector in targets.c if configured with --enable-targets=all
# and --enable-64-bit-bfd.
BFD64_BACKENDS = \
	aout64.lo \
	coff-alpha.lo \
	demo64.lo \
	elf64-alpha.lo \
	elf64-gen.lo \
	elf64-mips.lo \
	elf64-sparc.lo \
	elf64.lo \
	nlm32-alpha.lo \
	nlm64.lo

BFD64_BACKENDS_CFILES = \
	aout64.c \
	coff-alpha.c \
	demo64.c \
	elf64-alpha.c \
	elf64-gen.c \
	elf64-mips.c \
	elf64-sparc.c \
	elf64.c \
	nlm32-alpha.c \
	nlm64.c

OPTIONAL_BACKENDS = \
	aix386-core.lo \
	hpux-core.lo \
	irix-core.lo \
	lynx-core.lo \
	osf-core.lo \
	sco5-core.lo \
	trad-core.lo \
	cisco-core.lo

OPTIONAL_BACKENDS_CFILES = \
	aix386-core.c \
	hpux-core.c \
	irix-core.c \
	lynx-core.c \
	osf-core.c \
	sco5-core.c \
	trad-core.c \
	cisco-core.c

# These are defined by configure.in:
WORDSIZE = @wordsize@
ALL_BACKENDS = @all_backends@
BFD_BACKENDS = @bfd_backends@
BFD_MACHINES = @bfd_machines@
TDEFAULTS = @tdefaults@

INCLUDES = -D_GNU_SOURCE @HDEFINES@ @COREFLAG@ @TDEFINES@ $(CSEARCH) $(CSWITCHES) -I$(srcdir)/../intl -I../intl

# C source files that correspond to .o's.
CFILES = \
	$(BFD_LIBS_CFILES) \
	$(ALL_MACHINES_CFILES) \
	$(BFD32_BACKENDS_CFILES) \
	$(BFD64_BACKENDS_CFILES) \
	$(OPTIONAL_BACKENDS_CFILES)

## This is a list of all .h files which are in the source tree.
SOURCE_HFILES = \
	aout-target.h aoutf1.h aoutx.h coffcode.h coffswap.h \
	ecoffswap.h elf32-arm.h elfcode.h elfcore.h \
	elflink.h freebsd.h genlink.h go32stub.h libaout.h \
	libbfd.h libcoff.h libecoff.h elf-bfd.h libhppa.h libieee.h \
	libnlm.h liboasys.h libpei.h netbsd.h nlm-target.h nlmcode.h \
	nlmswap.h ns32k.h peicode.h som.h vms.h

HFILES = \
	elf32-target.h elf64-target.h targmatch.h \
	$(SOURCE_HFILES)

POTFILES = $(CFILES) $(SOURCE_HFILES)

po/POTFILES.in: @MAINT@ Makefile
	for file in $(POTFILES); do echo $$file; done | sort > tmp \
	  && mv tmp $(srcdir)/po/POTFILES.in

diststuff: info

# Various kinds of .o files to put in libbfd.a:
# BFD_BACKENDS	Routines the configured targets need.
# BFD_MACHINES	Architecture-specific routines the configured targets need.
# COREFILE	Core file routines for a native configuration
OFILES = $(BFD_BACKENDS) $(BFD_MACHINES) @COREFILE@

stamp-ofiles: Makefile
	rm -f tofiles
	f=""; \
	for i in $(OFILES) ; do \
	  case " $$f " in \
	    *" $$i "*) ;; \
	    *) f="$$f $$i" ;; \
	  esac ; \
	done ; \
	echo $$f > tofiles
	$(SHELL) $(srcdir)/../move-if-change tofiles ofiles
	touch stamp-ofiles

ofiles: stamp-ofiles ; @true

libbfd_la_SOURCES = $(BFD_LIBS_CFILES)
libbfd_la_DEPENDENCIES = $(OFILES) ofiles
libbfd_la_LIBADD = `cat ofiles` @WIN32LIBADD@
libbfd_la_LDFLAGS = -release $(VERSION) @WIN32LDFLAGS@

# libtool will build .libs/libbfd.a.  We create libbfd.a in the build
# directory so that we don't have to convert all the programs that use
# libbfd.a simultaneously.  This is a hack which should be removed if
# everything else starts using libtool.  FIXME.

noinst_LIBRARIES = libbfd.a
libbfd_a_SOURCES =

stamp-lib: libbfd.la
	libtooldir=`$(LIBTOOL) --config | sed -n -e 's/^objdir=//p'`; \
	if [ -f $$libtooldir/libbfd.a ]; then \
	  cp $$libtooldir/libbfd.a libbfd.tmp; \
	  $(SHELL) $(srcdir)/../move-if-change libbfd.tmp libbfd.a; \
	else true; fi
	touch stamp-lib

libbfd.a: stamp-lib ; @true

# This file holds an array associating configuration triplets and
# vector names.  It is built from config.bfd.  It is not compiled by
# itself, but is included by targets.c.
targmatch.h: config.bfd targmatch.sed
	rm -f targmatch.h
	sed -f $(srcdir)/targmatch.sed < $(srcdir)/config.bfd > targmatch.new
	mv -f targmatch.new targmatch.h

# When compiling archures.c and targets.c, supply the default target
# info from configure.

targets.lo: targets.c config.status
	$(LIBTOOL) --mode=compile $(COMPILE) -c $(TDEFAULTS) $(srcdir)/targets.c

archures.lo: archures.c config.status
	$(LIBTOOL) --mode=compile $(COMPILE) -c $(TDEFAULTS) $(srcdir)/archures.c

elf32-target.h : elfxx-target.h
	rm -f elf32-target.h
	sed -e s/NN/32/g < $(srcdir)/elfxx-target.h > elf32-target.new
	mv -f elf32-target.new elf32-target.h

elf64-target.h : elfxx-target.h
	rm -f elf64-target.h
	sed -e s/NN/64/g < $(srcdir)/elfxx-target.h > elf64-target.new
	mv -f elf64-target.new elf64-target.h

BFD_H_DEPS= $(INCDIR)/ansidecl.h
LOCAL_H_DEPS= libbfd.h sysdep.h config.h
$(BFD_LIBS): $(BFD_H) $(BFD_H_DEPS) $(LOCAL_H_DEPS)
$(BFD_MACHINES): $(BFD_H) $(BFD_H_DEPS) $(LOCAL_H_DEPS)
$(BFD_BACKENDS): $(BFD_H) $(BFD_H_DEPS) $(LOCAL_H_DEPS)
$(OPTIONAL_BACKENDS): $(BFD_H) $(BFD_H_DEPS) $(LOCAL_H_DEPS)

# Install BFD include file, and others that it needs.
install-data-local: $(BFD_H)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(includedir)
	$(INSTALL_DATA) $(BFD_H) $(includedir)/bfd.h
	$(INSTALL_DATA) $(INCDIR)/ansidecl.h $(includedir)/ansidecl.h
	$(INSTALL_DATA) $(INCDIR)/bfdlink.h $(includedir)/bfdlink.h

# Have to get rid of DEP1 here so that "$?" later includes all of $(CFILES).
DEP: dep.sed $(CFILES) $(HFILES) bfd.h
	rm -f DEP1
	$(MAKE) MKDEP="$(MKDEP)" DEP1
	sed -f dep.sed < DEP1 > $@
	echo '# IF YOU PUT ANYTHING HERE IT WILL GO AWAY' >> $@

DEP1: $(CFILES)
	echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > DEP2
	echo '# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.' >> DEP2
	$(MKDEP) $(INCLUDES) $(CFLAGS) $? >> DEP2
	mv -f DEP2 $@

dep.sed: dep-in.sed config.status
	sed <$(srcdir)/dep-in.sed >dep.sed	\
		-e 's!@BFD_H@!$(BFD_H)!'	\
		-e 's!@INCDIR@!$(INCDIR)!'	\
		-e 's!@SRCDIR@!$(srcdir)!'

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

host-aout.lo: Makefile

# The following program can be used to generate a simple config file
# which can be folded into an h-XXX file for a new host, with some editing.
aout-params.h: gen-aout
	./gen-aout host > aout-params.h
gen-aout: $(srcdir)/gen-aout.c Makefile
	$(CC) -o gen-aout $(CFLAGS) $(LFLAGS) $(srcdir)/gen-aout.c

$(BFD_H): stmp-bfd-h ; @true

stmp-bfd-h: bfd-in3.h
	rm -f bfd-tmp.h
	cp bfd-in3.h bfd-tmp.h
	$(SHELL) $(srcdir)/../move-if-change bfd-tmp.h $(BFD_H)
	rm -f bfd-tmp.h
	touch stmp-bfd-h

BFD_H_FILES = bfd-in.h init.c opncls.c libbfd.c section.c archures.c \
	reloc.c syms.c bfd.c archive.c corefile.c targets.c format.c
LIBBFD_H_FILES = libbfd-in.h init.c libbfd.c cache.c reloc.c archures.c elf.c
LIBCOFF_H_FILES = libcoff-in.h coffcode.h

# Could really use a "copy-if-change"...
headers:
	(cd $(docdir); $(MAKE) protos $(FLAGS_TO_PASS))
	cp $(docdir)/bfd.h bfd-in2.h-new
	$(SHELL) $(srcdir)/../move-if-change bfd-in2.h-new $(srcdir)/bfd-in2.h
	cp $(docdir)/libbfd.h libbfd.h-new
	$(SHELL) $(srcdir)/../move-if-change libbfd.h-new $(srcdir)/libbfd.h
	cp $(docdir)/libcoff.h libcoff.h-new
	$(SHELL) $(srcdir)/../move-if-change libcoff.h-new $(srcdir)/libcoff.h

# We only rebuild the header files automatically if we have been
# configured with --enable-maintainer-mode.

$(srcdir)/bfd-in2.h: @MAINT@ stmp-bin2-h ; @true
stmp-bin2-h: $(BFD_H_FILES)
	(cd $(docdir); $(MAKE) $(FLAGS_TO_PASS) bfd.h)
	cp $(docdir)/bfd.h bfd-in2.h-new
	$(SHELL) $(srcdir)/../move-if-change bfd-in2.h-new $(srcdir)/bfd-in2.h
	touch stmp-bin2-h

$(srcdir)/libbfd.h: @MAINT@ stmp-lbfd-h ; @true
stmp-lbfd-h: $(LIBBFD_H_FILES)
	(cd $(docdir); $(MAKE) $(FLAGS_TO_PASS) libbfd.h)
	cp $(docdir)/libbfd.h libbfd.h-new
	$(SHELL) $(srcdir)/../move-if-change libbfd.h-new $(srcdir)/libbfd.h
	touch stmp-lbfd-h

$(srcdir)/libcoff.h: @MAINT@ stmp-lcoff-h ; @true
stmp-lcoff-h: $(LIBCOFF_H_FILES)
	(cd $(docdir); $(MAKE) $(FLAGS_TO_PASS) libcoff.h)
	cp $(docdir)/libcoff.h libcoff.h-new
	$(SHELL) $(srcdir)/../move-if-change libcoff.h-new $(srcdir)/libcoff.h
	touch stmp-lcoff-h

MOSTLYCLEANFILES = elf32-target.h elf64-target.h ofiles stamp-ofiles \
	targmatch.h

CLEANFILES = bfd.h dep.sed stmp-bfd-h DEP DEP1 DEP2 libbfd.a stamp-lib \
	stmp-bin2-h stmp-lbfd-h stmp-lcoff-h

# We want to rerun configure if config.bfd or configure.host change.
config.status: $(srcdir)/configure $(srcdir)/config.bfd $(srcdir)/configure.host
	$(SHELL) ./config.status --recheck


elfarm-oabi.lo: elfarm-oabi.c elf32-arm.h elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/arm.h $(INCDIR)/elf/reloc-macros.h elf32-target.h

elfarm-nabi.lo: elfarm-nabi.c elf32-arm.h elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/arm.h $(INCDIR)/elf/reloc-macros.h elf32-target.h

# What appears below is generated by a hacked mkdep using gcc -MM.

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
archive.lo: archive.c $(INCDIR)/aout/ar.h $(INCDIR)/aout/ranlib.h
archures.lo: archures.c
bfd.lo: bfd.c $(INCDIR)/libiberty.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sym.h libcoff.h \
  libecoff.h $(INCDIR)/coff/ecoff.h elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h
cache.lo: cache.c
coffgen.lo: coffgen.c $(INCDIR)/coff/internal.h libcoff.h \
  $(INCDIR)/bfdlink.h
corefile.lo: corefile.c
format.lo: format.c
init.lo: init.c
libbfd.lo: libbfd.c
opncls.lo: opncls.c $(INCDIR)/objalloc.h
reloc.lo: reloc.c $(INCDIR)/bfdlink.h
section.lo: section.c $(INCDIR)/bfdlink.h
syms.lo: syms.c $(INCDIR)/bfdlink.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def
targets.lo: targets.c $(INCDIR)/fnmatch.h targmatch.h
hash.lo: hash.c $(INCDIR)/objalloc.h
linker.lo: linker.c $(INCDIR)/bfdlink.h genlink.h
srec.lo: srec.c $(INCDIR)/libiberty.h
binary.lo: binary.c
tekhex.lo: tekhex.c $(INCDIR)/libiberty.h
ihex.lo: ihex.c $(INCDIR)/libiberty.h
stabs.lo: stabs.c $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def
stab-syms.lo: stab-syms.c libaout.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab.def
cpu-a29k.lo: cpu-a29k.c
cpu-alpha.lo: cpu-alpha.c
cpu-arc.lo: cpu-arc.c
cpu-arm.lo: cpu-arm.c
cpu-avr.lo: cpu-avr.c
cpu-d10v.lo: cpu-d10v.c
cpu-d30v.lo: cpu-d30v.c
cpu-fr30.lo: cpu-fr30.c
cpu-h8300.lo: cpu-h8300.c
cpu-h8500.lo: cpu-h8500.c
cpu-hppa.lo: cpu-hppa.c
cpu-i370.lo: cpu-i370.c
cpu-i386.lo: cpu-i386.c
cpu-i860.lo: cpu-i860.c
cpu-i960.lo: cpu-i960.c
cpu-m32r.lo: cpu-m32r.c
cpu-m68k.lo: cpu-m68k.c
cpu-m88k.lo: cpu-m88k.c
cpu-m10200.lo: cpu-m10200.c
cpu-m10300.lo: cpu-m10300.c
cpu-mcore.lo: cpu-mcore.c
cpu-mips.lo: cpu-mips.c
cpu-ns32k.lo: cpu-ns32k.c ns32k.h
cpu-pj.lo: cpu-pj.c
cpu-powerpc.lo: cpu-powerpc.c
cpu-rs6000.lo: cpu-rs6000.c
cpu-sh.lo: cpu-sh.c
cpu-sparc.lo: cpu-sparc.c
cpu-tic30.lo: cpu-tic30.c
cpu-tic80.lo: cpu-tic80.c
cpu-v850.lo: cpu-v850.c
cpu-vax.lo: cpu-vax.c
cpu-we32k.lo: cpu-we32k.c
cpu-w65.lo: cpu-w65.c
cpu-z8k.lo: cpu-z8k.c
aout-adobe.lo: aout-adobe.c $(INCDIR)/aout/adobe.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def libaout.h \
  $(INCDIR)/bfdlink.h
aout-arm.lo: aout-arm.c libaout.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/aout/aout64.h aoutx.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h aout-target.h
aout-ns32k.lo: aout-ns32k.c $(INCDIR)/aout/aout64.h \
  ns32k.h libaout.h $(INCDIR)/bfdlink.h
aout-sparcle.lo: aout-sparcle.c $(INCDIR)/bfdlink.h \
  libaout.h aoutf1.h $(INCDIR)/aout/sun4.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h \
  aout-target.h
aout-tic30.lo: aout-tic30.c libaout.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def \
  $(INCDIR)/aout/ar.h aoutx.h
aout0.lo: aout0.c aoutf1.h $(INCDIR)/aout/sun4.h libaout.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h aout-target.h
aout32.lo: aout32.c aoutx.h $(INCDIR)/bfdlink.h libaout.h \
  $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def \
  $(INCDIR)/aout/ar.h
armnetbsd.lo: armnetbsd.c netbsd.h libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
bout.lo: bout.c $(INCDIR)/bfdlink.h genlink.h $(INCDIR)/bout.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def libaout.h
cf-i386lynx.lo: cf-i386lynx.c coff-i386.c $(INCDIR)/coff/i386.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
cf-m68klynx.lo: cf-m68klynx.c coff-m68k.c $(INCDIR)/coff/m68k.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
cf-sparclynx.lo: cf-sparclynx.c coff-sparc.c $(INCDIR)/coff/sparc.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-a29k.lo: coff-a29k.c $(INCDIR)/coff/a29k.h $(INCDIR)/coff/internal.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h
coff-apollo.lo: coff-apollo.c $(INCDIR)/coff/apollo.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-arm.lo: coff-arm.c $(INCDIR)/coff/arm.h $(INCDIR)/coff/internal.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h
coff-aux.lo: coff-aux.c $(INCDIR)/coff/aux-coff.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/m68k.h coff-m68k.c libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-h8300.lo: coff-h8300.c $(INCDIR)/bfdlink.h genlink.h \
  $(INCDIR)/coff/h8300.h $(INCDIR)/coff/internal.h libcoff.h \
  coffcode.h coffswap.h
coff-h8500.lo: coff-h8500.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/h8500.h \
  $(INCDIR)/coff/internal.h libcoff.h coffcode.h coffswap.h
coff-i386.lo: coff-i386.c $(INCDIR)/coff/i386.h $(INCDIR)/coff/internal.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h
coff-i860.lo: coff-i860.c $(INCDIR)/coff/i860.h $(INCDIR)/coff/internal.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h
coff-go32.lo: coff-go32.c coff-i386.c $(INCDIR)/coff/i386.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-i960.lo: coff-i960.c $(INCDIR)/coff/i960.h $(INCDIR)/coff/internal.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h
coff-m68k.lo: coff-m68k.c $(INCDIR)/coff/m68k.h $(INCDIR)/coff/internal.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h
coff-m88k.lo: coff-m88k.c $(INCDIR)/coff/m88k.h $(INCDIR)/coff/internal.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h
coff-mips.lo: coff-mips.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/symconst.h $(INCDIR)/coff/ecoff.h \
  $(INCDIR)/coff/mips.h libcoff.h libecoff.h coffswap.h \
  ecoffswap.h
coff-pmac.lo: coff-pmac.c coff-rs6000.c $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/rs6000.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-rs6000.lo: coff-rs6000.c $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/rs6000.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-sh.lo: coff-sh.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/sh.h \
  $(INCDIR)/coff/internal.h libcoff.h coffcode.h coffswap.h
coff-sparc.lo: coff-sparc.c $(INCDIR)/coff/sparc.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-stgo32.lo: coff-stgo32.c coff-i386.c $(INCDIR)/coff/i386.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/go32exe.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h coffswap.h \
  go32stub.h
coff-svm68k.lo: coff-svm68k.c coff-m68k.c $(INCDIR)/coff/m68k.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-tic30.lo: coff-tic30.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/tic30.h \
  $(INCDIR)/coff/internal.h libcoff.h coffcode.h coffswap.h
coff-tic80.lo: coff-tic80.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/tic80.h \
  $(INCDIR)/coff/internal.h libcoff.h coffcode.h coffswap.h
coff-u68k.lo: coff-u68k.c coff-m68k.c $(INCDIR)/coff/m68k.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-we32k.lo: coff-we32k.c $(INCDIR)/coff/we32k.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h \
  coffcode.h coffswap.h
coff-w65.lo: coff-w65.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/w65.h \
  $(INCDIR)/coff/internal.h libcoff.h coffcode.h coffswap.h
coff-z8k.lo: coff-z8k.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/z8k.h \
  $(INCDIR)/coff/internal.h libcoff.h coffcode.h coffswap.h
cofflink.lo: cofflink.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/internal.h \
  libcoff.h
dwarf1.lo: dwarf1.c $(INCDIR)/libiberty.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/elf/dwarf.h
dwarf2.lo: dwarf2.c $(INCDIR)/libiberty.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/elf/dwarf2.h
ecoff.lo: ecoff.c $(INCDIR)/bfdlink.h $(INCDIR)/aout/ar.h \
  $(INCDIR)/aout/ranlib.h $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def \
  libaout.h $(INCDIR)/aout/aout64.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/symconst.h $(INCDIR)/coff/ecoff.h \
  libcoff.h libecoff.h
ecofflink.lo: ecofflink.c $(INCDIR)/bfdlink.h $(INCDIR)/objalloc.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/symconst.h $(INCDIR)/coff/ecoff.h \
  libcoff.h libecoff.h
elf.lo: elf.c $(INCDIR)/bfdlink.h elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h
elf32-arc.lo: elf32-arc.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/arc.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elfarm-oabi.lo: elfarm-oabi.c $(INCDIR)/elf/arm-oabi.h \
  $(INCDIR)/elf/reloc-macros.h elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf32-arm.h elf32-target.h
elfarm-nabi.lo: elfarm-nabi.c $(INCDIR)/elf/arm.h $(INCDIR)/elf/reloc-macros.h \
  elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h elf32-arm.h \
  elf32-target.h
elf32-avr.lo: elf32-avr.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/avr.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-d10v.lo: elf32-d10v.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf32-target.h
elf32-d30v.lo: elf32-d30v.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf32-target.h
elf32-fr30.lo: elf32-fr30.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/fr30.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-gen.lo: elf32-gen.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf32-target.h
elf32-i370.lo: elf32-i370.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/i370.h elf32-target.h
elf32-i386.lo: elf32-i386.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/i386.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-i860.lo: elf32-i860.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf32-target.h
elf32-i960.lo: elf32-i960.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/i960.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-m32r.lo: elf32-m32r.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/m32r.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-m68k.lo: elf32-m68k.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/m68k.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-m88k.lo: elf32-m88k.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf32-target.h
elf-m10200.lo: elf-m10200.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf32-target.h
elf-m10300.lo: elf-m10300.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/mn10300.h $(INCDIR)/elf/reloc-macros.h \
  elf32-target.h
elf32-mcore.lo: elf32-mcore.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/mcore.h $(INCDIR)/elf/reloc-macros.h \
  elf32-target.h
elf32-mips.lo: elf32-mips.c $(INCDIR)/bfdlink.h genlink.h \
  elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/elf/mips.h $(INCDIR)/elf/reloc-macros.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/symconst.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/ecoff.h $(INCDIR)/coff/mips.h ecoffswap.h \
  elf32-target.h
elf32-pj.lo: elf32-pj.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/pj.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-ppc.lo: elf32-ppc.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/ppc.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-sh.lo: elf32-sh.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/sh.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32-sparc.lo: elf32-sparc.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/sparc.h $(INCDIR)/elf/reloc-macros.h \
  elf32-target.h
elf32-v850.lo: elf32-v850.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/v850.h $(INCDIR)/elf/reloc-macros.h elf32-target.h
elf32.lo: elf32.c elfcode.h $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  elfcore.h elflink.h
elflink.lo: elflink.c $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h
epoc-pe-arm.lo: epoc-pe-arm.c pe-arm.c coff-arm.c $(INCDIR)/coff/arm.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
epoc-pei-arm.lo: epoc-pei-arm.c pei-arm.c coff-arm.c \
  $(INCDIR)/coff/arm.h $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h \
  libcoff.h $(INCDIR)/bfdlink.h coffcode.h peicode.h \
  libpei.h
hp300bsd.lo: hp300bsd.c libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
hp300hpux.lo: hp300hpux.c $(INCDIR)/aout/hp300hpux.h \
  aoutx.h $(INCDIR)/bfdlink.h libaout.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h \
  aout-target.h
som.lo: som.c
i386aout.lo: i386aout.c $(INCDIR)/aout/aout64.h libaout.h \
  $(INCDIR)/bfdlink.h aout-target.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
i386bsd.lo: i386bsd.c libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
i386dynix.lo: i386dynix.c $(INCDIR)/aout/dynix3.h aoutx.h \
  $(INCDIR)/bfdlink.h libaout.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h \
  aout-target.h
i386freebsd.lo: i386freebsd.c freebsd.h libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
i386linux.lo: i386linux.c $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h libaout.h \
  $(INCDIR)/bfdlink.h aout-target.h
i386lynx.lo: i386lynx.c libaout.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/aout/aout64.h aout-target.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
i386msdos.lo: i386msdos.c libaout.h $(INCDIR)/bfdlink.h
i386netbsd.lo: i386netbsd.c netbsd.h libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
i386mach3.lo: i386mach3.c $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h libaout.h \
  $(INCDIR)/bfdlink.h aout-target.h
i386os9k.lo: i386os9k.c $(INCDIR)/bfdlink.h libaout.h \
  $(INCDIR)/os9k.h
ieee.lo: ieee.c $(INCDIR)/ieee.h libieee.h
m68k4knetbsd.lo: m68k4knetbsd.c netbsd.h libaout.h \
  $(INCDIR)/bfdlink.h aout-target.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
m68klinux.lo: m68klinux.c $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h libaout.h \
  $(INCDIR)/bfdlink.h aout-target.h
m68klynx.lo: m68klynx.c libaout.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/aout/aout64.h aout-target.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
m68knetbsd.lo: m68knetbsd.c netbsd.h libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
m88kmach3.lo: m88kmach3.c libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
mipsbsd.lo: mipsbsd.c libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
newsos3.lo: newsos3.c $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h libaout.h \
  $(INCDIR)/bfdlink.h aout-target.h
nlm.lo: nlm.c libnlm.h $(INCDIR)/nlm/common.h $(INCDIR)/nlm/internal.h \
  $(INCDIR)/nlm/external.h
nlm32-i386.lo: nlm32-i386.c $(INCDIR)/nlm/i386-ext.h \
  libnlm.h $(INCDIR)/nlm/common.h $(INCDIR)/nlm/internal.h \
  $(INCDIR)/nlm/external.h nlmswap.h nlm-target.h
nlm32-sparc.lo: nlm32-sparc.c $(INCDIR)/nlm/sparc32-ext.h \
  libnlm.h $(INCDIR)/nlm/common.h $(INCDIR)/nlm/internal.h \
  $(INCDIR)/nlm/external.h nlmswap.h nlm-target.h
nlm32-ppc.lo: nlm32-ppc.c $(INCDIR)/nlm/ppc-ext.h libnlm.h \
  $(INCDIR)/nlm/common.h $(INCDIR)/nlm/internal.h $(INCDIR)/nlm/external.h \
  nlmswap.h nlm-target.h
nlm32.lo: nlm32.c nlmcode.h libnlm.h $(INCDIR)/nlm/common.h \
  $(INCDIR)/nlm/internal.h $(INCDIR)/nlm/external.h
ns32knetbsd.lo: ns32knetbsd.c netbsd.h libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
oasys.lo: oasys.c $(INCDIR)/oasys.h liboasys.h
pc532-mach.lo: pc532-mach.c libaout.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/aout/aout64.h aout-target.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
pe-arm.lo: pe-arm.c coff-arm.c $(INCDIR)/coff/arm.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pei-arm.lo: pei-arm.c coff-arm.c $(INCDIR)/coff/arm.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pe-i386.lo: pe-i386.c coff-i386.c $(INCDIR)/coff/i386.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pei-i386.lo: pei-i386.c coff-i386.c $(INCDIR)/coff/i386.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pe-mcore.lo: pe-mcore.c coff-mcore.c $(INCDIR)/coff/mcore.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pei-mcore.lo: pei-mcore.c coff-mcore.c $(INCDIR)/coff/mcore.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pe-ppc.lo: pe-ppc.c coff-ppc.c $(INCDIR)/coff/powerpc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pei-ppc.lo: pei-ppc.c coff-ppc.c $(INCDIR)/coff/powerpc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
pe-sh.lo: pe-sh.c coff-sh.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/sh.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  coffcode.h peicode.h libpei.h
pei-sh.lo: pei-sh.c coff-sh.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/sh.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  coffcode.h peicode.h libpei.h
pe-mips.lo: pe-mips.c $(INCDIR)/coff/mipspe.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/pe.h libcoff.h $(INCDIR)/bfdlink.h coffcode.h \
  peicode.h libpei.h
pei-mips.lo: pei-mips.c pe-mips.c $(INCDIR)/coff/mipspe.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/pe.h libcoff.h \
  $(INCDIR)/bfdlink.h coffcode.h peicode.h libpei.h
peigen.lo: peigen.c $(INCDIR)/coff/internal.h $(INCDIR)/coff/i386.h \
  $(INCDIR)/coff/pe.h libcoff.h $(INCDIR)/bfdlink.h libpei.h
ppcboot.lo: ppcboot.c
reloc16.lo: reloc16.c $(INCDIR)/bfdlink.h genlink.h \
  $(INCDIR)/coff/internal.h libcoff.h
riscix.lo: riscix.c libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  aout-target.h $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def \
  $(INCDIR)/aout/ar.h
sparclinux.lo: sparclinux.c $(INCDIR)/aout/aout64.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h \
  libaout.h $(INCDIR)/bfdlink.h aout-target.h
sparclynx.lo: sparclynx.c $(INCDIR)/aout/sun4.h libaout.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h aout-target.h
sparcnetbsd.lo: sparcnetbsd.c netbsd.h libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
sunos.lo: sunos.c $(INCDIR)/bfdlink.h libaout.h aoutf1.h \
  $(INCDIR)/aout/sun4.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h aout-target.h
vaxnetbsd.lo: vaxnetbsd.c netbsd.h libaout.h $(INCDIR)/bfdlink.h \
  aout-target.h $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h
versados.lo: versados.c $(INCDIR)/libiberty.h
vms.lo: vms.c $(INCDIR)/bfdlink.h vms.h
vms-gsd.lo: vms-gsd.c $(INCDIR)/bfdlink.h vms.h
vms-hdr.lo: vms-hdr.c $(INCDIR)/bfdlink.h vms.h
vms-misc.lo: vms-misc.c $(INCDIR)/bfdlink.h vms.h
vms-tir.lo: vms-tir.c $(INCDIR)/bfdlink.h vms.h
xcofflink.lo: xcofflink.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/internal.h \
  libcoff.h
aout64.lo: aout64.c aoutx.h $(INCDIR)/bfdlink.h libaout.h \
  $(INCDIR)/aout/aout64.h $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def \
  $(INCDIR)/aout/ar.h
coff-alpha.lo: coff-alpha.c $(INCDIR)/bfdlink.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/symconst.h $(INCDIR)/coff/ecoff.h \
  $(INCDIR)/coff/alpha.h $(INCDIR)/aout/ar.h libcoff.h \
  libecoff.h coffswap.h ecoffswap.h
demo64.lo: demo64.c aoutf1.h $(INCDIR)/aout/sun4.h \
  libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(INCDIR)/aout/ar.h \
  aout-target.h
elf64-alpha.lo: elf64-alpha.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/alpha.h $(INCDIR)/elf/reloc-macros.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/symconst.h \
  $(INCDIR)/coff/ecoff.h $(INCDIR)/coff/alpha.h $(INCDIR)/aout/ar.h \
  libcoff.h libecoff.h ecoffswap.h elf64-target.h
elf64-gen.lo: elf64-gen.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  elf64-target.h
elf64-mips.lo: elf64-mips.c $(INCDIR)/aout/ar.h $(INCDIR)/bfdlink.h \
  genlink.h elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/elf/mips.h $(INCDIR)/elf/reloc-macros.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/symconst.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/ecoff.h $(INCDIR)/coff/alpha.h ecoffswap.h \
  elf64-target.h
elf64-sparc.lo: elf64-sparc.c elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/elf/sparc.h $(INCDIR)/elf/reloc-macros.h \
  elf64-target.h
elf64.lo: elf64.c elfcode.h $(INCDIR)/bfdlink.h elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  elfcore.h elflink.h
nlm32-alpha.lo: nlm32-alpha.c $(INCDIR)/nlm/alpha-ext.h \
  libnlm.h $(INCDIR)/nlm/common.h $(INCDIR)/nlm/internal.h \
  $(INCDIR)/nlm/external.h nlmswap.h nlm-target.h
nlm64.lo: nlm64.c nlmcode.h libnlm.h $(INCDIR)/nlm/common.h \
  $(INCDIR)/nlm/internal.h $(INCDIR)/nlm/external.h
aix386-core.lo: aix386-core.c $(INCDIR)/coff/i386.h \
  $(INCDIR)/coff/internal.h libcoff.h $(INCDIR)/bfdlink.h
hpux-core.lo: hpux-core.c
irix-core.lo: irix-core.c
lynx-core.lo: lynx-core.c
osf-core.lo: osf-core.c
sco5-core.lo: sco5-core.c libaout.h $(INCDIR)/bfdlink.h
trad-core.lo: trad-core.c libaout.h $(INCDIR)/bfdlink.h
cisco-core.lo: cisco-core.c
# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
