#
# $Id: Makefile,v 1.11 1998/04/23 16:30:51 bde Exp $
#

#
# This could probably be merged with ../cc_int/Makefile, but bsd.lib.mk
# is such a !@#!*#% nightmare because of how it reprograms the dependencies,
# suffix rules, SRCS, etc.  It's easiest to cheat by using bsd.prog.mk and
# SRCS to get dependencies.
#

# ../Makefile.inc will put an absolute path to our objdir in CFLAGS.
# Prevent mkdep from using it, so that we don't have to give rules for
# aliases of generated headers.
#
CFLAGS+=	-I.

.include "../Makefile.inc"

#-----------------------------------------------------------------------
# Bytecode components

.for i in arity opcode opname
build-tools: bi-$i

bc-$i.h: bi-$i bytecode.def
	./bi-$i < ${GCCDIR}/bytecode.def > bc-$i.h

bi-$i: bi-$i.o bi-parser.o bi-lexer.o bi-reverse.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	bc-$i.h bi-$i.c
CLEANFILES+=	bi-$i
.endfor

.ORDER: bi-parser.c bi-parser.h
bi-parser.c bi-parser.h: bi-parser.y
	${BISON} ${BISONFLAGS} -d ${.ALLSRC} -o bi-parser.c

SRCS+=		bi-lexer.c bi-reverse.c
GENSRCS+=	bi-parser.c bi-parser.h

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

gen$i: gen$i.o rtl.o obstack.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$i.c
CLEANFILES+=	gen$i
.endfor

.for i in attrtab
build-tools: gen$i

gen$i: gen$i.o rtl.o rtlanal.o print-rtl.o obstack.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$i.c
CLEANFILES+=	gen$i
.endfor

SRCS+=		print-rtl.c rtl.c rtlanal.c obstack.c

#-----------------------------------------------------------------------
# C hash codes
c-gperf.h: c-parse.gperf
	gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$$ \
		${GCCDIR}/c-parse.gperf > ${.TARGET}
GENSRCS+=	c-gperf.h

#-----------------------------------------------------------------------
# C++ hash codes
hash.h: gxx.gperf
	gperf -p -j1 -g -o -t -N is_reserved_word '-k1,4,7,$$' \
		${GCCDIR}/cp/gxx.gperf >hash.h
GENSRCS+=	hash.h

#-----------------------------------------------------------------------
# C parser
.ORDER: c-parse.c c-parse.h
c-parse.c c-parse.h: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" -e "/^end ifc$$/d" \
	    ${GCCDIR}/c-parse.in > c-parse.y
	${BISON} -d c-parse.y -o c-parse.c 
	rm -f c-parse.y

GENSRCS+=	c-parse.c c-parse.h
CLEANFILES+=	c-parse.y		# insurance

#-----------------------------------------------------------------------
# objc parser
.ORDER: objc-parse.c objc-parse.h
objc-parse.c objc-parse.h: c-parse.in
	sed -e "/^ifc$$/,/^end ifc$$/d" \
	    -e "/^ifobjc$$/d" -e "/^end ifobjc$$/d" \
	    ${GCCDIR}/c-parse.in > objc-parse.y
	${BISON} -d objc-parse.y -o objc-parse.c 
	rm -f objc-parse.y

GENSRCS+=	objc-parse.c objc-parse.h
CLEANFILES+=	objc-parse.y		# insurance

#-----------------------------------------------------------------------
# C++ parser done in its own makefile
#-----------------------------------------------------------------------
# CPP parser done in its own makefile
#-----------------------------------------------------------------------
# the host/target compiler config.

COMMONHDRS=	config.h hconfig.h tconfig.h tm.h options.h specs.h

${COMMONHDRS}:
	echo '#include "${MACHINE_ARCH}/freebsd.h"'     > tm.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > config.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > hconfig.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > tconfig.h
	echo '#include "cp/lang-options.h"'             > options.h
	echo '#include "cp/lang-specs.h"'               > specs.h
	echo '#include "f2c-specs.h"'                   >> specs.h

GENSRCS+=	${COMMONHDRS}

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
# Fudge pre-dependfile dependencies of objects the same as bsd.prog.mk
# would do if we defined PROG, except for leaving out dependencies on
# bc-*.h and insn-*.h because these dependencies would be circular.
#
${OBJS}: ${SRCS:M*.h:Nbc-*.h:Ninsn-*.h}

# Give all dependencies on bc-*.h and insn-*.h explicitly (none here for
# bc-*.h).  This suffices for `make -j<any> depend', and after that all
# the dependencies will be in .depend since all .c source files are in
# SRCS.
#
genattrtab.o genextract.o: insn-config.h
insn-attrtab.o: insn-attr.h insn-config.h
insn-emit.o: insn-codes.h insn-config.h insn-flags.h
insn-opinit.o: insn-codes.h insn-config.h insn-flags.h
insn-output.o: insn-attr.h insn-codes.h insn-config.h insn-flags.h
insn-recog.o: insn-config.h
.endif
