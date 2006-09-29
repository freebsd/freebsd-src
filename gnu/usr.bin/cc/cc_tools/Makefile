# $FreeBSD$

#
# This could probably be merged with ../cc_int/Makefile, but bsd.lib.mk
# is such a !@#!*#% nightmare because of how it reprograms the dependencies,
# suffix rules, SRCS, etc.  It's easiest to cheat by using bsd.prog.mk and
# SRCS to get dependencies.
#

#
# ../Makefile.inc will put an absolute path to our objdir in CFLAGS.
# Prevent mkdep from using it, so that we don't have to give rules for
# aliases of generated headers.
#
CFLAGS+=	-I.

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/f

CFLAGS+=	-DGENERATOR_FILE

#
#-----------------------------------------------------------------------
# Build 'pocket' libiberty exclusively for build tools use.

LIBIBERTY_SRCS=	choose-temp.c concat.c cp-demangle.c cp-demint.c cplus-dem.c \
	dyn-string.c fibheap.c getpwd.c getruntime.c hashtab.c hex.c \
	lbasename.c make-temp-file.c md5.c obstack.c partition.c pex-unix.c \
	physmem.c splay-tree.c xexit.c xmalloc.c xmemdup.c xstrdup.c xstrerror.c
LIBIBERTY_OBJS=	${LIBIBERTY_SRCS:R:S/$/.o/g}

SRCS+=	${LIBIBERTY_SRCS}

LIBIBERTY=libiberty.a
${LIBIBERTY}: ${LIBIBERTY_OBJS}
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} `lorder ${LIBIBERTY_OBJS} | tsort -q`
	${RANLIB} ${.TARGET}
CLEANFILES+=	${LIBIBERTY}

#
#-----------------------------------------------------------------------
# options
OPTION_FILES=${GCCDIR}/f/lang.opt ${GCCDIR}/c.opt ${GCCDIR}/common.opt

.ORDER: options.h options.c
options.h options.c: opts.sh ${OPTION_FILES}
	/bin/sh ${GCCDIR}/opts.sh mv options.c options.h ${OPTION_FILES}

GENSRCS+=	options.c options.h
CLEANFILES+=	options.c options.h

#-----------------------------------------------------------------------
# insn-* gunk

.for F in attr codes config flags constants
insn-$F.h: gen$F ${MD_FILE}
	./gen$F ${MD_FILE} > insn-$F.h
GENSRCS+=	insn-$F.h
.endfor

.for F in conditions
insn-$F.c: gen$F ${MD_FILE}
	./gen$F ${MD_FILE} > insn-$F.c
GENSRCS+=	insn-$F.c
.endfor

GENSRCS+=	gen-time-stamp
gen-time-stamp: genattr genattrtab genconditions genconstants genemit \
		genextract gengtype genopinit genoutput genpeep genrecog
	touch ${.TARGET}

.for F in attr codes config emit extract flags opinit output peep recog
gen$F: gen$F.o rtl.o print-rtl.o bitmap.o gensupport.o  ggc-none.o \
	read-rtl.o insn-conditions.o min-insn-modes.o errors.o ${LIBIBERTY}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

#
# genattrtab needs more complex build rule
#
genattrtab : genattrtab.o rtl.o print-rtl.o bitmap.o gensupport.o ggc-none.o \
	read-rtl.o insn-conditions.o genautomata.o varray.o min-insn-modes.o \
	errors.o ${LIBIBERTY}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC} -lm

GENSRCS+=	genattrtab.c
CLEANFILES+=	genattrtab

#
# genconstants and genconditions cannot depend on insn-conditions.o
# they should be liked with dummy-conditions.o stubs instead
#
.for F in constants conditions
gen$F: gen$F.o rtl.o bitmap.o gensupport.o ggc-none.o read-rtl.o dummy-conditions.o min-insn-modes.o errors.o ${LIBIBERTY}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

.for F in modes check genrtl preds
gen$F: gen$F.o errors.o ${LIBIBERTY}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

.ORDER: genrtl.c genrtl.h
genrtl.c genrtl.h: gengenrtl
	./gengenrtl > genrtl.c
	./gengenrtl -h > genrtl.h

GENSRCS+=	genrtl.c genrtl.h

SRCS+=	bitmap.c dummy-conditions.c errors.c genautomata.c gensupport.c \
	ggc-none.c print-rtl.c read-rtl.c rtl.c varray.c

#-----------------------------------------------------------------------
# insn modes stuff.

