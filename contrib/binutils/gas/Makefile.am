## Process this file with automake to generate Makefile.in

## Work around apparent automake bug.
INTLLIBS = @INTLLIBS@

AUTOMAKE_OPTIONS = cygnus dejagnu

SUBDIRS = doc po

tooldir = $(exec_prefix)/$(target_alias)

YACC = `if [ -f ../bison/bison ] ; then echo ../bison/bison -y -L../bison/bison ; else echo @YACC@ ; fi`
LEX = `if [ -f ../flex/flex ] ; then echo ../flex/flex ; else echo @LEX@ ; fi`

MKDEP = gcc -MM

TARG_CPU = @target_cpu_type@
TARG_CPU_C = $(srcdir)/config/tc-@target_cpu_type@.c
TARG_CPU_O = tc-@target_cpu_type@.o
TARG_CPU_H = $(srcdir)/config/tc-@target_cpu_type@.h
OBJ_FORMAT_C = $(srcdir)/config/obj-@obj_format@.c
OBJ_FORMAT_O = obj-@obj_format@.o
OBJ_FORMAT_H = $(srcdir)/config/obj-@obj_format@.h
TARG_ENV_H = $(srcdir)/config/te-@te_file@.h
ATOF_TARG_C = $(srcdir)/config/atof-@atof@.c
ATOF_TARG_O = atof-@atof@.o

# use @target_cpu_type@ for refering to configured target name
IT_HDRS=itbl-parse.h $(srcdir)/itbl-ops.h 
IT_SRCS=itbl-parse.c itbl-lex.c $(srcdir)/itbl-ops.c
IT_DEPS=$(srcdir)/itbl-parse.y $(srcdir)/itbl-lex.l $(srcdir)/config/itbl-@target_cpu_type@.h
IT_OBJS=itbl-parse.o itbl-lex.o itbl-ops.o

# CPU types.  This is only used for dependency information.

CPU_TYPES = \
	a29k \
	alpha \
	arc \
	arm \
	avr \
	d10v \
	d30v \
	fr30 \
	h8300 \
	h8500 \
	hppa \
	i370 \
	i386 \
	i860 \
	i960 \
	m32r \
	m68k \
	m88k \
	mcore \
	mips \
	mn10200 \
	mn10300 \
	ns32k \
	pj \
	ppc \
	sh \
	sparc \
	tahoe \
	tic30 \
	tic80 \
	vax \
	w65 \
	v850 \
	z8k

# Object format types.  This is only used for dependency information.
# We deliberately omit som, since it does not work as a cross assembler.

OBJ_FORMATS = \
	aout \
	bout \
	coff \
	ecoff \
	elf \
	evax \
	hp300 \
	ieee \
	vms

# This is an sh case which sets valid according to whether the CPU
# type in the shell variable c and the OS type in the shell variable o
# are supported.  This helps cuts down on the amount of dependency
# information.

CPU_OBJ_VALID = \
	valid= ; \
	case $$o in \
	aout) \
	  case $$c in \
	  a29k | arm | i386 | i860 | m68k | mips | ns32k | sparc | tahoe | tic30 | vax) \
	    valid=yes ;; \
	  esac ;; \
	bout) \
	  case $$c in \
	  i960) valid=yes ;; \
	  esac ;; \
	coff) valid=yes ;; \
	ecoff) \
	  case $$c in \
	  mips | alpha) valid=yes ;; \
	  esac ;; \
	elf) valid=yes ;; \
	evax) \
	  case $$c in \
	  alpha) valid=yes ;; \
	  esac ;; \
	hp300) \
	  case $$c in \
	  m68k) valid=yes ;; \
	  esac ;; \
	vms) \
	  case $$c in \
	  vax) valid=yes ;; \
	  esac ;; \
	esac;

# These are like CPU_TYPES and CPU_OBJ_VALID, for the obj=multi case.

MULTI_CPU_TYPES = i386 mips

MULTI_CPU_OBJ_VALID = \
	valid= ; \
	case $$o in \
	aout) \
	  case $$c in \
	  i386) valid=yes ;; \
	  esac ;; \
	coff) valid=yes ;; \
	ecoff) \
	  case $$c in \
	  mips) valid=yes ;; \
	  esac ;; \
	elf) valid=yes ;; \
	esac;

# Regular source files.

GAS_CFILES = \
	app.c \
	as.c \
	atof-generic.c \
	bignum-copy.c \
	cond.c \
	depend.c \
	dwarf2dbg.c \
	ecoff.c \
	ehopt.c \
	expr.c \
	flonum-copy.c \
	flonum-konst.c \
	flonum-mult.c \
	frags.c \
	hash.c \
	input-file.c \
	input-scrub.c \
	listing.c \
	literal.c \
	macro.c \
	messages.c \
	output-file.c \
	read.c \
	sb.c \
	stabs.c \
	subsegs.c \
	symbols.c \
	write.c

CFILES = $(GAS_CFILES) gasp.c itbl-ops.c

HFILES = \
	as.h \
	asintl.h \
	bignum.h \
	bit_fix.h \
	cgen.h \
	dwarf2dbg.h \
	ecoff.h \
	emul-target.h \
	emul.h \
	expr.h \
	flonum.h \
	frags.h \
	hash.h \
	input-file.h \
	itbl-ops.h \
	listing.h \
	macro.h \
	obj.h \
	output-file.h \
	read.h \
	sb.h \
	struc-symbol.h \
	subsegs.h \
	symbols.h \
	tc.h \
	write.h

# CPU files in config.

TARGET_CPU_CFILES = \
	config/tc-a29k.c \
	config/tc-alpha.c \
	config/tc-arc.c \
	config/tc-arm.c \
	config/tc-avr.c \
	config/tc-d10v.c \
	config/tc-d30v.c \
	config/tc-fr30.c \
	config/tc-h8300.c \
	config/tc-h8500.c \
	config/tc-hppa.c \
	config/tc-i370.c \
	config/tc-i386.c \
	config/tc-i860.c \
	config/tc-i960.c \
	config/tc-m32r.c \
	config/tc-m68k.c \
	config/tc-m88k.c \
	config/tc-mcore.c \
	config/tc-mips.c \
	config/tc-mn10200.c \
	config/tc-mn10300.c \
	config/tc-ns32k.c \
	config/tc-pj.c \
	config/tc-ppc.c \
	config/tc-sh.c \
	config/tc-sparc.c \
	config/tc-tahoe.c \
	config/tc-tic30.c \
	config/tc-tic80.c \
	config/tc-vax.c \
	config/tc-w65.c \
	config/tc-v850.c \
	config/tc-z8k.c

TARGET_CPU_HFILES = \
	config/tc-a29k.h \
	config/tc-alpha.h \
	config/tc-arc.h \
	config/tc-arm.h \
	config/tc-avr.h \
	config/tc-d10v.h \
	config/tc-d30v.h \
	config/tc-fr30.h \
	config/tc-h8300.h \
	config/tc-h8500.h \
	config/tc-hppa.h \
	config/tc-i370.h \
	config/tc-i386.h \
	config/tc-i860.h \
	config/tc-i960.h \
	config/tc-m32r.h \
	config/tc-m68k.h \
	config/tc-m88k.h \
	config/tc-mcore.h \
	config/tc-mips.h \
	config/tc-mn10200.h \
	config/tc-mn10300.h \
	config/tc-ns32k.h \
	config/tc-pj.h \
	config/tc-ppc.h \
	config/tc-sh.h \
	config/tc-sparc.h \
	config/tc-tahoe.h \
	config/tc-tic30.h \
	config/tc-tic80.h \
	config/tc-vax.h \
	config/tc-w65.h \
	config/tc-v850.h \
	config/tc-z8k.h

