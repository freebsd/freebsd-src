## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

SUBDIRS = po

INCDIR = $(srcdir)/../include
BFDDIR = $(srcdir)/../bfd
MKDEP = gcc -MM

lib_LTLIBRARIES = libopcodes.la

# This is where bfd.h lives.
BFD_H = ../bfd/bfd.h

# Header files.
HFILES = \
	arm-opc.h \
	fr30-desc.h fr30-opc.h \
	h8500-opc.h \
	m32r-desc.h m32r-opc.h \
	mcore-opc.h \
	sh-opc.h \
	sysdep.h \
	w65-opc.h \
	z8k-opc.h

# C source files that correspond to .o's.
CFILES = \
	a29k-dis.c \
	alpha-dis.c \
	alpha-opc.c \
	arc-dis.c \
	arc-opc.c \
	arm-dis.c \
	avr-dis.c \
	cgen-asm.c \
	cgen-dis.c \
	cgen-opc.c \
	d10v-dis.c \
	d10v-opc.c \
	d30v-dis.c \
	d30v-opc.c \
	dis-buf.c \
	disassemble.c \
	fr30-asm.c \
	fr30-desc.c \
	fr30-dis.c \
	fr30-ibld.c \
	fr30-opc.c \
	h8300-dis.c \
	h8500-dis.c \
	hppa-dis.c \
	i370-dis.c \
	i370-opc.c \
	i386-dis.c \
	i960-dis.c \
	m32r-asm.c \
	m32r-desc.c \
	m32r-dis.c \
	m32r-ibld.c \
	m32r-opc.c \
	m32r-opinst.c \
	m68k-dis.c \
	m68k-opc.c \
	m88k-dis.c \
	mcore-dis.c \
	mips-dis.c \
	mips-opc.c \
	mips16-opc.c \
	m10200-dis.c \
	m10200-opc.c \
	m10300-dis.c \
	m10300-opc.c \
	ns32k-dis.c \
	pj-dis.c \
	pj-opc.c \
	ppc-dis.c \
	ppc-opc.c \
	sh-dis.c \
	sparc-dis.c \
	sparc-opc.c \
	tic30-dis.c \
	tic80-dis.c \
	tic80-opc.c \
	v850-dis.c \
	v850-opc.c \
	vax-dis.c \
	w65-dis.c \
	z8k-dis.c \
	z8kgen.c

ALL_MACHINES = \
	a29k-dis.lo \
	alpha-dis.lo \
	alpha-opc.lo \
	arc-dis.lo \
	arc-opc.lo \
	arm-dis.lo \
	avr-dis.lo \
	cgen-asm.lo \
	cgen-dis.lo \
	cgen-opc.lo \
	d10v-dis.lo \
	d10v-opc.lo \
	d30v-dis.lo \
	d30v-opc.lo \
	fr30-asm.lo \
	fr30-desc.lo \
	fr30-dis.lo \
	fr30-ibld.lo \
	fr30-opc.lo \
	h8300-dis.lo \
	h8500-dis.lo \
	hppa-dis.lo \
	i386-dis.lo \
	i370-dis.lo \
	i370-opc.lo \
	i960-dis.lo \
	m32r-asm.lo \
	m32r-desc.lo \
	m32r-dis.lo \
	m32r-ibld.lo \
	m32r-opc.lo \
	m32r-opinst.lo \
	m68k-dis.lo \
	m68k-opc.lo \
	m88k-dis.lo \
	m10200-dis.lo \
	m10200-opc.lo \
	m10300-dis.lo \
	m10300-opc.lo \
	mcore-dis.lo \
	mips-dis.lo \
	mips-opc.lo \
	mips16-opc.lo \
	pj-dis.lo \
	pj-opc.lo \
	ppc-dis.lo \
	ppc-opc.lo \
	ns32k-dis.lo \
	sh-dis.lo \
	sparc-dis.lo \
	sparc-opc.lo \
	tic30-dis.lo \
	tic80-dis.lo \
	tic80-opc.lo \
	v850-dis.lo \
	v850-opc.lo \
	vax-dis.lo \
	w65-dis.lo \
	z8k-dis.lo