.ORDER: insn-modes.c insn-modes.h
insn-modes.h: genmodes
	./genmodes -h > insn-modes.h

insn-modes.c: genmodes
	./genmodes > insn-modes.c

min-insn-modes.c: genmodes
	./genmodes -m > min-insn-modes.c

GENSRCS+= insn-modes.c min-insn-modes.c insn-modes.h

#-----------------------------------------------------------------------
# Common parser stuff.

tree-check.h: gencheck
	./gencheck > ${.TARGET}
GENSRCS+=	tree-check.h

#-----------------------------------------------------------------------
# Predicates stuff.

tm-preds.h: genpreds
	./genpreds > ${.TARGET}
GENSRCS+=	tm-preds.h

#-----------------------------------------------------------------------
# Gengtype

gengtype-lex.c : gengtype-lex.l
	${LEX} -t  ${.ALLSRC} | \
	sed 's/^\(char msg\[\];\)/yyconst \1/' > ${.TARGET}

.ORDER: gengtype-yacc.c gengtype-yacc.h
gengtype-yacc.c gengtype-yacc.h: gengtype-yacc.y
	${YACC} -d -o gengtype-yacc.c ${.ALLSRC}

GENSRCS+=	gengtype-yacc+%DIKED.c gengtype-yacc.h gengtype-lex.c
CLEANFILES+=	gengtype-yacc.c

gengtype-yacc+%DIKED.c: gengtype-yacc.c
	cat    ${.ALLSRC} > ${.TARGET}
	sed -e "s/xmalloc/malloc/g" \
	    -e "s/xrealloc/realloc/g" \
	    -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    ${.ALLSRC} > ${.TARGET}

gengtype: gengtype.o gengtype-yacc+%DIKED.o gengtype-lex.o ${LIBIBERTY}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gengtype.c
CLEANFILES+=	gengtype

gengtype-lex.o: gengtype-yacc.h

.ORDER: gtype-desc.c gtype-desc.h
gtype-desc.c gtype-desc.h: gtype-time-stamp
	@true

GENSRCS+=	gtype-time-stamp
gtype-time-stamp: gengtype ${GTFILES}
	./gengtype
	touch ${.TARGET}

GENSRCS+=	gtype-desc.c gtype-desc.h
CLEANFILES+=	gt-*.h gtype-*.h

#
#-----------------------------------------------------------------------
# Fortran build tools

.if !defined(NO_FORTRAN)
gen-time-stamp: fini
fini: fini.o ${LIBIBERTY}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}
SRCS+=		fini.c
CLEANFILES+=	fini
.endif

#-----------------------------------------------------------------------
# Determine content of variables used by the target/host config files

#
# The list of headers to go into tm.h
#
.if ${TARGET_ARCH} == "amd64"
TARGET_INC=	i386/biarch64.h
.endif
.if ${TARGET_ARCH} != "arm"
TARGET_INC+=	${GCC_CPU}/${GCC_CPU}.h
.endif
.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
TARGET_INC+=	${GCC_CPU}/unix.h
TARGET_INC+=	${GCC_CPU}/att.h
.endif
.if ${TARGET_ARCH} != "alpha"
TARGET_INC+=	dbxelf.h
TARGET_INC+=	elfos.h
.endif
TARGET_INC+=	freebsd-native.h
TARGET_INC+=	freebsd-spec.h
TARGET_INC+=	freebsd.h
.if ${TARGET_ARCH} == "alpha"
TARGET_INC+=	${GCC_CPU}/elf.h
.endif
.if ${TARGET_ARCH} != "i386" && ${TARGET_ARCH} != "amd64"
.if exists(${GCCDIR}/config/${GCC_CPU}/sysv4.h)
TARGET_INC+=	${GCC_CPU}/sysv4.h
.endif
.endif
.if ${TARGET_ARCH} == "arm"
TARGET_INC+=	${GCC_CPU}/elf.h
TARGET_INC+=	${GCC_CPU}/aout.h
TARGET_INC+=	${GCC_CPU}/${GCC_CPU}.h
. if defined(TARGET_BIG_ENDIAN)
CFLAGS+= -DTARGET_ENDIAN_DEFAULT=ARM_FLAG_BIG_END
. endif
.endif
TARGET_INC+=	${GCC_CPU}/freebsd.h
.if ${TARGET_ARCH} == "amd64"
TARGET_INC+=	${GCC_CPU}/x86-64.h
TARGET_INC+=	${GCC_CPU}/freebsd64.h
TARGET_INC+=	freebsd64-fix.h
.endif
.if ${TARGET_ARCH} == "powepc"
TARGET_INC+=	altivec-defs.h
.endif
TARGET_INC+=	defaults.h

