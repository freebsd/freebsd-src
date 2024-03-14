# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: suffixes.mk,v 1.3 2024/02/17 17:26:57 sjg Exp $
#
#	@(#) Copyright (c) 2024, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

# some reasonable defaults
.SUFFIXES: .out .a .ln .o ${PICO} ${PCM} .s .S .c ${CXX_SUFFIXES} \
	${CCM_SUFFIXES} .F .f .r .y .l .cl .p .h \
	.sh .m4 .cpp-out

#
AFLAGS ?=
ARFLAGS ?=	r
.if ${MACHINE_ARCH} == "sparc64"
AFLAGS+= -Wa,-Av9a
.endif
AS ?=		as
CC ?=		cc
CFLAGS ?=	${DBG}
CXX ?=		c++
CXXFLAGS ?=	${CFLAGS}
CXXFLAGS ?=	${CFLAGS}
DBG ?=		-O2
FC ?=		f77
FFLAGS ?=	-O
INSTALL ?=	install
LD ?=		ld
LEX ?=		lex
LFLAGS ?=
NM ?=		nm
OBJC ?=		${CC}
OBJCFLAGS ?=	${CFLAGS}
PC ?=		pc
PFLAGS ?=
RFLAGS ?=
SIZE ?=		size
YACC ?=		yacc
YFLAGS ?=

COMPILE.s ?=	${CC} ${AFLAGS} -c
LINK.s ?=	${CC} ${AFLAGS} ${LDFLAGS}
COMPILE.S ?=	${CC} ${AFLAGS} ${CPPFLAGS} -c -traditional-cpp
LINK.S ?=	${CC} ${AFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.c ?=	${CC} ${CFLAGS} ${CPPFLAGS} -c
LINK.c ?=	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.cc ?=	${CXX} ${CXXFLAGS} ${CPPFLAGS} -c
COMPILE.pcm ?=	${COMPILE.cc:N-c} --precompile -c
LINK.cc ?=	${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.m ?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} -c
LINK.m ?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.f ?=	${FC} ${FFLAGS} -c
LINK.f ?=	${FC} ${FFLAGS} ${LDFLAGS}
COMPILE.F ?=	${FC} ${FFLAGS} ${CPPFLAGS} -c
LINK.F ?=	${FC} ${FFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.r ?=	${FC} ${FFLAGS} ${RFLAGS} -c
LINK.r ?=	${FC} ${FFLAGS} ${RFLAGS} ${LDFLAGS}
LEX.l ?=	${LEX} ${LFLAGS}
COMPILE.p ?=	${PC} ${PFLAGS} ${CPPFLAGS} -c
LINK.p ?=	${PC} ${PFLAGS} ${CPPFLAGS} ${LDFLAGS}
YACC.y ?=	${YACC} ${YFLAGS}
LEX.l ?=	${LEX} ${LFLAGS}

# C
.c:
	${LINK.c} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.c.o:
	${COMPILE.c} ${.IMPSRC}
.c.a:
	${COMPILE.c} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o
.c.ln:
	${LINT} ${LINTFLAGS} ${CPPFLAGS:M-[IDU]*} -i ${.IMPSRC}

# C++
${CXX_SUFFIXES}:
	${LINK.cc} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
${CXX_SUFFIXES:%=%.o}:
	${COMPILE.cc} ${.IMPSRC}
${CXX_SUFFIXES:%=%.a}:
	${COMPILE.cc} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o

# C++ precompiled modules
${CCM_SUFFIXES:%=%${PCM}}:
	@${COMPILE.pcm} ${.IMPSRC}

# Fortran/Ratfor
.f:
	${LINK.f} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.f.o:
	${COMPILE.f} ${.IMPSRC}
.f.a:
	${COMPILE.f} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o

.F:
	${LINK.F} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.F.o:
	${COMPILE.F} ${.IMPSRC}
.F.a:
	${COMPILE.F} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o

.r:
	${LINK.r} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.r.o:
	${COMPILE.r} ${.IMPSRC}
.r.a:
	${COMPILE.r} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o

# Pascal
.p:
	${LINK.p} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.p.o:
	${COMPILE.p} ${.IMPSRC}
.p.a:
	${COMPILE.p} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o

# Assembly
.s:
	${LINK.s} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.s.o:
	${COMPILE.s} ${.IMPSRC}
.s.a:
	${COMPILE.s} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o
.S:
	${LINK.S} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.S.o:
	${COMPILE.S} ${.IMPSRC}
.S.a:
	${COMPILE.S} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o

# Lex
.l:
	${LEX.l} ${.IMPSRC}
	${LINK.c} -o ${.TARGET} lex.yy.c ${LDLIBS} -ll
	rm -f lex.yy.c
.l.c:
	${LEX.l} ${.IMPSRC}
	mv lex.yy.c ${.TARGET}
.l.o:
	${LEX.l} ${.IMPSRC}
	${COMPILE.c} -o ${.TARGET} lex.yy.c
	rm -f lex.yy.c

# Yacc
.y:
	${YACC.y} ${.IMPSRC}
	${LINK.c} -o ${.TARGET} y.tab.c ${LDLIBS}
	rm -f y.tab.c
.y.c:
	${YACC.y} ${.IMPSRC}
	mv y.tab.c ${.TARGET}
.y.o:
	${YACC.y} ${.IMPSRC}
	${COMPILE.c} -o ${.TARGET} y.tab.c
	rm -f y.tab.c

# Shell
.sh:
	rm -f ${.TARGET}
	cp ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}


# this often helps with debugging
.c.cpp-out:
	@${COMPILE.c:N-c} -E ${.IMPSRC} | grep -v '^[ 	]*$$'

${CXX_SUFFIXES:%=%.cpp-out}:
	@${COMPILE.cc:N-c} -E ${.IMPSRC} | grep -v '^[ 	]*$$'
