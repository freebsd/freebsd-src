# $FreeBSD$

#
# This could probably be merged with ../cc_int/Makefile, but bsd.lib.mk
# is such a !@#!*#% nightmare because of how it reprograms the dependencies,
# suffix rules, SRCS, etc.  It's easiest to cheat by using bsd.prog.mk and
# SRCS to get dependencies.
#

# ../Makefile.inc will put an absolute path to our objdir in CFLAGS.
# Prevent mkdep from using it, so that we don't have to give rules for
# aliases of generated headers.

CFLAGS+=	-I. -static -DGENERATOR_FILE

.include "../Makefile.inc"

.PATH: ${GCCDIR}

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
build-tools: gen$F

gen$F: gen$F.o rtl.o obstack.o print-rtl.o bitmap.o errors.o gensupport.o \
	ggc-none.o hashtab.o read-rtl.o concat.o insn-conditions.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

#
# genattrtab needs more complex build rule
#
build-tools: genattrtab

genattrtab : genattrtab.o rtl.o obstack.o print-rtl.o bitmap.o errors.o \
	gensupport.o ggc-none.o hashtab.o read-rtl.o concat.o \
	insn-conditions.o genautomata.o varray.o getruntime.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC} -lm

GENSRCS+=	genattrtab.c
CLEANFILES+=	genattrtab

#
# genconstants and genconditions cannot depend on insn-conditions.o
# they should be liked with dummy-conditions.o stubs instead
#
.for F in constants conditions
build-tools: gen$F

gen$F: gen$F.o rtl.o obstack.o bitmap.o errors.o gensupport.o \
       ggc-none.o hashtab.o read-rtl.o concat.o dummy-conditions.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

.for F in check genrtl preds
build-tools: gen$F

gen$F: gen$F.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

.ORDER: genrtl.c genrtl.h
genrtl.c genrtl.h: gengenrtl
	./gengenrtl > genrtl.c
	./gengenrtl -h > genrtl.h

GENSRCS+=	genrtl.c genrtl.h

SRCS+=	bitmap.c concat.c dummy-conditions.c errors.c genautomata.c \
	gensupport.c getruntime.c ggc-none.c hashtab.c insn-conditions.c \
	obstack.c physmem.c print-rtl.c read-rtl.c rtl.c varray.c xmemdup.c

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

gengtype: gengtype.o gengtype-yacc+%DIKED.o gengtype-lex.o xmemdup.o
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

#-----------------------------------------------------------------------
# Determine content of variables used by the target/host config files

#
# The list of headers to go into tconfig.h
#
TARGET_INC=	ansidecl.h
TARGET_INC+=	${GCC_CPU}/${GCC_CPU}.h
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
TARGET_INC+=	${GCC_CPU}/freebsd.h
.if ${TARGET_ARCH} == "amd64"
TARGET_INC+=	${GCC_CPU}/x86-64.h
TARGET_INC+=	${GCC_CPU}/freebsd64.h
.endif
TARGET_INC+=	defaults.h

#
# Use TARGET_INC as a template and build a list of target specific
# include files for gengtype to scan
#
GCONFIG_H=	${.OBJDIR}/tconfig.h ${.CURDIR}/auto-host.h

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
out_file=	${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}.c
GTFILES_SRCDIR=	${GCCDIR}

#
# Copied unchanged from gcc/Makefile.in
#
GTFILES = $(GCONFIG_H) $(srcdir)/location.h \
  $(HASHTAB_H) \
  $(srcdir)/bitmap.h $(srcdir)/function.h  $(srcdir)/rtl.h $(srcdir)/optabs.h \
  $(srcdir)/tree.h $(srcdir)/libfuncs.h $(srcdir)/hashtable.h $(srcdir)/real.h \
  $(srcdir)/varray.h $(srcdir)/ssa.h $(srcdir)/insn-addr.h $(srcdir)/cselib.h \
  $(srcdir)/c-common.h $(srcdir)/c-tree.h \
  $(srcdir)/basic-block.h \
  $(srcdir)/alias.c $(srcdir)/bitmap.c $(srcdir)/cselib.c \
  $(srcdir)/dwarf2out.c $(srcdir)/emit-rtl.c \
  $(srcdir)/except.c $(srcdir)/explow.c $(srcdir)/expr.c \
  $(srcdir)/fold-const.c $(srcdir)/function.c \
  $(srcdir)/gcse.c $(srcdir)/integrate.c $(srcdir)/lists.c $(srcdir)/optabs.c \
  $(srcdir)/profile.c $(srcdir)/ra-build.c $(srcdir)/regclass.c \
  $(srcdir)/reg-stack.c \
  $(srcdir)/sdbout.c $(srcdir)/stmt.c $(srcdir)/stor-layout.c \
  $(srcdir)/tree.c $(srcdir)/varasm.c \
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
$L_GTFILES!=	sh -c '. ${GCCDIR}/$L-config-lang.in; echo $$gtfiles'
.else
$L_GTFILES!=	sh -c '. ${GCCDIR}/$L/config-lang.in; echo $$gtfiles'
.endif
GTFILES+=	${$L_GTFILES}
.for F in ${$L_GTFILES}
GTFILES_FILES+=	$F
GTFILES_LANGS+= $L
.endfor
.endfor