OFILES = @BFD_MACHINES@

INCLUDES = -D_GNU_SOURCE -I. -I$(srcdir) -I../bfd -I$(INCDIR) -I$(BFDDIR) @HDEFINES@ -I$(srcdir)/../intl -I../intl

disassemble.lo: disassemble.c $(INCDIR)/dis-asm.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c @archdefs@ $(srcdir)/disassemble.c

libopcodes_la_SOURCES =  dis-buf.c disassemble.c
libopcodes_la_DEPENDENCIES = $(OFILES)
libopcodes_la_LIBADD = $(OFILES) @WIN32LIBADD@
libopcodes_la_LDFLAGS = -release $(VERSION) @WIN32LDFLAGS@

# libtool will build .libs/libopcodes.a.  We create libopcodes.a in
# the build directory so that we don't have to convert all the
# programs that use libopcodes.a simultaneously.  This is a hack which
# should be removed if everything else starts using libtool.  FIXME.

noinst_LIBRARIES = libopcodes.a

stamp-lib: libopcodes.la
	libtooldir=`$(LIBTOOL) --config | sed -n -e 's/^objdir=//p'`; \
	if [ -f $$libtooldir/libopcodes.a ]; then \
	  cp $$libtooldir/libopcodes.a libopcodes.tmp; \
	  $(SHELL) $(srcdir)/../move-if-change libopcodes.tmp libopcodes.a; \
	else true; fi
	touch stamp-lib

libopcodes.a: stamp-lib ; @true

POTFILES = $(HFILES) $(CFILES)
po/POTFILES.in: @MAINT@ Makefile
	for file in $(POTFILES); do echo $$file; done | sort > tmp \
	  && mv tmp $(srcdir)/po/POTFILES.in

# We should reconfigure whenever bfd/configure.in changes, because
# that's where the version number comes from.
config.status: $(srcdir)/configure $(srcdir)/../bfd/configure.in
	$(SHELL) ./config.status --recheck

CLEANFILES = \
	libopcodes.a stamp-lib dep.sed DEP DEP1 DEP2



# The start marker is written this way to pass through automake unscathed.




# This dependency stuff is copied from BFD.

DEP: dep.sed $(CFILES) $(HFILES) config.h
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
		-e 's!@BFDDIR@!$(BFDDIR)!'	\
		-e 's!@SRCDIR@!$(srcdir)!'

dep: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < Makefile > tmp-Makefile
	cat DEP >> tmp-Makefile
	$(srcdir)/../move-if-change tmp-Makefile Makefile

dep-in: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.in > tmp-Makefile.in
	cat DEP >> tmp-Makefile.in
	$(srcdir)/../move-if-change tmp-Makefile.in $(srcdir)/Makefile.in

dep-am: DEP
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.am > tmp-Makefile.am
	cat DEP >> tmp-Makefile.am
	$(srcdir)/../move-if-change tmp-Makefile.am $(srcdir)/Makefile.am

.PHONY: dep dep-in dep-am

# What appears below is generated by a hacked mkdep using gcc -MM.

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
a29k-dis.lo: a29k-dis.c $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h $(INCDIR)/opcode/a29k.h
alpha-dis.lo: alpha-dis.c $(INCDIR)/ansidecl.h sysdep.h \
  config.h $(INCDIR)/dis-asm.h $(BFD_H) $(INCDIR)/opcode/alpha.h
alpha-opc.lo: alpha-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/alpha.h \
  $(BFD_H) opintl.h
arc-dis.lo: arc-dis.c $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h $(INCDIR)/opcode/arc.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/elf/arc.h $(INCDIR)/elf/reloc-macros.h \
  opintl.h
arc-opc.lo: arc-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/arc.h \
  opintl.h
