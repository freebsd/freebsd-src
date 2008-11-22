# $FreeBSD$

AWKSRC=	${.CURDIR}/../../contrib/one-true-awk
.PATH: ${AWKSRC}

PROG=	nawk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c ytab.h

CFLAGS+= -DHAS_ISBLANK -I. -I${AWKSRC}

DPADD=	${LIBM}
LDADD=	-lm

LINKS=	${BINDIR}/nawk ${BINDIR}/awk
MLINKS=	nawk.1 awk.1

CLEANFILES= maketab proctab.c ytab.h

ytab.h: awkgram.h
	ln -sf ${.ALLSRC} ${.TARGET}

proctab.c: maketab
	./maketab > proctab.c

build-tools: maketab
maketab: ytab.h ${AWKSRC}/maketab.c

CLEANFILES+=	nawk.1
nawk.1: awk.1
	cat ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