# OBJ files in config

OBJ_FORMAT_CFILES = \
	config/obj-aout.c \
	config/obj-bout.c \
	config/obj-coff.c \
	config/obj-ecoff.c \
	config/obj-elf.c \
	config/obj-evax.c \
	config/obj-hp300.c \
	config/obj-ieee.c \
	config/obj-som.c \
	config/obj-vms.c

OBJ_FORMAT_HFILES = \
	config/obj-aout.h \
	config/obj-bout.h \
	config/obj-coff.h \
	config/obj-ecoff.h \
	config/obj-elf.h \
	config/obj-evax.h \
	config/obj-hp300.h \
	config/obj-ieee.h \
	config/obj-som.h \
	config/obj-vms.h

# Emulation header files in config

TARG_ENV_HFILES = \
	config/te-386bsd.h \
	config/te-aux.h \
	config/te-delta.h \
	config/te-delt88.h \
	config/te-dpx2.h \
	config/te-dynix.h \
	config/te-epoc-pe.h \
	config/te-generic.h \
	config/te-go32.h \
	config/te-hp300.h \
	config/te-hppa.h \
	config/te-i386aix.h \
	config/te-ic960.h \
	config/te-linux.h \
	config/te-lnews.h \
	config/te-lynx.h \
	config/te-mach.h \
	config/te-macos.h \
	config/te-multi.h \
	config/te-nbsd.h \
	config/te-nbsd532.h \
	config/te-pc532mach.h \
	config/te-pe.h \
	config/te-ppcnw.h \
	config/te-psos.h \
	config/te-riscix.h \
	config/te-sparcaout.h \
	config/te-sun3.h \
	config/te-svr4.h \
	config/te-sysv32.h

# Multi files in config

MULTI_CFILES = \
	config/e-i386aout.c \
	config/e-i386coff.c \
	config/e-i386elf.c \
	config/e-mipsecoff.c \
	config/e-mipself.c

CONFIG_OBJS = \
	$(TARG_CPU_O) \
	$(OBJ_FORMAT_O) \
	$(ATOF_TARG_O) \
	$(extra_objects)

GENERIC_OBJS = \
	app.o \
	as.o \
	atof-generic.o \
	bignum-copy.o \
	cond.o \
	depend.o \
	dwarf2dbg.o \
	ehopt.o \
	expr.o \
	flonum-konst.o \
	flonum-copy.o \
	flonum-mult.o \
	frags.o \
	hash.o \
	input-file.o \
	input-scrub.o \
	literal.o \
	messages.o \
	output-file.o \
	read.o \
	subsegs.o \
	symbols.o \
	write.o \
	listing.o \
	ecoff.o \
	stabs.o \
	sb.o \
	macro.o

OBJS = $(CONFIG_OBJS) $(GENERIC_OBJS)

POTFILES = $(MULTI_CFILES) $(TARGET_ENV_HFILES) $(OBJ_FORMAT_HFILES) \
	$(OBJ_FORMAT_CFILES) $(TARGET_CPU_HFILES) $(TARGET_CPU_CFILES) \
	$(HFILES) $(CFILES) $(GAS_CFILES)
po/POTFILES.in: @MAINT@ Makefile
	for file in $(POTFILES); do echo $$file; done | sort > tmp \
	  && mv tmp $(srcdir)/po/POTFILES.in

noinst_PROGRAMS = as-new gasp-new
noinst_SCRIPTS = $(GDBINIT)
EXTRA_SCRIPTS = .gdbinit

$(srcdir)/make-gas.com: stamp-mk.com
stamp-mk.com: vmsconf.sh Makefile
	sh $(srcdir)/vmsconf.sh $(GENERIC_OBJS) > new-make.com
	$(SHELL) $(srcdir)/../move-if-change new-make.com $(srcdir)/make-gas.com
	touch stamp-mk.com

EXTRA_DIST = make-gas.com

DISTSTUFF = make-gas.com m68k-parse.c itbl-parse.c itbl-parse.h itbl-lex.c
diststuff: $(DISTSTUFF) info

DISTCLEANFILES = targ-cpu.h obj-format.h targ-env.h itbl-cpu.h cgen-desc.h

# Now figure out from those variables how to compile and link.

BASEDIR = $(srcdir)/..
BFDDIR = $(BASEDIR)/bfd
INCDIR = $(BASEDIR)/include

# This is the variable actually used when we compile.
# Specify the directories to be searched for header files.
# Both . and srcdir are used, in that order,
# so that tm.h and config.h will be found in the compilation
# subdirectory rather than in the source directory.
INCLUDES = -D_GNU_SOURCE -I. -I$(srcdir) -I../bfd -I$(srcdir)/config -I$(INCDIR) -I$(srcdir)/.. -I$(BFDDIR) -I$(srcdir)/../intl -I../intl -DLOCALEDIR="\"$(prefix)/share/locale\""

# This should be parallel to INCLUDES, but should replace $(srcdir)
# with $${srcdir}, and should work in a subdirectory.  This is used
# when building dependencies, because the dependency building is done
# in a subdirectory.
DEP_INCLUDES = -D_GNU_SOURCE -I.. -I$${srcdir} -I../../bfd -I$${srcdir}/config -I$${srcdir}/../include -I$${srcdir}/.. -I$${srcdir}/../bfd -I$${srcdir}/../intl -I../../intl -DLOCALEDIR="\"$(prefix)/share/locale\""

# How to link with both our special library facilities
# and the system's installed libraries.

GASLIBS = @OPCODES_LIB@ @BFDLIB@ ../libiberty/libiberty.a

# Files to be copied away after each stage in building.
STAGESTUFF = *.o $(noinst_PROGRAMS)

$(OBJS): @ALL_OBJ_DEPS@

as_new_SOURCES = $(GAS_CFILES)
as_new_LDADD = $(TARG_CPU_O) $(OBJ_FORMAT_O) $(ATOF_TARG_O) \
	$(extra_objects) $(GASLIBS) $(INTLLIBS)
as_new_DEPENDENCIES = $(TARG_CPU_O) $(OBJ_FORMAT_O) $(ATOF_TARG_O) \
	$(extra_objects) $(GASLIBS) $(INTLDEPS)

# Stuff that every object file depends upon.  If anything is removed
# from this list, remove it from dep-in.sed as well.
$(OBJS): config.h as.h $(TARG_ENV_H) $(OBJ_FORMAT_H) $(TARG_CPU_H) flonum.h \
	expr.h write.h frags.h hash.h read.h symbols.h tc.h \
	obj.h listing.h bignum.h bit_fix.h $(INCDIR)/libiberty.h asintl.h

gasp_new_SOURCES = gasp.c macro.c sb.c hash.c
gasp_new_LDADD = ../libiberty/libiberty.a $(INTLLIBS)
gasp_new_DEPENDENCIES = ../libiberty/libiberty.a $(INTLDEPS)

EXPECT = `if [ -f $${rootme}/../expect/expect ] ; then \
          echo $${rootme}/../expect/expect ; \
          else echo expect ; fi`

RUNTEST = `if [ -f $${srcdir}/../dejagnu/runtest ] ; then \
		echo $${srcdir}/../dejagnu/runtest ; else echo runtest; \
	   fi`
RUNTESTFLAGS=