arm-dis.lo: arm-dis.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) arm-opc.h $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h opintl.h $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/elf/arm.h \
  $(INCDIR)/elf/reloc-macros.h
avr-dis.lo: avr-dis.c $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h opintl.h
cgen-asm.lo: cgen-asm.c sysdep.h config.h $(INCDIR)/libiberty.h \
  $(BFD_H) $(INCDIR)/symcat.h $(INCDIR)/opcode/cgen.h \
  opintl.h
cgen-dis.lo: cgen-dis.c sysdep.h config.h $(INCDIR)/libiberty.h \
  $(BFD_H) $(INCDIR)/symcat.h $(INCDIR)/opcode/cgen.h
cgen-opc.lo: cgen-opc.c sysdep.h config.h $(INCDIR)/libiberty.h \
  $(BFD_H) $(INCDIR)/symcat.h $(INCDIR)/opcode/cgen.h
d10v-dis.lo: d10v-dis.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/d10v.h \
  $(INCDIR)/dis-asm.h $(BFD_H)
d10v-opc.lo: d10v-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/d10v.h
d30v-dis.lo: d30v-dis.c $(INCDIR)/opcode/d30v.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/ansidecl.h opintl.h
d30v-opc.lo: d30v-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/d30v.h
dis-buf.lo: dis-buf.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) opintl.h
disassemble.lo: disassemble.c $(INCDIR)/ansidecl.h \
  $(INCDIR)/dis-asm.h $(BFD_H)
fr30-asm.lo: fr30-asm.c sysdep.h config.h $(BFD_H) \
  $(INCDIR)/symcat.h fr30-desc.h $(INCDIR)/opcode/cgen.h \
  fr30-opc.h opintl.h
fr30-desc.lo: fr30-desc.c sysdep.h config.h $(BFD_H) \
  $(INCDIR)/symcat.h fr30-desc.h $(INCDIR)/opcode/cgen.h \
  fr30-opc.h opintl.h
fr30-dis.lo: fr30-dis.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/symcat.h fr30-desc.h $(INCDIR)/opcode/cgen.h \
  fr30-opc.h opintl.h
fr30-ibld.lo: fr30-ibld.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/symcat.h fr30-desc.h $(INCDIR)/opcode/cgen.h \
  fr30-opc.h opintl.h
fr30-opc.lo: fr30-opc.c sysdep.h config.h $(BFD_H) \
  $(INCDIR)/symcat.h fr30-desc.h $(INCDIR)/opcode/cgen.h \
  fr30-opc.h
h8300-dis.lo: h8300-dis.c $(INCDIR)/opcode/h8300.h \
  $(INCDIR)/dis-asm.h $(BFD_H) $(INCDIR)/ansidecl.h opintl.h
h8500-dis.lo: h8500-dis.c h8500-opc.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/ansidecl.h opintl.h
hppa-dis.lo: hppa-dis.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(BFDDIR)/libhppa.h $(INCDIR)/opcode/hppa.h
i370-dis.lo: i370-dis.c $(INCDIR)/ansidecl.h sysdep.h \
  config.h $(INCDIR)/dis-asm.h $(BFD_H) $(INCDIR)/opcode/i370.h
i370-opc.lo: i370-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/i370.h
i386-dis.lo: i386-dis.c $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h sysdep.h config.h opintl.h
i960-dis.lo: i960-dis.c $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h
m32r-asm.lo: m32r-asm.c sysdep.h config.h $(BFD_H) \
  $(INCDIR)/symcat.h m32r-desc.h $(INCDIR)/opcode/cgen.h \
  m32r-opc.h opintl.h
m32r-desc.lo: m32r-desc.c sysdep.h config.h $(BFD_H) \
  $(INCDIR)/symcat.h m32r-desc.h $(INCDIR)/opcode/cgen.h \
  m32r-opc.h opintl.h
m32r-dis.lo: m32r-dis.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/symcat.h m32r-desc.h $(INCDIR)/opcode/cgen.h \
  m32r-opc.h opintl.h
