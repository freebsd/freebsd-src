#
# $Id: Makefile,v 1.5 1998/01/11 04:13:25 jb Exp $
#

#
# This could probably be merged with ../cc_int/Makefile, but bsd.lib.mk
# is such a !@#!*#% nightmare because of how it reporgrams the dependencies,
# suffix rules, SRCS, etc.  It's easiest to cheat by using bsd.prog.mk and
# SRCS to get dependencies.
#

.include "../Makefile.inc"

#-----------------------------------------------------------------------
# Bytecode components

.for i in arity opcode opname
bc-$i.h:	bi-$i bytecode.def
	./bi-$i < ${GCCDIR}/bytecode.def > bc-$i.h

bi-$i:	bi-$i.o bi-parser.o bi-lexer.o bi-reverse.o
	${CC} ${.ALLSRC} -o ${.TARGET}

CLEANFILES+= bi-$i bi-$i.o
SRCS+= bc-$i.h

.endfor

.ORDER: bi-parser.c bi-parser.h
bi-parser.c bi-parser.h:	bi-parser.y
	${BISON} ${BISONFLAGS} -d ${.ALLSRC} -o bi-parser.c

SRCS+= bi-parser.c bi-parser.h

CLEANFILES+= bi-lexer.o bi-parser.o bi-reverse.o

#-----------------------------------------------------------------------
# insn-* gunk

.for i in config flags codes attr
insn-$i.h:	gen$i ${MD_FILE}
	./gen$i ${MD_FILE} > insn-$i.h
SRCS+= insn-$i.h
.endfor

.for i in emit recog opinit extract peep attrtab output
insn-$i.c:	gen$i ${MD_FILE}
	./gen$i ${MD_FILE} > insn-$i.c
SRCS+= insn-$i.c
.endfor

.for i in config flags codes emit opinit recog extract peep attr output
gen$i:	gen$i.o rtl.o obstack.o
	${CC} ${.ALLSRC} -o ${.TARGET}

gen$i.o:	gen$i.c ${RTL_H}
CLEANFILES+= gen$i gen$i.o
.endfor

.for i in attrtab
gen$i:	gen$i.o rtl.o rtlanal.o print-rtl.o obstack.o
	${CC} ${.ALLSRC} -o ${.TARGET}

gen$i.o:	gen$i.c ${RTL_H}
CLEANFILES+= gen$i gen$i.o
.endfor

CLEANFILES+= print-rtl.o rtl.o rtlanal.o obstack.o

#-----------------------------------------------------------------------
# C hash codes
c-gperf.h: c-parse.gperf
	gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$$ \
		${GCCDIR}/c-parse.gperf > ${.TARGET}
SRCS+=	c-gperf.h

#-----------------------------------------------------------------------
# C++ hash codes
hash.h: gxx.gperf
	gperf -p -j1 -g -o -t -N is_reserved_word '-k1,4,7,$$' \
		${GCCDIR}/cp/gxx.gperf >hash.h
SRCS+=	hash.h

#-----------------------------------------------------------------------
# C parser
.ORDER: c-parse.c c-parse.h
c-parse.c c-parse.h: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" -e "/^end ifc$$/d" \
	    ${GCCDIR}/c-parse.in > c-parse.y
	${BISON} -d c-parse.y -o c-parse.c 
	rm -f c-parse.y

SRCS+=	c-parse.c c-parse.h
CLEANFILES+= c-parse.y		# insurance

#-----------------------------------------------------------------------
# objc parser
.ORDER: objc-parse.c objc-parse.h
objc-parse.c objc-parse.h: c-parse.in
	sed -e "/^ifc$$/,/^end ifc$$/d" \
	    -e "/^ifobjc$$/d" -e "/^end ifobjc$$/d" \
	    ${GCCDIR}/c-parse.in > objc-parse.y
	${BISON} -d objc-parse.y -o objc-parse.c 
	rm -f objc-parse.y

SRCS+=	objc-parse.c objc-parse.h
CLEANFILES+= objc-parse.y		# insurance

#-----------------------------------------------------------------------
# C++ parser done in it's own makefile
#-----------------------------------------------------------------------
# CPP parser done in it's own makefile
#-----------------------------------------------------------------------
# All generates sources are cleaned
CLEANFILES+=	${SRCS}

#-----------------------------------------------------------------------
all:		${BINFORMAT} ${SRCS}

#-----------------------------------------------------------------------
# Make 'depend' in compat mode
.if make(depend)
.SINGLESHELL:
.endif

beforedepend:	${BINFORMAT}

#-----------------------------------------------------------------------
# the host/target compiler config.

aout:
	@rm -f elf
	echo '#include "${MACHINE_ARCH}/freebsd.h"'     > tm.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > config.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > hconfig.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > tconfig.h
	echo '#include "cp/lang-options.h"'             > options.h
	echo '#include "cp/lang-specs.h"'               > specs.h
	echo '#include "f2c-specs.h"'                   >> specs.h
	@touch aout

elf:
	@rm -f aout
	echo '#include "${MACHINE_ARCH}/freebsd-elf.h"' > tm.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > config.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > hconfig.h
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"'  > tconfig.h
	echo '#include "cp/lang-options.h"'             > options.h
	echo '#include "cp/lang-specs.h"'               > specs.h
	echo '#include "f2c-specs.h"'                   >> specs.h
	@touch elf

CLEANFILES+=	config.h hconfig.h tconfig.h tm.h options.h specs.h elf aout

.include <bsd.prog.mk>