check-DEJAGNU: site.exp
	if [ -d testsuite ]; then \
	  true; \
	else \
	  mkdir testsuite; \
	fi
	rm -f testsuite/site.exp
	cp site.exp testsuite/site.exp
	rootme=`pwd`; export rootme; \
	srcdir=`cd ${srcdir}; pwd` ; export srcdir ; \
	EXPECT=${EXPECT} ; export EXPECT ; \
	if [ -f $(top_builddir)/../expect/expect ]; then \
	  TCL_LIBRARY=`cd $(top_srcdir)/../tcl/library && pwd`; \
	  export TCL_LIBRARY; \
	fi; \
	runtest=$(RUNTEST); \
	cd testsuite; \
	if $(SHELL) -c "$$runtest --version" > /dev/null 2>&1; then \
	  $$runtest --tool $(DEJATOOL) --srcdir $${srcdir}/testsuite \
		$(RUNTESTFLAGS); \
	else echo "WARNING: could not find \`runtest'" 1>&2; :;\
	fi

# The implicit .c.o rule doesn't work for these, perhaps because of
# the variables, or perhaps because the sources are not on vpath.
$(TARG_CPU_O): $(TARG_CPU_C) $(TARG_CPU_DEP_@target_cpu_type@)
	$(COMPILE) -c $(TARG_CPU_C)
$(ATOF_TARG_O): $(ATOF_TARG_C)
	$(COMPILE) -c $(ATOF_TARG_C)

# ecoff.c only has full dependencies when ECOFF_DEBUGGING is defined,
# so the automatic dependency stuff doesn't work.
ecoff.o : ecoff.c ecoff.h $(INCDIR)/coff/internal.h $(INCDIR)/coff/sym.h \
	$(INCDIR)/coff/ecoff.h $(INCDIR)/coff/symconst.h \
	$(INCDIR)/aout/stab_gnu.h

# We need all these explicit rules for the multi stuff.  Because of
# these rules, we don't need one for OBJ_FORMAT_O.

obj-aout.o : $(srcdir)/config/obj-aout.c
	$(COMPILE) -c $(srcdir)/config/obj-aout.c
obj-bout.o : $(srcdir)/config/obj-bout.c
	$(COMPILE) -c $(srcdir)/config/obj-bout.c
obj-coff.o: $(srcdir)/config/obj-coff.c
	$(COMPILE) -c $(srcdir)/config/obj-coff.c
obj-ecoff.o : $(srcdir)/config/obj-ecoff.c
	$(COMPILE) -c $(srcdir)/config/obj-ecoff.c
obj-elf.o : $(srcdir)/config/obj-elf.c
	$(COMPILE) -c $(srcdir)/config/obj-elf.c
obj-evax.o : $(srcdir)/config/obj-evax.c
	$(COMPILE) -c $(srcdir)/config/obj-evax.c
obj-hp300.o : $(srcdir)/config/obj-hp300.c
	$(COMPILE) -c $(srcdir)/config/obj-hp300.c
obj-ieee.o : $(srcdir)/config/obj-ieee.c
	$(COMPILE) -c $(srcdir)/config/obj-ieee.c
obj-multi.o : $(srcdir)/config/obj-multi.c
	$(COMPILE) -c $(srcdir)/config/obj-multi.c
obj-som.o : $(srcdir)/config/obj-som.c
	$(COMPILE) -c $(srcdir)/config/obj-som.c
obj-vms.o : $(srcdir)/config/obj-vms.c
	$(COMPILE) -c $(srcdir)/config/obj-vms.c

e-mipself.o : $(srcdir)/config/e-mipself.c
	$(COMPILE) -c $(srcdir)/config/e-mipself.c
e-mipsecoff.o : $(srcdir)/config/e-mipsecoff.c
	$(COMPILE) -c $(srcdir)/config/e-mipsecoff.c
e-i386aout.o: $(srcdir)/config/e-i386aout.c
	$(COMPILE) -c $(srcdir)/config/e-i386aout.c
e-i386coff.o: $(srcdir)/config/e-i386coff.c
	$(COMPILE) -c $(srcdir)/config/e-i386coff.c
e-i386elf.o: $(srcdir)/config/e-i386elf.c
	$(COMPILE) -c $(srcdir)/config/e-i386elf.c

# The m68k operand parser.

EXTRA_as_new_SOURCES = config/m68k-parse.y

# If m68k-parse.y is in a different directory, then ylwrap will use an
# absolute path when it invokes yacc, which will cause yacc to put the
# absolute path into the generated file.  That's a pain when it comes
# to generating snapshots, because it introduces spurious diffs.
# Since when we make the snapshots $(srcdir) = ".", we check for that
# case and handle it differently.  This means that anybody who
# configures with $(srcdir) = "." will have to set their path in the
# debugger if they want to debug m68k-parse.y.  This is bad, but on
# the other hand it's good that people who use the prebuilt
# m68k-parse.c don't get a spurious absolute path.
m68k-parse.c: $(srcdir)/config/m68k-parse.y
	f=$(srcdir)/config/m68k-parse.y; \
	if [ $$f = "./config/m68k-parse.y" ]; then \
	  ln -s config/m68k-parse.y . > /dev/null 2>/dev/null || \
	   ln config/m68k-parse.y . > /dev/null 2>/dev/null || \
	   cp config/m68k-parse.y . >/dev/null 2>/dev/null; \
	  f=m68k-parse.y; \
	else true; fi; \
	$(SHELL) $(YLWRAP) "$(YACC)" $$f y.tab.c m68k-parse.c --; \
	if [ $$f = "m68k-parse.y" ]; then \
	  rm -f m68k-parse.y; \
	else true; fi
m68k-parse.o: m68k-parse.c $(srcdir)/config/m68k-parse.h

# Don't let the .y.h rule clobber m68k-parse.h.
m68k-parse.h: ; @true
$(srcdir)/config/m68k-parse.h: ; @true

# The instruction table specification lexical analyzer and parser.

itbl-lex.c: $(srcdir)/itbl-lex.l
itbl-lex.o: itbl-lex.c itbl-parse.h

itbl-parse.o: itbl-parse.c itbl-parse.h $(srcdir)/itbl-ops.h

itbl-ops.o: $(srcdir)/itbl-ops.c $(srcdir)/itbl-ops.h itbl-parse.h

itbl-parse.c itbl-parse.h: $(srcdir)/itbl-parse.y
	$(SHELL) $(YLWRAP) "$(YACC)" $(srcdir)/itbl-parse.y y.tab.c itbl-parse.c y.tab.h itbl-parse.h -- -d

# stand-alone itbl assembler & disassembler

EXTRA_PROGRAMS = itbl-test
itbl_test_SOURCES = itbl-parse.y itbl-lex.l
itbl_test_LDADD = itbl-tops.o itbl-test.o $(GASLIBS) @LEXLIB@

itbl-tops.o: $(srcdir)/itbl-ops.c $(srcdir)/itbl-ops.h itbl-parse.h
	$(COMPILE) -o itbl-tops.o -DSTAND_ALONE -c $(srcdir)/itbl-ops.c

itbl-test.o: $(srcdir)/testsuite/gas/all/itbl-test.c $(srcdir)/itbl-ops.h
	$(COMPILE) -c -DSTAND_ALONE $(srcdir)/testsuite/gas/all/itbl-test.c

# CGEN interface.

CGEN_CPU_PREFIX = @cgen_cpu_prefix@

cgen.o: cgen.c cgen.h cgen-desc.h subsegs.h \
	$(INCDIR)/obstack.h $(INCDIR)/opcode/cgen.h \
	$(srcdir)/../opcodes/$(CGEN_CPU_PREFIX)-desc.h \
	$(srcdir)/../opcodes/$(CGEN_CPU_PREFIX)-opc.h

