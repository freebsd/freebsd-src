#	from: @(#)sys.mk	8.2 (Berkeley) 3/21/94
#	$Id: sys.mk,v 1.14 1996/05/28 16:20:13 phk Exp $

unix		?=	We run FreeBSD, not UNIX.

.SUFFIXES:	.out .a .ln .o .c .cc .cxx .C .F .f .e .r .y .l .S .s .cl .p .h .sh

.LIBS:		.a

X11BASE		?=	/usr/X11R6

AR		?=	ar
ARFLAGS		?=	rl
RANLIB		?=	ranlib

AS		?=	as
AFLAGS		?=

CC		?=	cc
CFLAGS		?=	-O

CXX		?=	c++
CXXFLAGS	?=	${CXXINCLUDES} ${CFLAGS}

CPP		?=	cpp

.if ${.MAKEFLAGS:M-s} == ""
ECHO		?=	echo
ECHODIR		?=	echo
.else
ECHO		?=	true
.if ${.MAKEFLAGS:M-s} == "-s"
ECHODIR		?=	echo
.else
ECHODIR		?=	true
.endif
.endif

FC		?=	f77
FFLAGS		?=	-O
EFLAGS		?=

INSTALL		?=	install

LEX		?=	lex
LFLAGS		?=

LD		?=	ld
LDFLAGS		?=

LINT		?=	lint
LINTFLAGS	?=	-chapbx

MAKE		?=	make

PC		?=	pc
PFLAGS		?=

RC		?=	f77
RFLAGS		?=

SHELL		?=	sh

YACC		?=	yacc
YFLAGS		?=	-d

.c:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.sh:
	cp -p ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

.cc.o .cxx.o .C.o:
	${CXX} ${CXXFLAGS} -c ${.IMPSRC}

.p.o:
	${PC} ${PFLAGS} -c ${.IMPSRC}

.e.o .r.o .F.o .f.o:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} -c ${.IMPSRC}

.S.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}

# XXX not -j safe
.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c -o ${.TARGET}
	rm -f y.tab.c

.l.o:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} -c ${.PREFIX}.tmp.c -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c

# XXX not -j safe
.y.c:
	${YACC} ${YFLAGS} ${.IMPSRC}
	mv y.tab.c ${.TARGET}

.l.c:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.TARGET}

.s.out .c.out .o.out:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.f.out .F.out .r.out .e.out:
	${FC} ${EFLAGS} ${RFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} \
	    ${LDLIBS} -o ${.TARGET}
	rm -f ${.PREFIX}.o

# XXX not -j safe
.y.out:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} ${LDFLAGS} y.tab.c ${LDLIBS} -ly -o ${.TARGET}
	rm -f y.tab.c

.l.out:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.PREFIX}.tmp.c ${LDLIBS} -ll -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c

.include <bsd.own.mk>

.if exists(/etc/make.conf)
.include </etc/make.conf>
.endif
