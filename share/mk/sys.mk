#	@(#)sys.mk	5.11 (Berkeley) 3/13/91

unix=		We run UNIX.

#.SUFFIXES: .out .a .ln .o .c .cc .cxx .C .F .f .e .r .y .l .s .cl .p .h 
.SUFFIXES: .out .a .ln .o .c .F .f .e .r .y .l .s .cl .p .h 

.LIBS:		.a

AR=		ar
ARFLAGS=	rl
RANLIB=		ranlib

AS=		as
AFLAGS=

CC=		cc
CFLAGS=		-O

CXX=		g++
CXXFLAGS=	${CFLAGS}

CPP=		cpp

FC=		f77
FFLAGS=		-O
EFLAGS=

LEX=		lex
LFLAGS=

LD=		ld
LDFLAGS=

LINT=		lint
LINTFLAGS=	-chapbx

MAKE=		make

PC=		pc
PFLAGS=

RC=		f77
RFLAGS=

SHELL=		sh

YACC=		yacc
YFLAGS=-d

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

#.cc.o .cxx.o .C.o:
#	${CXX} ${CXXFLAGS} -c ${.IMPSRC}

.p.o:
	${PC} ${PFLAGS} -c ${.IMPSRC}

.e.o .r.o .F.o .f.o:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} -c ${.IMPSRC}

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}

.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c -o ${.TARGET}
	rm -f y.tab.c

.l.o:
	${LEX} ${LFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c lex.yy.c -o ${.TARGET}
	rm -f lex.yy.c

.y.c:
	${YACC} ${YFLAGS} ${.IMPSRC}
	mv y.tab.c ${.TARGET}

.l.c:
	${LEX} ${LFLAGS} ${.IMPSRC}
	mv lex.yy.c ${.TARGET}

.s.out .c.out .o.out:
	${CC} ${CFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.f.out .F.out .r.out .e.out:
	${FC} ${EFLAGS} ${RFLAGS} ${FFLAGS} ${.IMPSRC} \
	    ${LDLIBS} -o ${.TARGET}
	rm -f ${.PREFIX}.o

.y.out:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} y.tab.c ${LDLIBS} -ly -o ${.TARGET}
	rm -f y.tab.c

.l.out:
	${LEX} ${LFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} lex.yy.c ${LDLIBS} -ll -o ${.TARGET}
	rm -f lex.yy.c

.include <bsd.own.mk>
