## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus dejagnu

SUBDIRS = doc

tooldir = $(exec_prefix)/$(target_alias)

YACC = `if [ -f ../bison/bison ] ; then echo ../bison/bison -y -L../bison/bison ; else echo bison -y ; fi`
LEX = `if [ -f ../flex/flex ] ; then echo ../flex/flex ;  else echo flex ; fi`

DEP = mkdep

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
	d10v \
	h8300 \
	h8500 \
	hppa \
	i386 \
	i860 \
	i960 \
	m32r \
	m68k \
	m88k \
	mips \
	mn10200 \
	mn10300 \
	ns32k \
	ppc \
	sh \
	sparc \
	tahoe \
	tic30 \
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

# This is like CPU_OBJ_VALID, for the obj=multi case.

CPU_MULTI_VALID = \
	valid= ; \
	case $$c in \
	i386 | mips) valid=yes ;; \
	esac;

# Regular source files.

GAS_CFILES = \
	app.c \
	as.c \
	atof-generic.c \
	bignum-copy.c \
	cond.c \
	depend.c \
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
	bignum.h \
	bit_fix.h \
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
	config/tc-d10v.c \
	config/tc-h8300.c \
	config/tc-h8500.c \
	config/tc-hppa.c \
	config/tc-i386.c \
	config/tc-i860.c \
	config/tc-i960.c \
	config/tc-m32r.c \
	config/tc-m68k.c \
	config/tc-m88k.c \
	config/tc-mips.c \
	config/tc-mn10200.c \
	config/tc-mn10300.c \
	config/tc-ns32k.c \
	config/tc-ppc.c \
	config/tc-sh.c \
	config/tc-sparc.c \
	config/tc-tahoe.c \
	config/tc-tic30.c \
	config/tc-vax.c \
	config/tc-w65.c \
	config/tc-v850.c \
	config/tc-z8k.c

TARGET_CPU_HFILES = \
	config/tc-a29k.h \
	config/tc-alpha.h \
	config/tc-arc.h \
	config/tc-arm.h \
	config/tc-d10v.h \
	config/tc-h8300.h \
	config/tc-h8500.h \
	config/tc-hppa.h \
	config/tc-i386.h \
	config/tc-i860.h \
	config/tc-i960.h \
	config/tc-m32r.h \
	config/tc-m68k.h \
	config/tc-m88k.h \
	config/tc-mips.h \
	config/tc-mn10200.h \
	config/tc-mn10300.h \
	config/tc-ns32k.h \
	config/tc-ppc.h \
	config/tc-sh.h \
	config/tc-sparc.h \
	config/tc-tahoe.h \
	config/tc-tic30.h \
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

noinst_PROGRAMS = as-new gasp-new
noinst_SCRIPTS = .gdbinit

$(srcdir)/make-gas.com: stamp-mk.com
stamp-mk.com: vmsconf.sh Makefile
	sh $(srcdir)/vmsconf.sh $(GENERIC_OBJS) > new-make.com
	$(SHELL) $(srcdir)/../move-if-change new-make.com $(srcdir)/make-gas.com
	touch stamp-mk.com

EXTRA_DIST = make-gas.com

DISTSTUFF = make-gas.com m68k-parse.c itbl-parse.c itbl-parse.h itbl-lex.c
diststuff: $(DISTSTUFF) info

DISTCLEANFILES = targ-cpu.h obj-format.h targ-env.h itbl-cpu.h cgen-opc.h

# Now figure out from those variables how to compile and link.

BASEDIR = $(srcdir)/..
BFDDIR = $(BASEDIR)/bfd
INCDIR = $(BASEDIR)/include

# This is the variable actually used when we compile.
# Specify the directories to be searched for header files.
# Both . and srcdir are used, in that order,
# so that tm.h and config.h will be found in the compilation
# subdirectory rather than in the source directory.
INCLUDES = -D_GNU_SOURCE -I. -I$(srcdir) -I../bfd -I$(srcdir)/config -I$(INCDIR) -I$(srcdir)/.. -I$(BFDDIR)

# This should be parallel to INCLUDES, but should replace $(srcdir)
# with $${srcdir}, and should work in a subdirectory.  This is used
# when building dependencies, because the dependency building is done
# in a subdirectory.
DEP_INCLUDES = -D_GNU_SOURCE -I.. -I$${srcdir} -I../../bfd -I$${srcdir}/config -I$${srcdir}/../include -I$${srcdir}/.. -I$${srcdir}/../bfd

