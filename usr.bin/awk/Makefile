# $FreeBSD$

AWKSRC=	${.CURDIR}/../../contrib/one-true-awk
.PATH: ${AWKSRC}

PROG=	nawk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c

CFLAGS+= -I. -I${AWKSRC}

DPADD=	${LIBM}
LDADD=	-lm

.if ${MACHINE_ARCH} == "sparc64"
LINKS+=	${BINDIR}/nawk ${BINDIR}/awk
MLINKS+=	nawk.1 awk.1
.endif

CLEANFILES+=	maketab proctab.c ytab.c ytab.h

ytab.h: awkgram.h                                                              
	ln -sf ${.ALLSRC} ${.TARGET}

proctab.c: maketab
	./maketab > proctab.c

build-tools: maketab
maketab: ytab.h ${AWKSRC}/maketab.c
	${CC} ${CFLAGS} ${AWKSRC}/maketab.c -o maketab

CLEANFILES+=	nawk.1
nawk.1: awk.1
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
