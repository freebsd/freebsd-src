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

CFLAGS+=	-I.

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/cp

CFLAGS+=	-I${GCCDIR}/objc

#-----------------------------------------------------------------------
# insn-* gunk

.for i in attr codes config flags
insn-$i.h: gen$i ${MD_FILE}
	./gen$i ${MD_FILE} > insn-$i.h
GENSRCS+=	insn-$i.h
.endfor

.for i in attrtab emit extract opinit output peep recog
insn-$i.c: gen$i ${MD_FILE}
	./gen$i ${MD_FILE} > insn-$i.c
GENSRCS+=	insn-$i.c
.endfor

.for i in attr codes config emit extract flags opinit output peep recog
build-tools: gen$i

gen$i: gen$i.o rtl.o obstack.o bitmap.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$i.c
CLEANFILES+=	gen$i
.endfor

.for i in attrtab
build-tools: gen$i

gen$i: gen$i.o rtl.o rtlanal.o print-rtl.o obstack.o bitmap.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$i.c
CLEANFILES+=	gen$i
.endfor

SRCS+=		bitmap.c obstack.c print-rtl.c rtl.c rtlanal.c

.for i in check genrtl
build-tools: gen$i

gen$i: gen$i.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$i.c
CLEANFILES+=	gen$i
.endfor

.ORDER: genrtl.c genrtl.h
genrtl.c genrtl.h: gengenrtl
	./gengenrtl genrtl.h genrtl.c

GENSRCS+=	genrtl.c genrtl.h

#-----------------------------------------------------------------------
# C hash codes
c-gperf.h: c-parse.gperf
	gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$$ \
	    ${GCCDIR}/c-parse.gperf > ${.TARGET}
GENSRCS+=	c-gperf.h

#-----------------------------------------------------------------------
# C++ hash codes
gxx-hash.h: gxx.gperf
	gperf -p -j1 -g -o -t -N is_reserved_word '-k1,4,7,$$' \
	    ${GCCDIR}/cp/gxx.gperf >gxx-hash.h
GENSRCS+=	gxx-hash.h

# make a link the the Cygnus used name for now -- hopes are they will change it
hash.h: gxx-hash.h
	ln -sf ${.ALLSRC} ${.TARGET}
GENSRCS+=	hash.h

#-----------------------------------------------------------------------
# Common parser stuff.

tree-check.h: gencheck
	./gencheck > ${.TARGET}
GENSRCS+=	tree-check.h

#-----------------------------------------------------------------------
# C parser
.ORDER: c-parse.c c-parse.h
c-parse.c c-parse.h: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" -e "/^end ifc$$/d" \
	    ${GCCDIR}/c-parse.in > c-parse.y
	${YACC} -d -o c-parse.c c-parse.y
	mv c-parse.y c-parse.y.out

GENSRCS+=	c-parse.c c-parse.h
CLEANFILES+=	c-parse.y c-parse.y.out		# insurance

#-----------------------------------------------------------------------
# objc parser
.ORDER: objc-parse.c objc-parse.h
objc-parse.c objc-parse.h: c-parse.in
	sed -e "/^ifc$$/,/^end ifc$$/d" \
	    -e "/^ifobjc$$/d" -e "/^end ifobjc$$/d" \
	    ${GCCDIR}/c-parse.in > objc-parse.y
	${YACC} -d -o objc-parse.c objc-parse.y
	mv objc-parse.y objc-parse.y.out

GENSRCS+=	objc-parse.c objc-parse.h
CLEANFILES+=	objc-parse.y objc-parse.y.out	# insurance

#-----------------------------------------------------------------------
# C++ parser done in its own makefile
#-----------------------------------------------------------------------
# CPP parser done in its own makefile
#-----------------------------------------------------------------------
# the host/target compiler config.

COMMONHDRS=	config.h hconfig.h multilib.h options.h specs.h tconfig.h tm.h
GENSRCS+=	${COMMONHDRS}

config.h hconfig.h:
	echo '#include "auto-host.h"' > ${.TARGET}
	echo '#include "${GCC_ARCH}/xm-${GCC_ARCH}.h"' >> ${.TARGET}

multilib.h:
	echo 'static char *multilib_raw[] = { \
	    "aout maout;", "elf !maout;", NULL };' > ${.TARGET}
	echo 'static char *multilib_matches_raw[] = { \
	    "maout maout;", "melf melf;", NULL };' >> ${.TARGET}
	echo 'static char *multilib_extra = "";' >> ${.TARGET}

options.h:
	echo '#include "cp/lang-options.h"' > ${.TARGET}
	echo '#include "f/lang-options.h"' >> ${.TARGET}

specs.h:
	echo '#include "cp/lang-specs.h"' > ${.TARGET}
	echo '#include "f/lang-specs.h"'    >> ${.TARGET}

tconfig.h:
	echo '#include "${GCC_ARCH}/xm-${GCC_ARCH}.h"' > ${.TARGET}

tm.h:
	echo '#include "${GCC_ARCH}/${GCC_ARCH}.h"' > ${.TARGET}
.if ${GCC_ARCH} == "i386"
	echo '#include "${GCC_ARCH}/att.h"' >> ${.TARGET}
	echo '#include "svr4.h"' >> ${.TARGET}
.endif
	echo '#include "${GCC_ARCH}/freebsd.h"' >> ${.TARGET}
.if ${GCC_ARCH} == "i386"
	echo '#include "${GCC_ARCH}/perform.h"' >> ${.TARGET}
.endif
	echo '#include <freebsd-native.h>' >> ${.TARGET}

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
${OBJS:Nbitmap.o:Ngenattr.o:Ngencheck.o:Ngencodes.o:Ngenconfig.o:Ngenflags.o:Ngengenrtl.o:Nobstack.o:Nrtl.o}: ${SRCS:M*.h}

# Next, make each of the problematic objects depend on only most headers.
# Filter out the headers that would cause problems (and a few more when it
# is inconvenient to filter precisely).
#
bitmap.o genattr.o gencodes.o genconfig.o genflags.o obstack.o rtl.o: \
    ${SRCS:M*.h:Ninsn-*.h}
gencheck.o: ${SRCS:M*.h:Ngenrtl.h:Ntree-check.h:Ninsn-*.h}
gengenrtl.o: ${SRCS:M*.h:Ngenrtl.h:Ninsn-*.h}
.endif