# Remake the info files.

MOSTLYCLEANFILES = $(STAGESTUFF) core stamp-mk.com \
	testsuite/*.o testsuite/*.out testsuite/gas.log testsuite/gas.sum \
	testsuite/site.exp site.bak site.exp stage stage1 stage2

CLEANFILES = dep.sed DEPTC DEPTCA DEPOBJ DEPOBJA DEP2 DEP2a DEP1 DEPA DEP DEPDIR

.PHONY: install-exec-local install-data-local
.PHONY: install-exec-bindir install-exec-tooldir

install-exec-local: install-exec-bindir @install_tooldir@

install-exec-bindir: $(noinst_PROGRAMS)
	$(mkinstalldirs) $(bindir)
	@list='$(noinst_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(bindir)/`echo $$p|sed -e 's/$(EXEEXT)$$//' -e 's/-new//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
	    $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(bindir)/`echo $$p|sed -e 's/$(EXEEXT)$$//' -e 's/-new//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	  else :; fi; \
	done

install-exec-tooldir: install-exec-bindir $(noinst_PROGRAMS)
	$(mkinstalldirs) $(tooldir)/bin
	n=`echo as | sed '$(transform)'`; \
	if [ "$(bindir)/$$n$(EXEEXT)" != "$(tooldir)/bin/as$(EXEEXT)" ]; then \
	  rm -f $(tooldir)/bin/as$(EXEEXT); \
	  ln $(bindir)/$$n$(EXEEXT) $(tooldir)/bin/as$(EXEEXT) >/dev/null 2>/dev/null \
	    || $(LIBTOOL) --mode=install $(INSTALL_PROGRAM) as-new$(EXEEXT) $(tooldir)/bin/as$(EXEEXT); \
	else \
	  true ; \
	fi

# These exist for maintenance purposes.

.PHONY: bootstrap bootstrap2 bootstrap3 stage1 stage2 stage3 comparison

bootstrap: as-new
	$(MAKE) stage1
	rm -f stage && ln -s stage1 stage
	$(MAKE) CC="$(CC)" CFLAGS="-O -Bstage/ $(CFLAGS)" libdir=$(libdir) ALLOCA= $(noinst_PROGRAMS)
	$(MAKE) stage2
	rm -f stage && ln -s stage2 stage
	$(MAKE) CC="$(CC)" CFLAGS="-O -Bstage/ $(CFLAGS)" libdir=$(libdir) ALLOCA= $(noinst_PROGRAMS)
	$(MAKE) comparison against=stage2

bootstrap2:
	rm -f stage && ln -s stage1 stage
	$(MAKE) CC="$(CC)" CFLAGS="-O -Bstage/ $(CFLAGS)" libdir=$(libdir) ALLOCA= $(noinst_PROGRAMS)
	$(MAKE) stage2
	rm -f stage && ln -s stage2 stage
	$(MAKE) CC="$(CC)" CFLAGS="-O -Bstage/ $(CFLAGS)" libdir=$(libdir) ALLOCA= $(noinst_PROGRAMS)
	$(MAKE) comparison against=stage2

bootstrap3:
	rm -f stage && ln -s stage2 stage
	$(MAKE) CC="$(CC)" CFLAGS="-O -Bstage/ $(CFLAGS)" libdir=$(libdir) ALLOCA= $(noinst_PROGRAMS)
	$(MAKE) comparison against=stage2

# Copy the object files from a particular stage into a subdirectory.
stage1:
	-mkdir stage1
	-mv $(STAGESTUFF) stage1
	if [ -f stage1/as-new$(EXEEXT) -a ! -f stage1/as$(EXEEXT) ] ; then (cd stage1 ; ln -s as-new$(EXEEXT) as$(EXEEXT)) ; fi

stage2:
	-mkdir stage2
	-mv $(STAGESTUFF) stage2
	if [ -f stage2/as-new$(EXEEXT) -a ! -f stage2/as$(EXEEXT) ] ; then (cd stage2 ; ln -s as-new$(EXEEXT) as$(EXEEXT)) ; fi

stage3:
	-mkdir stage3
	-mv $(STAGESTUFF) stage3
	if [ -f stage3/as-new$(EXEEXT) -a ! -f stage3/as$(EXEEXT) ] ; then (cd stage3 ; ln -s as-new as$(EXEEXT)) ; fi

against=stage2

# This rule is derived from corresponding code in the Makefile.in for gcc.
# The "tail +16c" is to bypass headers which may include timestamps or
# temporary assembly file names.
comparison:
	x=0 ; \
	for file in *.o ; do \
	  tail +16c ./$$file > tmp-foo1; \
	  if tail +16c ${against}/$$file > tmp-foo2 2>/dev/null ; then \
	    if cmp tmp-foo1 tmp-foo2 ; then \
	      true ; \
	    else \
	      echo $$file differs ; \
	      x=1 ; \
	    fi ; \
	  else true; fi ; \
	done ; \
	exit $$x
	-rm -f tmp-foo*

.PHONY: de-stage1 de-stage2 de-stage3

de-stage1:
	- (cd stage1 ; rm -f as$(EXEEXT) ; mv -f * ..)
	- rmdir stage1

de-stage2:
	- (cd stage2 ; rm -f as$(EXEEXT) ; mv -f * ..)
	- rmdir stage2

de-stage3:
	- (cd stage3 ; rm -f as$(EXEEXT) ; mv -f * ..)
	- rmdir stage3

# Automatic dependency computation.  This is a real pain, because the
# dependencies change based on target_cpu_type and obj_format.  We
# currently ignore any dependencies caused by emulation files.

DEP_FILE_DEPS =  $(CFILES) $(HFILES) $(TARGET_CPU_CFILES) \
	$(TARGET_CPU_HFILES) $(OBJ_FORMAT_CFILES) $(OBJ_FORMAT_HFILES)

DEP: dep.sed $(DEP_FILE_DEPS) DEPTC DEPOBJ DEP2
	rm -f DEP1
	srcdir=`cd $(srcdir); pwd`; \
	$(MAKE) MKDEP="$(MKDEP)" srcdir="$${srcdir}" VPATH="$${srcdir}" DEP1
	rm -rf DEPDIR
	sed -f dep.sed < DEP1 > DEPA
	sed -f dep.sed < DEPTC >> DEPA
	sed -f dep.sed < DEPOBJ >> DEPA
	sed -f dep.sed < DEP2 >> DEPA
	echo '$$(OBJS): $$(DEP_@target''_cpu_type@_@obj''_format@)' >> DEPA
	echo '$$(TARG_CPU_O): $$(DEPTC_@target''_cpu_type@_@obj''_format@)' >> DEPA
	echo '$$(OBJ_FORMAT_O): $$(DEPOBJ_@target''_cpu_type@_@obj''_format@)' >> DEPA
	echo '# IF YOU PUT ANYTHING HERE IT WILL GO AWAY' >> DEPA
	mv -f DEPA $@

DEP1: $(CFILES) $(MULTI_CFILES)
	if [ -d DEPDIR ]; then true; else mkdir DEPDIR; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd DEPDIR; \
	echo '' > targ-cpu.h; \
	echo '' > obj-format.h; \
	echo '' > targ-env.h; \
	echo '' > itbl-cpu.h; \
	echo '' > itbl-parse.h; \
	echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > DEP; \
	echo '# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.' >> DEP; \
	$(MKDEP) -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) $? >> DEP
	mv -f DEPDIR/DEP $@

# Work out the special dependencies for the tc-*.c files.
DEPTC: $(TARGET_CPU_CFILES)
	rm -f DEPTCA
	if [ -d DEPDIR ]; then true; else mkdir DEPDIR; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd DEPDIR; \
	for c in $(CPU_TYPES); do \
	  for o in $(OBJ_FORMATS); do \
	    $(CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '#include "tc-'"$${c}"'.h"' > targ-cpu.h; \
	      echo '#include "obj-'"$${o}"'.h"' > obj-format.h; \
	      echo '#include "te-generic.h"' > targ-env.h; \
	      echo '' > itbl-cpu.h; \
	      echo '' > itbl-parse.h; \
	      echo '#include "opcodes/'"$${c}"'-desc.h"' > cgen-desc.h; \
	      rm -f dummy.c; \
	      cp $${srcdir}/config/tc-$${c}.c dummy.c; \
	      $(MKDEP) -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) dummy.c | \
	        sed -e "s/dummy.o: dummy.c/DEPTC_$${c}_$${o} =/" >> ../DEPTCA; \
	      rm -f dummy.c; \
	    else true; fi; \
	  done; \
	done
	echo 'DEPTC_hppa_som = $$(srcdir)/config/tc-hppa.h subsegs.h \' >> DEPTCA
	echo '  $$(INCDIR)/obstack.h $$(BFDDIR)/libhppa.h \' >> DEPTCA
	echo '  $$(INCDIR)/opcode/hppa.h $$(BFDDIR)/som.h' >> DEPTCA
	for c in $(MULTI_CPU_TYPES); do \
	  echo 'DEPTC_'"$${c}"'_multi = \' >> DEPTCA; \
	  for o in $(OBJ_FORMATS); do \
	    $(MULTI_CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '$$(DEPTC_'"$${c}_$${o}"') \' >> DEPTCA; \
	    else true; fi; \
	  done; \
	  echo '' >> DEPTCA; \
	done
	mv -f DEPTCA DEPTC

# Work out the special dependencies for the obj-*.c files.
DEPOBJ: $(OBJ_FORMAT_CFILES)
	rm -f DEPOBJA
	if [ -d DEPDIR ]; then true; else mkdir DEPDIR; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd DEPDIR; \
	for c in $(CPU_TYPES); do \
	  for o in $(OBJ_FORMATS); do \
	    $(CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '#include "tc-'"$${c}"'.h"' > targ-cpu.h; \
	      echo '#include "obj-'"$${o}"'.h"' > obj-format.h; \
	      echo '#include "te-generic.h"' > targ-env.h; \
	      echo '' > itbl-cpu.h; \
	      echo '' > itbl-parse.h; \
	      rm -f dummy.c; \
	      cp $${srcdir}/config/obj-$${o}.c dummy.c; \
	      $(MKDEP) -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) dummy.c | \
	        sed -e "s/dummy.o: dummy.c/DEPOBJ_$${c}_$${o} =/" >> ../DEPOBJA; \
	      rm -f dummy.c; \
	    else true; fi; \
	  done; \
	done
	echo 'DEPOBJ_hppa_som = $$(srcdir)/config/obj-som.h subsegs.h \' >> DEPOBJA
	echo '  $$(INCDIR)/obstack.h $$(BFDDIR)/libhppa.h \' >> DEPOBJA
	echo '  $$(BFDDIR)/som.h  $$(INCDIR)/aout/stab_gnu.h \' >> DEPOBJA
	echo '  $$(INCDIR)/aout/stab.def' >> DEPOBJA
	for c in $(MULTI_CPU_TYPES); do \
	  echo 'DEPOBJ_'"$${c}"'_multi = \' >> DEPOBJA; \
	  for o in $(OBJ_FORMATS); do \
	    $(MULTI_CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '$$(DEPOBJ_'"$${c}_$${o}"') \' >> DEPOBJA; \
	    else true; fi; \
	  done; \
	  echo '' >> DEPOBJA; \
	done
	mv -f DEPOBJA DEPOBJ

# Work out the dependencies for each CPU/OBJ combination.
# Note that SOM is a special case, because it only works native.
DEP2: $(TARGET_CPU_HFILES) $(OBJ_FORMAT_HFILES)
	rm -f DEP2a
	if [ -d DEPDIR ]; then true; else mkdir DEPDIR; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd DEPDIR; \
	for c in $(CPU_TYPES); do \
	  for o in $(OBJ_FORMATS); do \
	    $(CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '#include "tc-'"$${c}"'.h"' > targ-cpu.h; \
	      echo '#include "obj-'"$${o}"'.h"' > dummy.c; \
	      $(MKDEP) -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) dummy.c | \
	        sed -e "s/dummy.o: dummy.c/DEP_$${c}_$${o} =/" >> ../DEP2a; \
	    else true; fi; \
	  done; \
	done
	echo 'DEP_hppa_som = $$(BFDDIR)/som.h' >> DEP2a
	for c in $(MULTI_CPU_TYPES); do \
	  echo 'DEP_'"$${c}"'_multi = \' >> DEP2a; \
	  for o in $(OBJ_FORMATS); do \
	    $(MULTI_CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '$$(DEP_'"$${c}_$${o}"') \' >> DEP2a; \
	    else true; fi; \
	  done; \
	  echo '' >> DEP2a; \
	done
	mv -f DEP2a DEP2

dep.sed: dep-in.sed config.status
	srcdir=`cd $(srcdir); pwd`; \
	sed <$(srcdir)/dep-in.sed >dep.sed \
		-e "s!@INCDIR@!$${srcdir}/../include!" \
		-e "s!@BFDDIR@!$${srcdir}/../bfd!" \
		-e "s!@SRCDIR@!$${srcdir}!"

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

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
app.o: app.c $(INCDIR)/bin-bugs.h emul.h
as.o: as.c $(INCDIR)/bin-bugs.h emul.h subsegs.h $(INCDIR)/obstack.h \
  output-file.h sb.h macro.h
atof-generic.o: atof-generic.c $(INCDIR)/bin-bugs.h \
  emul.h
bignum-copy.o: bignum-copy.c $(INCDIR)/bin-bugs.h emul.h
cond.o: cond.c $(INCDIR)/bin-bugs.h emul.h macro.h \
  sb.h $(INCDIR)/obstack.h
depend.o: depend.c $(INCDIR)/bin-bugs.h emul.h
dwarf2dbg.o: dwarf2dbg.c $(INCDIR)/bin-bugs.h emul.h \
  dwarf2dbg.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/elf/dwarf2.h
ecoff.o: ecoff.c $(INCDIR)/bin-bugs.h emul.h ecoff.h
ehopt.o: ehopt.c $(INCDIR)/bin-bugs.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/elf/dwarf2.h
expr.o: expr.c $(INCDIR)/bin-bugs.h emul.h $(INCDIR)/obstack.h
flonum-copy.o: flonum-copy.c $(INCDIR)/bin-bugs.h emul.h
flonum-konst.o: flonum-konst.c
flonum-mult.o: flonum-mult.c
frags.o: frags.c $(INCDIR)/bin-bugs.h emul.h subsegs.h \
  $(INCDIR)/obstack.h
hash.o: hash.c $(INCDIR)/bin-bugs.h emul.h $(INCDIR)/obstack.h
input-file.o: input-file.c $(INCDIR)/bin-bugs.h emul.h \
  input-file.h
input-scrub.o: input-scrub.c $(INCDIR)/bin-bugs.h emul.h \
  input-file.h sb.h
listing.o: listing.c $(INCDIR)/bin-bugs.h emul.h input-file.h \
  subsegs.h
literal.o: literal.c $(INCDIR)/bin-bugs.h emul.h subsegs.h \
  $(INCDIR)/obstack.h
macro.o: macro.c sb.h macro.h
messages.o: messages.c $(INCDIR)/bin-bugs.h emul.h
output-file.o: output-file.c $(INCDIR)/bin-bugs.h emul.h \
  output-file.h
read.o: read.c $(INCDIR)/bin-bugs.h emul.h subsegs.h \
  $(INCDIR)/obstack.h sb.h macro.h ecoff.h
sb.o: sb.c sb.h
stabs.o: stabs.c $(INCDIR)/bin-bugs.h emul.h $(INCDIR)/obstack.h \
  subsegs.h ecoff.h $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def
subsegs.o: subsegs.c $(INCDIR)/bin-bugs.h emul.h subsegs.h \
  $(INCDIR)/obstack.h
symbols.o: symbols.c $(INCDIR)/bin-bugs.h emul.h $(INCDIR)/obstack.h \
  subsegs.h struc-symbol.h
write.o: write.c $(INCDIR)/bin-bugs.h emul.h subsegs.h \
  $(INCDIR)/obstack.h output-file.h
gasp.o: gasp.c $(INCDIR)/bin-bugs.h sb.h macro.h
itbl-ops.o: itbl-ops.c itbl-ops.h $(INCDIR)/bin-bugs.h \
  emul.h itbl-parse.h
e-i386aout.o: $(srcdir)/config/e-i386aout.c $(INCDIR)/bin-bugs.h \
  emul.h emul-target.h
e-i386coff.o: $(srcdir)/config/e-i386coff.c $(INCDIR)/bin-bugs.h \
  emul.h emul-target.h
e-i386elf.o: $(srcdir)/config/e-i386elf.c $(INCDIR)/bin-bugs.h \
  emul.h emul-target.h
e-mipsecoff.o: $(srcdir)/config/e-mipsecoff.c $(INCDIR)/bin-bugs.h \
  emul.h emul-target.h
e-mipself.o: $(srcdir)/config/e-mipself.c $(INCDIR)/bin-bugs.h \
  emul.h emul-target.h
DEPTC_a29k_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-a29k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/opcode/a29k.h
DEPTC_a29k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-a29k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/a29k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/opcode/a29k.h
DEPTC_a29k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-a29k.h \
  emul.h $(INCDIR)/opcode/a29k.h
DEPTC_alpha_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-alpha.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h struc-symbol.h ecoff.h $(INCDIR)/opcode/alpha.h \
  $(srcdir)/config/atof-vax.c
DEPTC_alpha_ecoff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-ecoff.h \
  $(srcdir)/config/tc-alpha.h ecoff.h $(INCDIR)/coff/sym.h \
  $(INCDIR)/coff/ecoff.h emul.h subsegs.h $(INCDIR)/obstack.h \
  struc-symbol.h $(INCDIR)/opcode/alpha.h $(srcdir)/config/atof-vax.c
DEPTC_alpha_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-alpha.h \
  emul.h subsegs.h $(INCDIR)/obstack.h struc-symbol.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h \
  $(INCDIR)/opcode/alpha.h $(INCDIR)/elf/alpha.h $(INCDIR)/elf/reloc-macros.h \
  $(srcdir)/config/atof-vax.c
DEPTC_alpha_evax = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-evax.h \
  $(srcdir)/config/tc-alpha.h emul.h subsegs.h $(INCDIR)/obstack.h \
  struc-symbol.h ecoff.h $(INCDIR)/opcode/alpha.h $(srcdir)/config/atof-vax.c
DEPTC_arc_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-arc.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/arc.h $(INCDIR)/elf/arc.h \
  $(INCDIR)/elf/reloc-macros.h
DEPTC_arc_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arc.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/arc.h \
  $(INCDIR)/elf/arc.h $(INCDIR)/elf/reloc-macros.h
DEPTC_arm_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-arm.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h
DEPTC_arm_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-arm.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/arm.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h
DEPTC_arm_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arm.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/elf/arm.h \
  $(INCDIR)/elf/reloc-macros.h
DEPTC_avr_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-avr.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h
DEPTC_avr_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-avr.h \
  emul.h subsegs.h $(INCDIR)/obstack.h
DEPTC_d10v_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-d10v.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/d10v.h $(INCDIR)/elf/ppc.h \
  $(INCDIR)/elf/reloc-macros.h
DEPTC_d10v_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d10v.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/d10v.h \
  $(INCDIR)/elf/ppc.h $(INCDIR)/elf/reloc-macros.h
DEPTC_d30v_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-d30v.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/d30v.h
DEPTC_d30v_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d30v.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/d30v.h
DEPTC_fr30_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-fr30.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/symcat.h $(srcdir)/../opcodes/fr30-desc.h \
  $(INCDIR)/opcode/cgen.h $(srcdir)/../opcodes/fr30-opc.h \
  cgen.h
DEPTC_fr30_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-fr30.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/symcat.h \
  $(srcdir)/../opcodes/fr30-desc.h $(INCDIR)/opcode/cgen.h \
  $(srcdir)/../opcodes/fr30-opc.h cgen.h
DEPTC_h8300_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-h8300.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/h8300.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/h8300.h
DEPTC_h8300_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8300.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/h8300.h
DEPTC_h8500_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-h8500.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/h8500.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/h8500-opc.h
DEPTC_h8500_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8500.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/h8500-opc.h
DEPTC_hppa_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-hppa.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(BFDDIR)/libhppa.h $(BFDDIR)/libbfd.h \
  $(INCDIR)/opcode/hppa.h
DEPTC_i370_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i370.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h struc-symbol.h $(INCDIR)/opcode/i370.h
DEPTC_i370_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i370.h \
  emul.h subsegs.h $(INCDIR)/obstack.h struc-symbol.h \
  $(INCDIR)/opcode/i370.h $(INCDIR)/elf/i370.h
DEPTC_i386_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-i386.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/i386.h
DEPTC_i386_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i386.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/i386.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/i386.h
DEPTC_i386_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i386.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/i386.h
DEPTC_i860_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-i860.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/opcode/i860.h
DEPTC_i860_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i860.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/opcode/i860.h
DEPTC_i860_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i860.h \
  emul.h $(INCDIR)/opcode/i860.h
DEPTC_i960_bout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-bout.h \
  $(srcdir)/config/tc-i960.h emul.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/i960.h
DEPTC_i960_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i960.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/i960.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h $(INCDIR)/opcode/i960.h
DEPTC_i960_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i960.h \
  emul.h $(INCDIR)/obstack.h $(INCDIR)/opcode/i960.h
DEPTC_m32r_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-m32r.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/symcat.h $(srcdir)/../opcodes/m32r-desc.h \
  $(INCDIR)/opcode/cgen.h $(srcdir)/../opcodes/m32r-opc.h \
  cgen.h
DEPTC_m32r_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m32r.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/symcat.h \
  $(srcdir)/../opcodes/m32r-desc.h $(INCDIR)/opcode/cgen.h \
  $(srcdir)/../opcodes/m32r-opc.h cgen.h
DEPTC_m68k_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-m68k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h $(INCDIR)/opcode/m68k.h \
  $(srcdir)/config/m68k-parse.h
DEPTC_m68k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-m68k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/m68k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h $(INCDIR)/opcode/m68k.h \
  $(srcdir)/config/m68k-parse.h
DEPTC_m68k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m68k.h \
  emul.h $(INCDIR)/obstack.h subsegs.h $(INCDIR)/opcode/m68k.h \
  $(srcdir)/config/m68k-parse.h $(INCDIR)/elf/m68k.h \
  $(INCDIR)/elf/reloc-macros.h
DEPTC_m68k_hp300 = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-hp300.h \
  $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-m68k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h $(INCDIR)/opcode/m68k.h $(srcdir)/config/m68k-parse.h
DEPTC_m88k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-m88k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/m88k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/config/m88k-opcode.h
DEPTC_m88k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m88k.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/config/m88k-opcode.h
DEPTC_mcore_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mcore.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/mcore.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/mcore-opc.h
DEPTC_mcore_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mcore.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/mcore-opc.h \
  $(INCDIR)/elf/mcore.h $(INCDIR)/elf/reloc-macros.h
DEPTC_mips_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-mips.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mips.h \
  itbl-ops.h $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/mips.h $(INCDIR)/elf/reloc-macros.h ecoff.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h
DEPTC_mips_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mips.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/mipspe.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mips.h \
  itbl-ops.h $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/elf/mips.h $(INCDIR)/elf/reloc-macros.h ecoff.h \
  $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h
DEPTC_mips_ecoff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-ecoff.h \
  $(srcdir)/config/tc-mips.h ecoff.h $(INCDIR)/coff/sym.h \
  $(INCDIR)/coff/ecoff.h emul.h subsegs.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/mips.h itbl-ops.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(INCDIR)/elf/mips.h \
  $(INCDIR)/elf/reloc-macros.h
DEPTC_mips_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mips.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mips.h \
  itbl-ops.h $(INCDIR)/elf/mips.h $(INCDIR)/elf/reloc-macros.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h
DEPTC_mn10200_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mn10200.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10200.h
DEPTC_mn10200_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10200.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10200.h
DEPTC_mn10300_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mn10300.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10300.h
DEPTC_mn10300_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10300.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10300.h
DEPTC_ns32k_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-ns32k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/opcode/ns32k.h $(INCDIR)/obstack.h
DEPTC_ns32k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-ns32k.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/opcode/ns32k.h \
  $(INCDIR)/obstack.h
DEPTC_ns32k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ns32k.h \
  emul.h $(INCDIR)/opcode/ns32k.h $(INCDIR)/obstack.h
DEPTC_pj_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-pj.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/opcode/pj.h
DEPTC_pj_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-pj.h \
  emul.h $(INCDIR)/opcode/pj.h
DEPTC_ppc_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-ppc.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/rs6000.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/ppc.h
DEPTC_ppc_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ppc.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/ppc.h \
  $(INCDIR)/elf/ppc.h $(INCDIR)/elf/reloc-macros.h
DEPTC_sh_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-sh.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sh.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/sh-opc.h
DEPTC_sh_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sh.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/sh-opc.h \
  $(INCDIR)/elf/sh.h $(INCDIR)/elf/reloc-macros.h
DEPTC_sparc_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-sparc.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/sparc.h
DEPTC_sparc_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-sparc.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sparc.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/sparc.h
DEPTC_sparc_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sparc.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/sparc.h \
  $(INCDIR)/elf/sparc.h $(INCDIR)/elf/reloc-macros.h
DEPTC_tahoe_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-tahoe.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h $(INCDIR)/opcode/tahoe.h
DEPTC_tahoe_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-tahoe.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/tahoe.h
DEPTC_tahoe_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tahoe.h \
  emul.h $(INCDIR)/obstack.h $(INCDIR)/opcode/tahoe.h
DEPTC_tic30_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-tic30.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/opcode/tic30.h
DEPTC_tic30_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-tic30.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/tic30.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/opcode/tic30.h
DEPTC_tic30_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic30.h \
  emul.h $(INCDIR)/opcode/tic30.h
DEPTC_tic80_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-tic80.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/tic80.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/opcode/tic80.h
DEPTC_tic80_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic80.h \
  emul.h $(INCDIR)/opcode/tic80.h
DEPTC_vax_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-vax.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(srcdir)/config/vax-inst.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/vax.h
DEPTC_vax_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-vax.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(srcdir)/config/vax-inst.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/vax.h
DEPTC_vax_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-vax.h \
  emul.h $(srcdir)/config/vax-inst.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/vax.h
DEPTC_vax_vms = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-vms.h \
  $(srcdir)/config/tc-vax.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def emul.h $(srcdir)/config/vax-inst.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/vax.h
DEPTC_w65_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-w65.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/w65.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/w65-opc.h
DEPTC_w65_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-w65.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/w65-opc.h
DEPTC_v850_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-v850.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/v850.h
DEPTC_v850_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-v850.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/v850.h
DEPTC_z8k_coff = $(srcdir)/../opcodes/z8k-opc.h $(INCDIR)/bin-bugs.h \
  $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-z8k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/z8k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h emul.h
DEPTC_z8k_elf = $(srcdir)/../opcodes/z8k-opc.h $(INCDIR)/bin-bugs.h \
  $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h \
  $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h \
  $(srcdir)/config/tc-z8k.h emul.h
DEPTC_hppa_som = $(srcdir)/config/tc-hppa.h subsegs.h \
  $(INCDIR)/obstack.h $(BFDDIR)/libhppa.h $(INCDIR)/opcode/hppa.h \
  $(BFDDIR)/som.h
DEPTC_i386_multi = $(DEPTC_i386_aout) $(DEPTC_i386_coff) \
  $(DEPTC_i386_elf)
DEPTC_mips_multi = $(DEPTC_mips_coff) $(DEPTC_mips_ecoff) \
  $(DEPTC_mips_elf)
DEPOBJ_a29k_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-a29k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_a29k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-a29k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/a29k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_a29k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-a29k.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_alpha_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-alpha.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_alpha_ecoff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-ecoff.h \
  $(srcdir)/config/tc-alpha.h ecoff.h $(INCDIR)/coff/sym.h \
  $(INCDIR)/coff/ecoff.h emul.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h $(BFDDIR)/libecoff.h
DEPOBJ_alpha_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-alpha.h \
  emul.h subsegs.h $(INCDIR)/obstack.h ecoff.h $(INCDIR)/coff/sym.h \
  $(INCDIR)/coff/ecoff.h $(INCDIR)/elf/alpha.h $(INCDIR)/elf/reloc-macros.h \
  $(INCDIR)/aout/aout64.h
DEPOBJ_alpha_evax = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-evax.h \
  $(srcdir)/config/tc-alpha.h emul.h
DEPOBJ_arc_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-arc.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_arc_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arc.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_arm_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-arm.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_arm_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-arm.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/arm.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_arm_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arm.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_avr_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-avr.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_avr_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-avr.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_d10v_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-d10v.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_d10v_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d10v.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_d30v_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-d30v.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_d30v_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d30v.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_fr30_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-fr30.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_fr30_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-fr30.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_h8300_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-h8300.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/h8300.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_h8300_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8300.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_h8500_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-h8500.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/h8500.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_h8500_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8500.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_hppa_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-hppa.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_i370_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i370.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_i370_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i370.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/elf/i370.h \
  $(INCDIR)/aout/aout64.h
DEPOBJ_i386_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-i386.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_i386_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i386.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/i386.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_i386_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i386.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_i860_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-i860.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_i860_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i860.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_i860_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i860.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_i960_bout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-bout.h \
  $(srcdir)/config/tc-i960.h emul.h $(INCDIR)/obstack.h
DEPOBJ_i960_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-i960.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/i960.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_i960_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i960.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_m32r_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-m32r.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_m32r_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m32r.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_m68k_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-m68k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_m68k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-m68k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/m68k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_m68k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m68k.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_m68k_hp300 = $(srcdir)/config/obj-aout.c $(INCDIR)/bin-bugs.h \
  $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-m68k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
DEPOBJ_m88k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-m88k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/m88k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_m88k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m88k.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_mcore_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mcore.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/mcore.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_mcore_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mcore.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_mips_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-mips.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_mips_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mips.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/mipspe.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_mips_ecoff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-ecoff.h \
  $(srcdir)/config/tc-mips.h ecoff.h $(INCDIR)/coff/sym.h \
  $(INCDIR)/coff/ecoff.h emul.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h $(BFDDIR)/libecoff.h
DEPOBJ_mips_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mips.h \
  emul.h subsegs.h $(INCDIR)/obstack.h ecoff.h $(INCDIR)/coff/sym.h \
  $(INCDIR)/coff/ecoff.h $(INCDIR)/elf/mips.h $(INCDIR)/elf/reloc-macros.h \
  $(INCDIR)/aout/aout64.h
DEPOBJ_mn10200_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mn10200.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_mn10200_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10200.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_mn10300_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-mn10300.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_mn10300_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10300.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_ns32k_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-ns32k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_ns32k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-ns32k.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_ns32k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ns32k.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_pj_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-pj.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_pj_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-pj.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_ppc_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-ppc.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/rs6000.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_ppc_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ppc.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/elf/ppc.h \
  $(INCDIR)/elf/reloc-macros.h $(INCDIR)/aout/aout64.h
DEPOBJ_sh_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-sh.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sh.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_sh_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sh.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_sparc_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-sparc.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_sparc_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-sparc.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/sparc.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_sparc_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sparc.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_tahoe_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-tahoe.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_tahoe_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-tahoe.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_tahoe_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tahoe.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_tic30_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-tic30.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_tic30_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-tic30.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/tic30.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_tic30_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic30.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_tic80_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-tic80.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/tic80.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_tic80_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic80.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_vax_aout = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-vax.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/aout/aout64.h $(INCDIR)/obstack.h
DEPOBJ_vax_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-vax.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_vax_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-vax.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_vax_vms = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-vms.h \
  $(srcdir)/config/tc-vax.h $(INCDIR)/aout/stab_gnu.h \
  $(INCDIR)/aout/stab.def emul.h subsegs.h $(INCDIR)/obstack.h
DEPOBJ_w65_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-w65.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/w65.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_w65_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-w65.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_v850_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-v850.h $(INCDIR)/coff/internal.h \
  $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h emul.h $(INCDIR)/obstack.h \
  subsegs.h
DEPOBJ_v850_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-v850.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_z8k_coff = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-z8k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/z8k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  emul.h $(INCDIR)/obstack.h subsegs.h
DEPOBJ_z8k_elf = $(INCDIR)/bin-bugs.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-z8k.h \
  emul.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
DEPOBJ_hppa_som = $(srcdir)/config/obj-som.h subsegs.h \
  $(INCDIR)/obstack.h $(BFDDIR)/libhppa.h $(BFDDIR)/som.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def
DEPOBJ_i386_multi = $(DEPOBJ_i386_aout) $(DEPOBJ_i386_coff) \
  $(DEPOBJ_i386_elf)
DEPOBJ_mips_multi = $(DEPOBJ_mips_coff) $(DEPOBJ_mips_ecoff) \
  $(DEPOBJ_mips_elf)
DEP_a29k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-a29k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_a29k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-a29k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/a29k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_a29k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-a29k.h
DEP_alpha_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-alpha.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_alpha_ecoff = $(srcdir)/config/obj-ecoff.h $(srcdir)/config/tc-alpha.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h
DEP_alpha_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-alpha.h
DEP_alpha_evax = $(srcdir)/config/obj-evax.h $(srcdir)/config/tc-alpha.h
DEP_arc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-arc.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_arc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arc.h
DEP_arm_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-arm.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_arm_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-arm.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/arm.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_arm_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arm.h
DEP_avr_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-avr.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_avr_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-avr.h
DEP_d10v_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-d10v.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_d10v_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d10v.h
DEP_d30v_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-d30v.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_d30v_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d30v.h
DEP_fr30_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-fr30.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_fr30_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-fr30.h
DEP_h8300_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-h8300.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/h8300.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_h8300_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8300.h
DEP_h8500_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-h8500.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/h8500.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_h8500_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8500.h
DEP_hppa_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-hppa.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_i370_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i370.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_i370_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i370.h
DEP_i386_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-i386.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_i386_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i386.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/i386.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_i386_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i386.h
DEP_i860_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-i860.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_i860_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i860.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_i860_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i860.h
DEP_i960_bout = $(srcdir)/config/obj-bout.h $(srcdir)/config/tc-i960.h
DEP_i960_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i960.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/i960.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_i960_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i960.h
DEP_m32r_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m32r.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_m32r_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m32r.h
DEP_m68k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-m68k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_m68k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m68k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/m68k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_m68k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m68k.h
DEP_m68k_hp300 = $(srcdir)/config/obj-hp300.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-m68k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_m88k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m88k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/m88k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_m88k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m88k.h
DEP_mcore_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mcore.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/mcore.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_mcore_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mcore.h
DEP_mips_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-mips.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_mips_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mips.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/mipspe.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_mips_ecoff = $(srcdir)/config/obj-ecoff.h $(srcdir)/config/tc-mips.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h
DEP_mips_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mips.h
DEP_mn10200_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mn10200.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_mn10200_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10200.h
DEP_mn10300_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mn10300.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_mn10300_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10300.h
DEP_ns32k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-ns32k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_ns32k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-ns32k.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_ns32k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ns32k.h
DEP_pj_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-pj.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_pj_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-pj.h
DEP_ppc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-ppc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/rs6000.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_ppc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ppc.h
DEP_sh_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-sh.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sh.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_sh_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sh.h
DEP_sparc_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-sparc.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_sparc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-sparc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sparc.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_sparc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sparc.h
DEP_tahoe_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-tahoe.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_tahoe_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-tahoe.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_tahoe_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tahoe.h
DEP_tic30_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-tic30.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_tic30_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-tic30.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/tic30.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_tic30_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic30.h
DEP_tic80_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-tic80.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/tic80.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_tic80_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic80.h
DEP_vax_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-vax.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_vax_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-vax.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_vax_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-vax.h
DEP_vax_vms = $(srcdir)/config/obj-vms.h $(srcdir)/config/tc-vax.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def
DEP_w65_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-w65.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/w65.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_w65_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-w65.h
DEP_v850_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-v850.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_v850_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-v850.h
DEP_z8k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-z8k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/z8k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h
DEP_z8k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-z8k.h
DEP_hppa_som = $(BFDDIR)/som.h
DEP_i386_multi = $(DEP_i386_aout) $(DEP_i386_coff) \
  $(DEP_i386_elf)
DEP_mips_multi = $(DEP_mips_coff) $(DEP_mips_ecoff) \
  $(DEP_mips_elf)
$(OBJS): $(DEP_@target_cpu_type@_@obj_format@)
$(TARG_CPU_O): $(DEPTC_@target_cpu_type@_@obj_format@)
$(OBJ_FORMAT_O): $(DEPOBJ_@target_cpu_type@_@obj_format@)
# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