#-----------------------------------------------------------------------
# the host/target compiler config.

COMMONHDRS=	config.h hconfig.h multilib.h options.h specs.h tconfig.h \
		tm_p.h configargs.h safe-ctype.h
GENSRCS+=	${COMMONHDRS} gencheck.h gtyp-gen.h

MFILE?=	${.CURDIR}/Makefile
${COMMONHDRS}: ${MFILE}

configargs.h:
	echo 'static const char configuration_arguments[] ='	> ${.TARGET}
	echo '	"FreeBSD/${TARGET_ARCH} system compiler";'	>> ${.TARGET}
	echo 'static const char thread_model[] = "posix";'	>> ${.TARGET}

hconfig.h:
	echo '#include "auto-host.h"'			> ${.TARGET}
	echo '#include <tconfig.h>'			>> ${.TARGET}

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

options.h:
	echo '#include "cp/lang-options.h"'		> ${.TARGET}
	echo '#include "f/lang-options.h"'		>> ${.TARGET}
	echo '#include "objc/lang-options.h"'		>> ${.TARGET}

specs.h:
	echo '#include "cp/lang-specs.h"'		> ${.TARGET}
	echo '#include "f/lang-specs.h"'		>> ${.TARGET}
	echo '#include "objc/lang-specs.h"'		>> ${.TARGET}

config.h:
	echo '#include <hconfig.h>'			> ${.TARGET}
	echo '#ifndef GENERATOR_FILE'			>> ${.TARGET}
	echo '#include "insn-constants.h"'		>> ${.TARGET}
	echo '#include "insn-flags.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}

tconfig.h:
	echo 'struct rtx_def;'				> ${.TARGET}
	echo 'typedef struct rtx_def *rtx;'		>> ${.TARGET}
	echo 'struct rtvec_def;'			>> ${.TARGET}
	echo 'typedef struct rtvec_def *rtvec;'		>> ${.TARGET}
	echo 'union tree_node;'				>> ${.TARGET}
	echo 'typedef union tree_node *tree;'		>> ${.TARGET}
	echo '#ifndef GTY'				>> ${.TARGET}
	echo '# define GTY(x)'				>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
	echo ''						>> ${.TARGET}
.if ${TARGET_ARCH} == "amd64"
	echo '#include "i386/biarch64.h"'		>> ${.TARGET}
.endif
.if ${TARGET_ARCH} == "ia64"
	echo '#define TARGET_CPU_DEFAULT (MASK_GNU_AS|MASK_GNU_LD)' >>${.TARGET}
.endif
.for H in ${TARGET_INC}
	echo '#include "$H"'				>> ${.TARGET}
.endfor
	echo '#ifndef POSIX'				>> ${.TARGET}
	echo '# define POSIX'				>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
.if exists(${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}-modes.def)
	echo '#define EXTRA_MODES_FILE "${GCC_CPU}/${GCC_CPU}-modes.def"' >> ${.TARGET}
	echo '#define EXTRA_CC_MODES 1'			>> ${.TARGET}
.endif

tm_p.h:
	echo '#include "${GCC_CPU}/${GCC_CPU}-protos.h"'	>> ${.TARGET}
	echo '#include "tm-preds.h"'				>> ${.TARGET}

safe-ctype.h: Makefile
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
	echo "#define	xexit exit"				>> ${.TARGET}


#-----------------------------------------------------------------------
# General things.

SRCS+=		${GENSRCS}
CLEANFILES+=	${GENSRCS}

all: ${SRCS}

.include <bsd.prog.mk>

#-----------------------------------------------------------------------
# Fixups.

# Set OBJS the same as bsd.prog.mk would do if we defined PROG.  We can't
# define PROG because we have multiple programs.
#
OBJS+=		${SRCS:N*.h:R:S/$/.o/g}

.if !exists(${DEPENDFILE})
# Fudge pre-dependfile dependencies of objects in much the same way as
# bsd.prog.mk would do if we defined PROG.  There are complications to
# avoid circular dependencies.  First, only make most objects depend on
# all headers.  Filter out the objects that would cause problems (i.e.,
# objects that will be used to create programs that will generate headers).
#
${OBJS}: ${SRCS:M*.h:Ngtype-desc.h:Ngenrtl.h:Ntree-check.h:Ntm-preds.h:Ninsn-*.h}

${OBJS:Ngencheck.o:Ngengenrtl.o:Ngenpreds.o}: tree-check.h tm-preds.h genrtl.h

${OBJS:Ngengtype*.o:Nxmemdup.o:Ngengenrtl.o:Ngencheck.o:Ngenpreds.o}: gtype-desc.h

genextract.o: insn-config.h

insn-conditions.o: insn-constants.h

.endif
