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

CFLAGS+=	-DGENERATOR_FILE -I.

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/cp

#-----------------------------------------------------------------------
# insn-* gunk

CLEANFILES+=	gensupport+%DIKED.c
gensupport+%DIKED.c: gensupport.c
	sed \
	    -e 's/^xmalloc/DONT_xmalloc/g'	\
	    -e 's/^xrealloc/DONT_xrealloc/g'	\
	    -e 's/^xstrdup/DONT_xstrdup/g'	\
	${.ALLSRC} > ${.TARGET}

.for F in attr codes config flags
insn-$F.h: gen$F ${MD_FILE}
	./gen$F ${MD_FILE} > insn-$F.h
GENSRCS+=	insn-$F.h
.endfor

GENSRCS+=	gen-time-stamp
gen-time-stamp: genattrtab genemit genextract genopinit genoutput genpeep genrecog
	touch ${.TARGET}

.for F in attr codes config emit extract flags opinit output peep recog
build-tools: gen$F

gen$F: gen$F.o rtl.o obstack.o print-rtl.o bitmap.o errors.o gensupport+%DIKED.o ggc-none.o hashtab.o #safe-ctype.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

.for F in attrtab
build-tools: gen$F

gen$F: gen$F.o rtl.o print-rtl.o obstack.o bitmap.o errors.o gensupport+%DIKED.o ggc-none.o hashtab.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

SRCS+=		bitmap.c errors.c gensupport+%DIKED.c ggc-none.c hashtab.c obstack.c print-rtl.c regclass.c rtl.c rtlanal.c

.for F in check genrtl
build-tools: gen$F

gen$F: gen$F.o
	${CC} -static -DGENERATOR_FILE ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F
.endfor

.ORDER: genrtl.c genrtl.h
genrtl.c genrtl.h: gengenrtl
	./gengenrtl > genrtl.c
	./gengenrtl -h > genrtl.h

GENSRCS+=	genrtl.c genrtl.h

#-----------------------------------------------------------------------
# Common parser stuff.

tree-check.h: gencheck
	./gencheck > ${.TARGET}
GENSRCS+=	tree-check.h

#-----------------------------------------------------------------------
# the host/target compiler config.

COMMONHDRS=	config.h hconfig.h multilib.h options.h specs.h tconfig.h tm.h \
		tm_p.h configargs.h safe-ctype.h
GENSRCS+=	${COMMONHDRS} gencheck.h

${COMMONHDRS}: ${.CURDIR}/Makefile

configargs.h:
	echo 'static const char configuration_arguments[] ='	> ${.TARGET}
	echo '	"FreeBSD/${GCC_ARCH} system compiler";'		>> ${.TARGET}
	echo 'static const char thread_model[] = "posix";'	>> ${.TARGET}

config.h hconfig.h:
	echo '#include "auto-host.h"'			> ${.TARGET}
	echo '#include "gansidecl.h"'			>> ${.TARGET}
	echo '#include "${GCC_ARCH}/xm-${GCC_ARCH}.h"'	>> ${.TARGET}
	echo '#include "defaults.h"'			>> ${.TARGET}
	echo '#include "hwint.h"'			>> ${.TARGET}
	echo '#ifndef POSIX'				>> ${.TARGET}
	echo '# define POSIX'				>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
	echo '#ifndef GENERATOR_FILE'			>> ${.TARGET}
	echo '#include "insn-codes.h"'			>> ${.TARGET}
	echo '#include "insn-flags.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}

gencheck.h:
	echo '#include "cp/cp-tree.def"'		> ${.TARGET}
	echo '#include "objc/objc-tree.def"'		>> ${.TARGET}

multilib.h:
	echo 'static const char *const multilib_raw[] = { \
	    "aout maout;", "elf !maout;", NULL };'	> ${.TARGET}
	echo 'static const char *const multilib_matches_raw[] = { \
	    "maout maout;", "melf melf;", NULL };'	>> ${.TARGET}
	echo 'static const char *multilib_extra = "";'	>> ${.TARGET}
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