# How to link with both our special library facilities
# and the system's installed libraries.

GASLIBS = @OPCODES_LIB@ @BFDLIB@ ../libiberty/libiberty.a

# Files to be copied away after each stage in building.
STAGESTUFF = *.o $(noinst_PROGRAMS)

$(OBJS): @ALL_OBJ_DEPS@

as_new_SOURCES = $(GAS_CFILES)
as_new_LDADD = $(TARG_CPU_O) $(OBJ_FORMAT_O) $(ATOF_TARG_O) \
	$(extra_objects) $(GASLIBS)
as_new_DEPENDENCIES = $(TARG_CPU_O) $(OBJ_FORMAT_O) $(ATOF_TARG_O) \
	$(extra_objects) $(GASLIBS)

# Stuff that every object file depends upon.  If anything is removed
# from this list, remove it from dep-in.sed as well.
$(OBJS): config.h as.h $(TARG_ENV_H) $(OBJ_FORMAT_H) $(TARG_CPU_H) flonum.h \
	expr.h struc-symbol.h write.h frags.h hash.h read.h symbols.h tc.h \
	obj.h listing.h bignum.h bit_fix.h $(INCDIR)/libiberty.h

gasp_new_SOURCES = gasp.c macro.c sb.c hash.c
gasp_new_LDADD = ../libiberty/libiberty.a

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
e-i386coff.o: $(srcdir)/config/e-i386coff.c
	$(COMPILE) -c $(srcdir)/config/e-i386coff.c
e-i386elf.o: $(srcdir)/config/e-i386elf.c
	$(COMPILE) -c $(srcdir)/config/e-i386elf.c

# The m68k operand parser.

EXTRA_as_new_SOURCES = config/m68k-parse.y

m68k-parse.c: $(srcdir)/config/m68k-parse.y
	$(SHELL) $(YLWRAP) "$(YACC)" $(srcdir)/config/m68k-parse.y y.tab.c m68k-parse.c --
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
itbl_test_LDADD = itbl-test-ops.o itbl-test.o $(GASLIBS) @LEXLIB@

itbl-test-ops.o: $(srcdir)/itbl-ops.c $(srcdir)/itbl-ops.h itbl-parse.h
	$(COMPILE) -o itbl-test-ops.o -DSTAND_ALONE -c $(srcdir)/itbl-ops.c

itbl-test.o: $(srcdir)/testsuite/gas/all/itbl-test.c $(srcdir)/itbl-ops.h
	$(COMPILE) -c -DSTAND_ALONE $(srcdir)/testsuite/gas/all/itbl-test.c

# CGEN interface.

cgen.o: cgen.c cgen-opc.h subsegs.h \
	$(INCDIR)/obstack.h $(INCDIR)/opcode/cgen.h \
	$(srcdir)/../opcodes/$(TARG_CPU)-opc.h

# Remake the info files.