m32r-ibld.lo: m32r-ibld.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/symcat.h m32r-desc.h $(INCDIR)/opcode/cgen.h \
  m32r-opc.h opintl.h
m32r-opc.lo: m32r-opc.c sysdep.h config.h $(BFD_H) \
  $(INCDIR)/symcat.h m32r-desc.h $(INCDIR)/opcode/cgen.h \
  m32r-opc.h
m32r-opinst.lo: m32r-opinst.c sysdep.h config.h $(BFD_H) \
  $(INCDIR)/symcat.h m32r-desc.h $(INCDIR)/opcode/cgen.h \
  m32r-opc.h
m68k-dis.lo: m68k-dis.c $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h $(INCDIR)/floatformat.h opintl.h \
  $(INCDIR)/opcode/m68k.h
m68k-opc.lo: m68k-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/m68k.h
m88k-dis.lo: m88k-dis.c $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h $(INCDIR)/opcode/m88k.h opintl.h
mcore-dis.lo: mcore-dis.c mcore-opc.h $(INCDIR)/ansidecl.h \
  $(INCDIR)/dis-asm.h $(BFD_H)
mips-dis.lo: mips-dis.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/opcode/mips.h opintl.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/elf/mips.h $(INCDIR)/elf/reloc-macros.h
mips-opc.lo: mips-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/mips.h
mips16-opc.lo: mips16-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/mips.h
m10200-dis.lo: m10200-dis.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/mn10200.h \
  $(INCDIR)/dis-asm.h $(BFD_H) opintl.h
m10200-opc.lo: m10200-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/mn10200.h
m10300-dis.lo: m10300-dis.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/mn10300.h \
  $(INCDIR)/dis-asm.h $(BFD_H) opintl.h
m10300-opc.lo: m10300-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/mn10300.h
ns32k-dis.lo: ns32k-dis.c $(BFD_H) $(INCDIR)/ansidecl.h \
  sysdep.h config.h $(INCDIR)/dis-asm.h $(INCDIR)/opcode/ns32k.h \
  opintl.h
pj-dis.lo: pj-dis.c $(INCDIR)/opcode/pj.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/ansidecl.h
pj-opc.lo: pj-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/pj.h
ppc-dis.lo: ppc-dis.c $(INCDIR)/ansidecl.h sysdep.h \
  config.h $(INCDIR)/dis-asm.h $(BFD_H) $(INCDIR)/opcode/ppc.h
ppc-opc.lo: ppc-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/ppc.h \
  opintl.h
sh-dis.lo: sh-dis.c sh-opc.h $(INCDIR)/dis-asm.h $(BFD_H) \
  $(INCDIR)/ansidecl.h
sparc-dis.lo: sparc-dis.c $(INCDIR)/ansidecl.h sysdep.h \
  config.h $(INCDIR)/opcode/sparc.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/libiberty.h opintl.h
sparc-opc.lo: sparc-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/sparc.h
tic30-dis.lo: tic30-dis.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/opcode/tic30.h
tic80-dis.lo: tic80-dis.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/tic80.h \
  $(INCDIR)/dis-asm.h $(BFD_H)
tic80-opc.lo: tic80-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/tic80.h
v850-dis.lo: v850-dis.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/v850.h \
  $(INCDIR)/dis-asm.h $(BFD_H) opintl.h
v850-opc.lo: v850-opc.c $(INCDIR)/ansidecl.h $(INCDIR)/opcode/v850.h \
  opintl.h
vax-dis.lo: vax-dis.c $(INCDIR)/opcode/vax.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/ansidecl.h
w65-dis.lo: w65-dis.c w65-opc.h $(INCDIR)/dis-asm.h \
  $(BFD_H) $(INCDIR)/ansidecl.h
z8k-dis.lo: z8k-dis.c sysdep.h config.h $(INCDIR)/dis-asm.h \
  $(BFD_H) z8k-opc.h
z8kgen.lo: z8kgen.c sysdep.h config.h
# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