tconfig.h:
	echo '#include "gansidecl.h"'			> ${.TARGET}
	echo '#include "${GCC_ARCH}/xm-${GCC_ARCH}.h"'	>> ${.TARGET}
	echo '#include "defaults.h"'			>> ${.TARGET}
	echo '#ifndef GENERATOR_FILE'			>> ${.TARGET}
	echo '#include "insn-codes.h"'			>> ${.TARGET}
	echo '#include "insn-flags.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}
	echo '#define USING_SJLJ_EXCEPTIONS 0'		>> ${.TARGET}

#	KEEP THIS IN SYNC with src/gcc/lib/libgcc/Makefile !!
tm.h:
	echo '#include "gansidecl.h"'			> ${.TARGET}
	echo '#include "${GCC_ARCH}/${GCC_ARCH}.h"'	>> ${.TARGET}
.if ${GCC_ARCH} == "i386"
	echo '#include "${GCC_ARCH}/att.h"'		>> ${.TARGET}
.endif
	echo '#include <freebsd-native.h>'		>> ${.TARGET}
	echo '#include <freebsd.h>'			>> ${.TARGET}
.if exists(${GCCDIR}/config/${GCC_ARCH}/elf.h)
	echo '#include "${GCC_ARCH}/elf.h"'		>> ${.TARGET}
.endif
	echo '#include "${GCC_ARCH}/freebsd.h"'		>> ${.TARGET}
.if ${GCC_ARCH} == "i386"
	echo '#include "${GCC_ARCH}/perform.h"'		>> ${.TARGET}
.endif
	echo '#include "defaults.h"'			>> ${.TARGET}
	echo '#ifndef GENERATOR_FILE'			>> ${.TARGET}
	echo '#include "insn-codes.h"'			>> ${.TARGET}
	echo '#include "insn-flags.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}

tm_p.h:
	echo '#include "${GCC_ARCH}/${GCC_ARCH}-protos.h"' > ${.TARGET}
	echo '#include <freebsd-native.h>'		>> ${.TARGET}
	echo '#ifndef GENERATOR_FILE'			>> ${.TARGET}
	echo '#include "insn-codes.h"'			>> ${.TARGET}
	echo '#include "insn-flags.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}

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
${OBJS:Nbitmap.o:Nerrors.o:Ngenattr.o:Ngencheck.o:Ngencodes.o:Ngenconfig.o:Ngenflags.o:Ngengenrtl.o:Ngensupport+%DIKED.o:Nggc-none.o:Nhashtab.o:Nobstack.o:Nprint-rtl.o:Nrtl.o}: ${SRCS:M*.h}

# Next, make each of the problematic objects depend on only most headers.
# Filter out the headers that would cause problems (and a few more when it
# is inconvenient to filter precisely).
#
bitmap.o genattr.o gencodes.o genconfig.o genflags.o obstack.o print-rtl.o \
    rtl.o: ${SRCS:M*.h:Ninsn-*.h}
gencheck.o: gencheck.h ${SRCS:M*.h:Ngenrtl.h:Ntree-check.h:Ninsn-*.h}
gengenrtl.o: ${SRCS:M*.h:Ngenrtl.h:Ninsn-*.h}
.endif

#GENSRCS+=	safe-ctype.h
${OBJS} gensupport+%DIKED.o: safe-ctype.h
safe-ctype.h: Makefile
	echo '#include <ctype.h>'	> ${.TARGET}
.for Z in TOUPPER TOLOWER ISDIGIT ISXDIGIT ISUPPER ISLOWER ISALPHA ISALNUM ISSPACE ISPUNCT ISGRAPH ISBLANK ISPRINT ISCNTRL
	echo '#define	${Z}	${Z:L}'	>> ${.TARGET}
.endfor
	echo "#define	ISIDST(x)	((x) == '_' || isalpha(x))"	>> ${.TARGET}
	echo "#define	ISIDNUM(x)	(isdigit(x) || ISIDST(x))"	>> ${.TARGET}
	echo "#define	IS_VSPACE(x)	((x) == '\n' || (x) == '\r')"	>> ${.TARGET}
	echo "#define	IS_NVSPACE(x)	(!IS_VSPACE(x) && (isspace(x) || (x) == '\0'))"	>> ${.TARGET}
	echo "#define	IS_SPACE_OR_NUL(x)	(isspace(x) || (x) == '\0')"	>> ${.TARGET}