#
# Use TARGET_INC as a template and build a list of target specific
# include files for gengtype to scan
#
GCONFIG_H=	${.CURDIR}/auto-host.h

.for H in ${TARGET_INC}
.for D in ${GCCDIR}/config ${GCCDIR} ${.CURDIR}
.if exists($D/$H)
GCONFIG_H+=	$D/$H
.endif
.endfor
.endfor

#
# Define some variables to make blocks copied from Makefile.in happy
#
srcdir=		${GCCDIR}
HASHTAB_H=	${GCCDIR}/hashtab.h
SPLAY_TREE_H=	${GCCDIR}/splay-tree.h
out_file=	${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}.c
GTFILES_SRCDIR=	${GCCDIR}

#
# Copied unchanged from gcc/Makefile.in
#
GTFILES = $(srcdir)/input.h $(srcdir)/coretypes.h $(srcdir)/cpplib.h \
  $(GCONFIG_H) $(HASHTAB_H) $(SPLAY_TREE_H) \
  $(srcdir)/bitmap.h $(srcdir)/coverage.c $(srcdir)/function.h  $(srcdir)/rtl.h \
  $(srcdir)/optabs.h $(srcdir)/tree.h $(srcdir)/libfuncs.h $(srcdir)/hashtable.h \
  $(srcdir)/real.h $(srcdir)/varray.h $(srcdir)/insn-addr.h \
  $(srcdir)/cselib.h $(srcdir)/basic-block.h  $(srcdir)/cgraph.h \
  $(srcdir)/c-common.h $(srcdir)/c-tree.h \
  $(srcdir)/alias.c $(srcdir)/bitmap.c $(srcdir)/cselib.c $(srcdir)/cgraph.c \
  $(srcdir)/dbxout.c $(srcdir)/dwarf2out.c $(srcdir)/dwarf2asm.c \
  $(srcdir)/dojump.c \
  $(srcdir)/emit-rtl.c $(srcdir)/except.c $(srcdir)/explow.c $(srcdir)/expr.c \
  $(srcdir)/fold-const.c $(srcdir)/function.c \
  $(srcdir)/gcse.c $(srcdir)/integrate.c $(srcdir)/lists.c $(srcdir)/optabs.c \
  $(srcdir)/profile.c $(srcdir)/ra-build.c $(srcdir)/regclass.c \
  $(srcdir)/reg-stack.c $(srcdir)/cfglayout.c $(srcdir)/langhooks.c \
  $(srcdir)/sdbout.c $(srcdir)/stmt.c $(srcdir)/stor-layout.c \
  $(srcdir)/stringpool.c $(srcdir)/tree.c $(srcdir)/varasm.c \
  $(out_file)

#
# Build a list of frontend directories to look into
#
GTFILES_LANG_DIR_NAMES=

.if !defined(NO_CXX)
GTFILES_LANG_DIR_NAMES+=	cp
.endif

.if !defined(NO_OBJC)
GTFILES_LANG_DIR_NAMES+=	objc
.endif

.if !defined(NO_FORTRAN)
GTFILES_LANG_DIR_NAMES+=	f
.endif

#
# Build a list of language specific files for gengtype
#
.for L in ${GTFILES_LANG_DIR_NAMES} c
.if exists(${GCCDIR}/$L-config-lang.in)
# Source the language config file
L_GTFILES!=	sh -c '. ${GCCDIR}/$L-config-lang.in; echo $$gtfiles'
.else
L_GTFILES!=	sh -c '. ${GCCDIR}/$L/config-lang.in; echo $$gtfiles'
.endif
.for F in ${L_GTFILES}
GTFILES_FILES+=	$F
GTFILES_LANGS+= $L
.endfor
.endfor
GTFILES+=	${GTFILES_FILES}

#-----------------------------------------------------------------------
# the host/target compiler config.

COMMONHDRS=	bconfig.h config.h configargs.h gencheck.h multilib.h \
		specs.h safe-ctype.h tconfig.h tm.h tm_p.h gcov-iov.h \
		gtyp-gen.h
GENSRCS+=	${COMMONHDRS}

MFILE?=	${.CURDIR}/Makefile
${COMMONHDRS}: ${MFILE}