MOSTLYCLEANFILES = $(STAGESTUFF) core stamp-mk.com \
	testsuite/*.o testsuite/*.out testsuite/gas.log testsuite/gas.sum \
	testsuite/site.exp site.bak site.exp stage stage1 stage2

CLEANFILES = dep.sed .tcdep .objdep .dep2 .dep1 .depa .dep .depdir

.PHONY: install-exec-local install-data-local

install-exec-local: $(noinst_PROGRAMS)
	$(mkinstalldirs) $(bindir) $(tooldir)/bin
	@list='$(noinst_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(bindir)/`echo $$p|sed -e 's/-new//'|sed '$(transform)'`"; \
	    $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(bindir)/`echo $$p|sed -e 's/-new//'|sed '$(transform)'`; \
	  else :; fi; \
	done
	rm -f $(tooldir)/bin/as$(EXEEXT)
	n=`echo as | sed '$(transform)'`; \
	if [ "$(bindir)/$$n$(EXEEXT)" != "$(tooldir)/bin/as$(EXEEXT)" ]; then \
	  ln $(bindir)/$$n$(EXEEXT) $(tooldir)/bin/as$(EXEEXT) >/dev/null 2>/dev/null \
	    || $(LIBTOOL) --mode=install $(INSTALL_PROGRAM) as-new$(EXEEXT) $(tooldir)/bin/as$(EXEEXT); \
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

.dep: dep.sed $(DEP_FILE_DEPS) .tcdep .objdep .dep2
	rm -f .dep1
	srcdir=`cd $(srcdir); pwd`; \
	$(MAKE) DEP=$(DEP) srcdir=$${srcdir} VPATH=$${srcdir} .dep1
	rm -rf .depdir
	sed -f dep.sed < .dep1 > .depa
	sed -f dep.sed < .tcdep >> .depa
	sed -f dep.sed < .objdep >> .depa
	sed -f dep.sed < .dep2 >> .depa
	echo '$$(OBJS): $$(DEP_@target''_cpu_type@_@obj''_format@)' >> .depa
	echo '$$(TARG_CPU_O): $$(TCDEP_@target''_cpu_type@_@obj''_format@)' >> .depa
	echo '$$(OBJ_FORMAT_O): $$(OBJDEP_@target''_cpu_type@_@obj''_format@)' >> .depa
	echo '# IF YOU PUT ANYTHING HERE IT WILL GO AWAY' >> .depa
	$(SHELL) $(srcdir)/../move-if-change .depa .dep

# This rule needs a mkdep that runs "gcc -MM".
.dep1: $(CFILES) $(MULTI_CFILES)
	if [ -d .depdir ]; then true; else mkdir .depdir; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd .depdir; \
	echo '' > targ-cpu.h; \
	echo '' > obj-format.h; \
	echo '' > targ-env.h; \
	echo '' > itbl-cpu.h; \
	echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > .dep; \
	$(DEP) -f .dep -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) $?
	sed -e '/IF YOU PUT ANYTHING/,$$d' < .depdir/.dep > .dep1
	rm -f .depdir/.dep

# Work out the special dependencies for the tc-*.c files.
.tcdep: $(TARGET_CPU_CFILES)
	rm -f .tcdepa
	if [ -d .depdir ]; then true; else mkdir .depdir; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd .depdir; \
	for c in $(CPU_TYPES); do \
	  for o in $(OBJ_FORMATS); do \
	    $(CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '#include "tc-'"$${c}"'.h"' > targ-cpu.h; \
	      echo '#include "obj-'"$${o}"'.h"' > obj-format.h; \
	      echo '#include "te-generic.h"' > targ-env.h; \
	      echo '' > itbl-cpu.h; \
	      echo '#include "opcodes/'"$${c}"'-opc.h"' > cgen-opc.h; \
	      rm -f dummy.c; \
	      cp $${srcdir}/config/tc-$${c}.c dummy.c; \
	      echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > .dep; \
	      $(DEP) -f .dep -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) dummy.c; \
	      sed -e "s/dummy.o: dummy.c/TCDEP_$${c}_$${o} =/" \
		  -e '1,/DO NOT PUT ANYTHING AFTER/d' \
		  -e '/IF YOU PUT ANYTHING/,$$d' \
		  -e '/^$$/d' < .dep >> ../.tcdepa; \
	      rm -f dummy.c; \
	    else true; fi; \
	  done; \
	done
	echo 'TCDEP_hppa_som = $$(srcdir)/config/tc-hppa.h subsegs.h \' >> .tcdepa
	echo '  $$(INCDIR)/obstack.h $$(BFDDIR)/libhppa.h \' >> .tcdepa
	echo '  $$(INCDIR)/opcode/hppa.h $$(BFDDIR)/som.h' >> .tcdepa
	# We don't try to handle all multi cases.
	for c in $(CPU_TYPES); do \
	  $(CPU_MULTI_VALID) \
	  if [ x$${valid} = xyes ]; then \
	    o=ecoff; \
	    $(CPU_OBJ_VALID) \
	    echo 'TCDEP_'"$${c}"'_multi = \' >> .tcdepa; \
	    echo '$$(TCDEP_'"$${c}"'_coff) \' >> .tcdepa; \
	    if [ x$${valid} = xyes ]; then \
	      echo '$$(TCDEP_'"$${c}"'_ecoff) \' >> .tcdepa; \
	    else true; fi; \
	    echo '$$(TCDEP_'"$${c}"'_elf)' >> .tcdepa; \
	  else true; fi; \
	done
	mv -f .tcdepa .tcdep

# Work out the special dependencies for the obj-*.c files.
.objdep: $(OBJ_FORMAT_CFILES)
	rm -f .objdepa
	if [ -d .depdir ]; then true; else mkdir .depdir; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd .depdir; \
	for c in $(CPU_TYPES); do \
	  for o in $(OBJ_FORMATS); do \
	    $(CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '#include "tc-'"$${c}"'.h"' > targ-cpu.h; \
	      echo '#include "obj-'"$${o}"'.h"' > obj-format.h; \
	      echo '#include "te-generic.h"' > targ-env.h; \
	      echo '' > itbl-cpu.h; \
	      rm -f dummy.c; \
	      cp $${srcdir}/config/obj-$${o}.c dummy.c; \
	      echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > .dep; \
	      $(DEP) -f .dep -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) dummy.c; \
	      sed -e "s/dummy.o: dummy.c/OBJDEP_$${c}_$${o} =/" \
		  -e '1,/DO NOT PUT ANYTHING AFTER/d' \
		  -e '/IF YOU PUT ANYTHING/,$$d' \
		  -e '/^$$/d' < .dep >> ../.objdepa; \
	      rm -f dummy.c; \
	    else true; fi; \
	  done; \
	done
	echo 'OBJDEP_hppa_som = $$(srcdir)/config/obj-som.h subsegs.h \' >> .objdepa
	echo '  $$(INCDIR)/obstack.h $$(BFDDIR)/libhppa.h \' >> .objdepa
	echo '  $$(BFDDIR)/som.h  $$(INCDIR)/aout/stab_gnu.h \' >> .objdepa
	echo '  $$(INCDIR)/aout/stab.def' >> .objdepa
	# We don't try to handle all multi cases.
	for c in $(CPU_TYPES); do \
	  $(CPU_MULTI_VALID) \
	  if [ x$${valid} = xyes ]; then \
	    o=ecoff; \
	    $(CPU_OBJ_VALID) \
	    echo 'OBJDEP_'"$${c}"'_multi = \' >> .objdepa; \
	    echo '$$(OBJDEP_'"$${c}"'_coff) \' >> .objdepa; \
	    if [ x$${valid} = xyes ]; then \
	      echo '$$(OBJDEP_'"$${c}"'_ecoff) \' >> .objdepa; \
	    else true; fi; \
	    echo '$$(OBJDEP_'"$${c}"'_elf)' >> .objdepa; \
	  else true; fi; \
	done
	mv -f .objdepa .objdep

# Work out the dependencies for each CPU/OBJ combination.
# Note that SOM is a special case, because it only works native.
.dep2: $(TARGET_CPU_HFILES) $(OBJ_FORMAT_HFILES)
	rm -f .dep2a
	if [ -d .depdir ]; then true; else mkdir .depdir; fi
	srcdir=`cd $(srcdir); pwd`; \
	cd .depdir; \
	for c in $(CPU_TYPES); do \
	  for o in $(OBJ_FORMATS); do \
	    $(CPU_OBJ_VALID) \
	    if [ x$${valid} = xyes ]; then \
	      echo '#include "tc-'"$${c}"'.h"' > targ-cpu.h; \
	      echo '#include "obj-'"$${o}"'.h"' > dummy.c; \
	      echo '# DO NOT DELETE THIS LINE -- mkdep uses it.' > .dep; \
	      $(DEP) -f .dep -DBFD_ASSEMBLER -I. -I.. -I$${srcdir} -I../../bfd $(DEP_INCLUDES) dummy.c; \
	      sed -e "s/dummy.o: dummy.c/DEP_$${c}_$${o} =/" \
		  -e '1,/DO NOT PUT ANYTHING AFTER/d' \
		  -e '/IF YOU PUT ANYTHING/,$$d' \
		  -e '/^$$/d' < .dep >> ../.dep2a; \
	    else true; fi; \
	  done; \
	done
	echo 'DEP_hppa_som = $$(BFDDIR)/som.h' >> .dep2a
	# We don't try to handle all multi cases.
	for c in $(CPU_TYPES); do \
	  $(CPU_MULTI_VALID) \
	  if [ x$${valid} = xyes ]; then \
	    o=ecoff; \
	    $(CPU_OBJ_VALID) \
	    echo 'DEP_'"$${c}"'_multi = \' >> .dep2a; \
	    echo '$$(DEP_'"$${c}"'_coff) \' >> .dep2a; \
	    if [ x$${valid} = xyes ]; then \
	      echo '$$(DEP_'"$${c}"'_ecoff) \' >> .dep2a; \
	    else true; fi; \
	    echo '$$(DEP_'"$${c}"'_elf)' >> .dep2a; \
	  else true; fi; \
	done
	mv -f .dep2a .dep2

dep.sed: dep-in.sed config.status
	srcdir=`cd $(srcdir); pwd`; \
	sed <$(srcdir)/dep-in.sed >dep.sed \
		-e "s!@INCDIR@!$${srcdir}/../include!" \
		-e "s!@BFDDIR@!$${srcdir}/../bfd!" \
		-e "s!@SRCDIR@!$${srcdir}!"

dep: .dep
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < Makefile > tmp-Makefile
	cat .dep >> tmp-Makefile
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile Makefile

dep-in: .dep
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.in > tmp-Makefile.in
	cat .dep >> tmp-Makefile.in
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile.in $(srcdir)/Makefile.in

dep-am: .dep
	sed -e '/^..DO NOT DELETE THIS LINE/,$$d' < $(srcdir)/Makefile.am > tmp-Makefile.am
	cat .dep >> tmp-Makefile.am
	$(SHELL) $(srcdir)/../move-if-change tmp-Makefile.am $(srcdir)/Makefile.am

.PHONY: dep dep-in dep-am

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

app.o: app.c
as.o: as.c subsegs.h $(INCDIR)/obstack.h output-file.h \
  sb.h macro.h
atof-generic.o: atof-generic.c
bignum-copy.o: bignum-copy.c
cond.o: cond.c macro.h sb.h $(INCDIR)/obstack.h
depend.o: depend.c
ecoff.o: ecoff.c
ehopt.o: ehopt.c subsegs.h $(INCDIR)/obstack.h $(INCDIR)/elf/dwarf2.h
expr.o: expr.c $(INCDIR)/obstack.h
flonum-copy.o: flonum-copy.c
flonum-konst.o: flonum-konst.c
flonum-mult.o: flonum-mult.c
frags.o: frags.c subsegs.h $(INCDIR)/obstack.h
hash.o: hash.c
input-file.o: input-file.c input-file.h
input-scrub.o: input-scrub.c input-file.h sb.h
listing.o: listing.c input-file.h subsegs.h
literal.o: literal.c subsegs.h $(INCDIR)/obstack.h
macro.o: macro.c sb.h macro.h
messages.o: messages.c
output-file.o: output-file.c output-file.h
read.o: read.c subsegs.h $(INCDIR)/obstack.h sb.h macro.h \
  ecoff.h
sb.o: sb.c sb.h
stabs.o: stabs.c $(INCDIR)/obstack.h subsegs.h ecoff.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def
subsegs.o: subsegs.c subsegs.h $(INCDIR)/obstack.h
symbols.o: symbols.c $(INCDIR)/obstack.h subsegs.h
write.o: write.c subsegs.h $(INCDIR)/obstack.h output-file.h
gasp.o: gasp.c sb.h macro.h
e-i386coff.o: $(srcdir)/config/e-i386coff.c emul.h \
  emul-target.h
e-i386elf.o: $(srcdir)/config/e-i386elf.c emul.h emul-target.h
e-mipsecoff.o: $(srcdir)/config/e-mipsecoff.c emul.h \
  emul-target.h
e-mipself.o: $(srcdir)/config/e-mipself.c emul.h emul-target.h

TCDEP_a29k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-a29k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/opcode/a29k.h
TCDEP_a29k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-a29k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/a29k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/opcode/a29k.h
TCDEP_a29k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-a29k.h $(INCDIR)/opcode/a29k.h
TCDEP_alpha_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-alpha.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/alpha.h \
  $(srcdir)/config/atof-vax.c
TCDEP_alpha_ecoff = $(srcdir)/config/obj-ecoff.h $(srcdir)/config/tc-alpha.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/alpha.h \
  $(srcdir)/config/atof-vax.c
TCDEP_alpha_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-alpha.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/alpha.h $(INCDIR)/elf/alpha.h \
  $(srcdir)/config/atof-vax.c
TCDEP_alpha_evax = $(srcdir)/config/obj-evax.h $(srcdir)/config/tc-alpha.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/alpha.h \
  $(srcdir)/config/atof-vax.c
TCDEP_arc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-arc.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/arc.h \
  $(INCDIR)/elf/arc.h
TCDEP_arc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arc.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/arc.h $(INCDIR)/elf/arc.h
TCDEP_arm_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-arm.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h
TCDEP_arm_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-arm.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/arm.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h
TCDEP_arm_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arm.h subsegs.h \
  $(INCDIR)/obstack.h
TCDEP_d10v_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-d10v.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/d10v.h \
  $(INCDIR)/elf/ppc.h
TCDEP_d10v_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d10v.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/d10v.h $(INCDIR)/elf/ppc.h
TCDEP_h8300_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-h8300.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/h8300.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/h8300.h
TCDEP_h8300_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8300.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/h8300.h
TCDEP_h8500_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-h8500.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/h8500.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/h8500-opc.h
TCDEP_h8500_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8500.h subsegs.h \
  $(INCDIR)/obstack.h $(srcdir)/../opcodes/h8500-opc.h
TCDEP_hppa_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-hppa.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(BFDDIR)/libhppa.h $(BFDDIR)/libbfd.h \
  $(INCDIR)/opcode/hppa.h
TCDEP_hppa_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-hppa.h $(BFDDIR)/elf32-hppa.h \
  $(BFDDIR)/libhppa.h $(INCDIR)/elf/hppa.h subsegs.h \
  $(INCDIR)/obstack.h $(BFDDIR)/libbfd.h $(INCDIR)/opcode/hppa.h
TCDEP_i386_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-i386.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/i386.h
TCDEP_i386_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i386.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/i386.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/i386.h
TCDEP_i386_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i386.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/i386.h
TCDEP_i860_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-i860.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/opcode/i860.h
TCDEP_i860_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i860.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/opcode/i860.h
TCDEP_i860_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i860.h $(INCDIR)/opcode/i860.h
TCDEP_i960_bout = $(srcdir)/config/obj-bout.h $(srcdir)/config/tc-i960.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/i960.h
TCDEP_i960_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i960.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/i960.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h $(INCDIR)/opcode/i960.h
TCDEP_i960_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i960.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/i960.h
TCDEP_m32r_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m32r.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/symcat.h cgen-opc.h \
  $(srcdir)/../opcodes/m32r-opc.h $(INCDIR)/opcode/cgen.h
TCDEP_m32r_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m32r.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/symcat.h cgen-opc.h $(srcdir)/../opcodes/m32r-opc.h \
  $(INCDIR)/opcode/cgen.h
TCDEP_m68k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-m68k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h \
  subsegs.h $(INCDIR)/opcode/m68k.h $(srcdir)/config/m68k-parse.h
TCDEP_m68k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m68k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/m68k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h $(INCDIR)/opcode/m68k.h \
  $(srcdir)/config/m68k-parse.h
TCDEP_m68k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m68k.h $(INCDIR)/obstack.h \
  subsegs.h $(INCDIR)/opcode/m68k.h $(srcdir)/config/m68k-parse.h
TCDEP_m68k_hp300 = $(srcdir)/config/obj-hp300.h $(srcdir)/config/obj-aout.h \
  $(srcdir)/config/tc-m68k.h $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h $(INCDIR)/opcode/m68k.h \
  $(srcdir)/config/m68k-parse.h
TCDEP_m88k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m88k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/m88k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/config/m88k-opcode.h
TCDEP_m88k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m88k.h subsegs.h \
  $(INCDIR)/obstack.h $(srcdir)/config/m88k-opcode.h
TCDEP_mips_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-mips.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/mips.h itbl-ops.h ecoff.h $(INCDIR)/coff/sym.h \
  $(INCDIR)/coff/ecoff.h
TCDEP_mips_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mips.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mips.h \
  itbl-ops.h ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h
TCDEP_mips_ecoff = $(srcdir)/config/obj-ecoff.h $(srcdir)/config/tc-mips.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mips.h \
  itbl-ops.h
TCDEP_mips_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mips.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/mips.h itbl-ops.h \
  $(INCDIR)/elf/mips.h ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h
TCDEP_mn10200_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mn10200.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10200.h
TCDEP_mn10200_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10200.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10200.h
TCDEP_mn10300_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mn10300.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10300.h
TCDEP_mn10300_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10300.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/mn10300.h
TCDEP_ns32k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-ns32k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/opcode/ns32k.h \
  $(INCDIR)/obstack.h
TCDEP_ns32k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-ns32k.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/opcode/ns32k.h $(INCDIR)/obstack.h
TCDEP_ns32k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ns32k.h $(INCDIR)/opcode/ns32k.h \
  $(INCDIR)/obstack.h
TCDEP_ppc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-ppc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/rs6000.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/ppc.h
TCDEP_ppc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ppc.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/ppc.h $(INCDIR)/elf/ppc.h
TCDEP_sh_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-sh.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sh.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/sh-opc.h
TCDEP_sh_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sh.h subsegs.h \
  $(INCDIR)/obstack.h $(srcdir)/../opcodes/sh-opc.h
TCDEP_sparc_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-sparc.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/sparc.h
TCDEP_sparc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-sparc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sparc.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/sparc.h
TCDEP_sparc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sparc.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/sparc.h $(INCDIR)/elf/sparc.h
TCDEP_tahoe_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-tahoe.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/tahoe.h
TCDEP_tahoe_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-tahoe.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/tahoe.h
TCDEP_tahoe_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tahoe.h $(INCDIR)/obstack.h \
  $(INCDIR)/opcode/tahoe.h
TCDEP_tic30_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-tic30.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/opcode/tic30.h
TCDEP_tic30_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-tic30.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/tic30.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/opcode/tic30.h
TCDEP_tic30_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic30.h $(INCDIR)/opcode/tic30.h
TCDEP_vax_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-vax.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(srcdir)/config/vax-inst.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/vax.h
TCDEP_vax_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-vax.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(srcdir)/config/vax-inst.h $(INCDIR)/obstack.h $(INCDIR)/opcode/vax.h
TCDEP_vax_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-vax.h $(srcdir)/config/vax-inst.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/vax.h
TCDEP_vax_vms = $(srcdir)/config/obj-vms.h $(srcdir)/config/tc-vax.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def $(srcdir)/config/vax-inst.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/vax.h
TCDEP_w65_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-w65.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/w65.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h subsegs.h $(INCDIR)/obstack.h $(srcdir)/../opcodes/w65-opc.h
TCDEP_w65_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-w65.h subsegs.h \
  $(INCDIR)/obstack.h $(srcdir)/../opcodes/w65-opc.h
TCDEP_v850_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-v850.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  subsegs.h $(INCDIR)/obstack.h $(INCDIR)/opcode/v850.h
TCDEP_v850_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-v850.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/opcode/v850.h
TCDEP_z8k_coff = $(srcdir)/../opcodes/z8k-opc.h $(srcdir)/config/obj-coff.h \
  $(srcdir)/config/tc-z8k.h $(INCDIR)/coff/internal.h \
  $(INCDIR)/coff/z8k.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
TCDEP_z8k_elf = $(srcdir)/../opcodes/z8k-opc.h $(srcdir)/config/obj-elf.h \
  $(BFDDIR)/elf-bfd.h $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h \
  $(INCDIR)/elf/external.h $(INCDIR)/bfdlink.h $(srcdir)/config/tc-z8k.h
TCDEP_hppa_som = $(srcdir)/config/tc-hppa.h subsegs.h \
  $(INCDIR)/obstack.h $(BFDDIR)/libhppa.h $(INCDIR)/opcode/hppa.h \
  $(BFDDIR)/som.h
TCDEP_i386_multi = $(TCDEP_i386_coff) $(TCDEP_i386_elf)
TCDEP_mips_multi = $(TCDEP_mips_coff) $(TCDEP_mips_ecoff) \
  $(TCDEP_mips_elf)
OBJDEP_a29k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-a29k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_a29k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-a29k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/a29k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_a29k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-a29k.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_alpha_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-alpha.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_alpha_ecoff = $(srcdir)/config/obj-ecoff.h $(srcdir)/config/tc-alpha.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(BFDDIR)/libecoff.h
OBJDEP_alpha_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-alpha.h subsegs.h \
  $(INCDIR)/obstack.h ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h \
  $(INCDIR)/elf/alpha.h $(INCDIR)/aout/aout64.h
OBJDEP_alpha_evax = $(srcdir)/config/obj-evax.h $(srcdir)/config/tc-alpha.h
OBJDEP_arc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-arc.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_arc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arc.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_arm_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-arm.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_arm_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-arm.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/arm.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_arm_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-arm.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_d10v_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-d10v.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_d10v_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d10v.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_h8300_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-h8300.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/h8300.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_h8300_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8300.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_h8500_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-h8500.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/h8500.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_h8500_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-h8500.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_hppa_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-hppa.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_hppa_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-hppa.h $(BFDDIR)/elf32-hppa.h \
  $(BFDDIR)/libhppa.h $(INCDIR)/elf/hppa.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_i386_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-i386.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_i386_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i386.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/i386.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_i386_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i386.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_i860_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-i860.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_i860_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i860.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_i860_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i860.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_i960_bout = $(srcdir)/config/obj-bout.h $(srcdir)/config/tc-i960.h \
  $(INCDIR)/obstack.h
OBJDEP_i960_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-i960.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/i960.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_i960_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-i960.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_m32r_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m32r.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_m32r_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m32r.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_m68k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-m68k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_m68k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m68k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/m68k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_m68k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m68k.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_m68k_hp300 = $(srcdir)/config/obj-aout.c $(srcdir)/config/obj-hp300.h \
  $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-m68k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_m88k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-m88k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/m88k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_m88k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-m88k.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_mips_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-mips.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_mips_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mips.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_mips_ecoff = $(srcdir)/config/obj-ecoff.h $(srcdir)/config/tc-mips.h \
  ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(BFDDIR)/libecoff.h
OBJDEP_mips_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mips.h subsegs.h \
  $(INCDIR)/obstack.h ecoff.h $(INCDIR)/coff/sym.h $(INCDIR)/coff/ecoff.h \
  $(INCDIR)/elf/mips.h $(INCDIR)/aout/aout64.h
OBJDEP_mn10200_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mn10200.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_mn10200_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10200.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_mn10300_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mn10300.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_mn10300_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-mn10300.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_ns32k_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-ns32k.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_ns32k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-ns32k.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_ns32k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ns32k.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_ppc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-ppc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/rs6000.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_ppc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-ppc.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/elf/ppc.h $(INCDIR)/aout/aout64.h
OBJDEP_sh_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-sh.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sh.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_sh_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sh.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_sparc_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-sparc.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_sparc_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-sparc.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/sparc.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_sparc_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-sparc.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_tahoe_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-tahoe.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_tahoe_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-tahoe.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_tahoe_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tahoe.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_tic30_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-tic30.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_tic30_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-tic30.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/tic30.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_tic30_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-tic30.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_vax_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-vax.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h $(INCDIR)/aout/aout64.h \
  $(INCDIR)/obstack.h
OBJDEP_vax_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-vax.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_vax_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-vax.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_vax_vms = $(srcdir)/config/obj-vms.h $(srcdir)/config/tc-vax.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def subsegs.h \
  $(INCDIR)/obstack.h
OBJDEP_w65_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-w65.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/w65.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_w65_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-w65.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_v850_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-v850.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h \
  $(INCDIR)/obstack.h subsegs.h
OBJDEP_v850_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-v850.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_z8k_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-z8k.h \
  $(INCDIR)/coff/internal.h $(INCDIR)/coff/z8k.h $(BFDDIR)/libcoff.h \
  $(INCDIR)/bfdlink.h $(INCDIR)/obstack.h subsegs.h
OBJDEP_z8k_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-z8k.h subsegs.h \
  $(INCDIR)/obstack.h $(INCDIR)/aout/aout64.h
OBJDEP_hppa_som = $(srcdir)/config/obj-som.h subsegs.h \
  $(INCDIR)/obstack.h $(BFDDIR)/libhppa.h $(BFDDIR)/som.h \
  $(INCDIR)/aout/stab_gnu.h $(INCDIR)/aout/stab.def
OBJDEP_i386_multi = $(OBJDEP_i386_coff) $(OBJDEP_i386_elf)
OBJDEP_mips_multi = $(OBJDEP_mips_coff) $(OBJDEP_mips_ecoff) \
  $(OBJDEP_mips_elf)
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
DEP_d10v_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-d10v.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
DEP_d10v_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-d10v.h
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
DEP_hppa_elf = $(srcdir)/config/obj-elf.h $(BFDDIR)/elf-bfd.h \
  $(INCDIR)/elf/common.h $(INCDIR)/elf/internal.h $(INCDIR)/elf/external.h \
  $(INCDIR)/bfdlink.h $(srcdir)/config/tc-hppa.h $(BFDDIR)/elf32-hppa.h \
  $(BFDDIR)/libhppa.h $(INCDIR)/elf/hppa.h
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
DEP_mips_aout = $(srcdir)/config/obj-aout.h $(srcdir)/config/tc-mips.h \
  $(BFDDIR)/libaout.h $(INCDIR)/bfdlink.h
DEP_mips_coff = $(srcdir)/config/obj-coff.h $(srcdir)/config/tc-mips.h \
  $(INCDIR)/coff/internal.h $(BFDDIR)/libcoff.h $(INCDIR)/bfdlink.h
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
DEP_i386_multi = $(DEP_i386_coff) $(DEP_i386_elf)
DEP_mips_multi = $(DEP_mips_coff) $(DEP_mips_ecoff) \
  $(DEP_mips_elf)
$(OBJS): $(DEP_@target_cpu_type@_@obj_format@)
$(TARG_CPU_O): $(TCDEP_@target_cpu_type@_@obj_format@)
$(OBJ_FORMAT_O): $(OBJDEP_@target_cpu_type@_@obj_format@)
# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