configargs.h:
	echo 'static const char configuration_arguments[] ='	> ${.TARGET}
	echo '	"FreeBSD/${TARGET_ARCH} system compiler";'	>> ${.TARGET}
	echo 'static const char thread_model[] = "posix";'	>> ${.TARGET}
	echo 'static const struct {'				>> ${.TARGET}
	echo '	const char *name, *value;'			>> ${.TARGET}
	echo '} configure_default_options[] = {'		>> ${.TARGET}
	echo '	{ "NULL", "NULL" } };'				>> ${.TARGET}

tconfig.h:
	echo '#ifndef GCC_TCONFIG_H'			> ${.TARGET}
	echo '#define GCC_TCONFIG_H'			>> ${.TARGET}
	echo '#ifdef IN_GCC'				>> ${.TARGET}
	echo '# include "ansidecl.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
	echo '#define USED_FOR_TARGET'			>> ${.TARGET}
	echo '#endif /* GCC_TCONFIG_H */'		>> ${.TARGET}

bconfig.h:
	echo '#ifndef GCC_BCONFIG_H'			> ${.TARGET}
	echo '#define GCC_BCONFIG_H'			>> ${.TARGET}
	echo '#include "auto-host.h"'			>> ${.TARGET}
.if exists(${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}-modes.def)
	echo '#define EXTRA_MODES_FILE "${GCC_CPU}/${GCC_CPU}-modes.def"' >> ${.TARGET}
.endif
	echo '#ifdef IN_GCC'				>> ${.TARGET}
	echo '# include "ansidecl.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
	echo '#endif /* GCC_BCONFIG_H */'		>> ${.TARGET}

gencheck.h:
	echo '#include "cp/cp-tree.def"'		> ${.TARGET}
	echo '#include "objc/objc-tree.def"'		>> ${.TARGET}

multilib.h:
	echo 'static const char *const multilib_raw[] = { \
	    "aout maout;", "elf !maout;", NULL };'	> ${.TARGET}
	echo 'static const char *const multilib_matches_raw[] = { \
	    "maout maout;", "melf melf;", NULL };'	>> ${.TARGET}
	echo 'static const char *multilib_extra = "";'	>> ${.TARGET}
	echo 'static const char *multilib_options = "";'>> ${.TARGET}
	echo 'static const char *const multilib_exclusions_raw[] = { \
	    NULL };'					>> ${.TARGET}

specs.h:
	echo '#include "cp/lang-specs.h"'		> ${.TARGET}
	echo '#include "f/lang-specs.h"'		>> ${.TARGET}
	echo '#include "objc/lang-specs.h"'		>> ${.TARGET}

config.h: bconfig.h
	echo '#include <bconfig.h>'			> ${.TARGET}

tm.h:
	echo '#ifndef GCC_TM_H'				> ${.TARGET}
	echo '#define GCC_TM_H'				>> ${.TARGET}
.if defined(TARGET_CPU_DEFAULT)
	echo "#define TARGET_CPU_DEFAULT (${TARGET_CPU_DEFAULT})" >> ${.TARGET}
.endif
	echo '#ifdef IN_GCC'				>> ${.TARGET}
.for H in ${TARGET_INC}
	echo '#include "$H"'				>> ${.TARGET}
.endfor
	echo '#if !defined GENERATOR_FILE && !defined USED_FOR_TARGET' >> ${.TARGET}
	echo '# include "insn-constants.h"'		>> ${.TARGET}
	echo '# include "insn-flags.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
.if exists(${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}-modes.def)
	echo '#define EXTRA_MODES_FILE "${GCC_CPU}/${GCC_CPU}-modes.def"' >> ${.TARGET}
.endif
	echo '#endif /* GCC_TM_H */'			>> ${.TARGET}

tm_p.h:
	echo '#include "${GCC_CPU}/${GCC_CPU}-protos.h"'	>> ${.TARGET}
	echo '#include "tm-preds.h"'				>> ${.TARGET}

safe-ctype.h:
	echo '#include <ctype.h>'				> ${.TARGET}
.for Z in TOUPPER TOLOWER ISDIGIT ISXDIGIT ISUPPER ISLOWER ISALPHA ISALNUM \
    ISSPACE ISPUNCT ISGRAPH ISBLANK ISPRINT ISCNTRL
	echo '#define ${Z}	${Z:L}'				>> ${.TARGET}
.endfor
	echo "#define ISIDST(x)		\
		((x) == '_' || isalpha(x))"			>> ${.TARGET}
	echo "#define ISIDNUM(x)	\
		(isdigit(x) || ISIDST(x))"			>> ${.TARGET}
	echo "#define IS_VSPACE(x)	\
		((x) == '\n' || (x) == '\r')"			>> ${.TARGET}
	echo "#define IS_NVSPACE(x)	\
		(!IS_VSPACE(x) && (isspace(x) || (x) == '\0'))"	>> ${.TARGET}
	echo "#define IS_SPACE_OR_NUL(x)	\
		(isspace(x) || (x) == '\0')"			>> ${.TARGET}

gtyp-gen.h:
	echo "/* This file is machine generated.  Do not edit.  */" > ${.TARGET}
	echo "static const char *srcdir = "			>> ${.TARGET}
	echo "\"$(GTFILES_SRCDIR)\";"				>> ${.TARGET}
	echo "static const char *lang_files[] = {"		>> ${.TARGET}
.for F in ${GTFILES_FILES}
	echo "\"$F\", "						>> ${.TARGET}
.endfor
	echo "NULL};"						>> ${.TARGET}
	echo "static const char *langs_for_lang_files[] = {"	>> ${.TARGET}
.for F in ${GTFILES_LANGS}
	echo "\"$F\", "						>> ${.TARGET}
.endfor
	echo "NULL};"						>> ${.TARGET}
	echo "static const char *all_files[] = {"		>> ${.TARGET}
.for F in ${GTFILES}
	echo "\"$F\", "						>> ${.TARGET}
.endfor
	echo "NULL};"						>> ${.TARGET}
	echo "static const char *lang_dir_names[] = { \"c\", "	>> ${.TARGET}
.for F in ${GTFILES_LANG_DIR_NAMES}
	echo "\"$F\", "						>> ${.TARGET}
.endfor
	echo "NULL};"						>> ${.TARGET}

gcov-iov.h:
	echo "#define GCOV_VERSION ((gcov_unsigned_t)0x33303470)" >> ${.TARGET}

#-----------------------------------------------------------------------
# General things.

SRCS+=		${GENSRCS}
CLEANFILES+=	${GENSRCS}

all:		${SRCS}

.include <bsd.prog.mk>

#-----------------------------------------------------------------------
# Fixups.

# Set OBJS the same as bsd.prog.mk would do if we defined PROG.  We can't
# define PROG because we have multiple programs.
#
OBJS+=		${SRCS:N*.h:R:S/$/.o/g}
CLEANFILES+=	${OBJS}

.if !exists(${DEPENDFILE})
# Fudge pre-dependfile dependencies of objects in much the same way as
# bsd.prog.mk would do if we defined PROG.

${OBJS}: ${COMMONHDRS}

dummy-conditions.o:
gencheck.o:
genmodes.o:

genpreds.o: insn-modes.h
genconstants.o: insn-modes.h genrtl.h
gengtype.o: insn-modes.h genrtl.h gtyp-gen.h
rtl.o: insn-modes.h gtype-desc.h genrtl.h
bitmap.o: insn-modes.h gtype-desc.h genrtl.h
ggc-none.o: gtype-desc.h
gensupport.o: insn-modes.h genrtl.h
varray.o: gtype-desc.h
genautomata.o: insn-modes.h genrtl.h
genconditions.o: insn-modes.h genrtl.h
gencodes.o: insn-modes.h genrtl.h
genconfig.o: insn-modes.h genrtl.h
print-rtl.o: insn-modes.h genrtl.h tm-preds.h tree-check.h
read-rtl.o: insn-modes.h genrtl.h
genattr.o: insn-modes.h genrtl.h
genemit.o: insn-modes.h genrtl.h
genflags.o: insn-modes.h genrtl.h
genopinit.o: insn-modes.h genrtl.h
genoutput.o: insn-modes.h genrtl.h
genpeep.o: insn-modes.h genrtl.h
genrecog.o: insn-modes.h genrtl.h
genextract.o: genrtl.h insn-config.h
genattrtab.o: insn-modes.h gtype-desc.h genrtl.h
genrtl.o: insn-modes.h genrtl.h gtype-desc.h

insn-conditions.o: insn-constants.h tm-preds.h
insn-modes.o: insn-modes.h
min-insn-modes.o: insn-modes.h
gtype-desc.o: insn-modes.h insn-config.h insn-codes.h tree-check.h

.endif
