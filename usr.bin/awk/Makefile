# $FreeBSD$

AWKSRC=	${.CURDIR}/../../contrib/one-true-awk
.PATH: ${AWKSRC}

PROG=	awk
SRCS=	b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c ytab.c ytab.h
MAN1=	awk.1

# Important for ctype macros!
CFLAGS+= -funsigned-char
CFLAGS+= -I. -I${AWKSRC}

DPADD+=	${LIBM}
LDADD+=	-lm

LINKS+=	${BINDIR}/awk ${BINDIR}/nawk
MLINKS+=	awk.1 nawk.1

CLEANFILES+=	maketab proctab.c ytab.c ytab.h

.ORDER:	ytab.c ytab.h proctab.c
ytab.c ytab.h: ${AWKSRC}/awkgram.y
	@echo Expect 42 reduce/shift conflicts and 83 reduce/reduce conflicts
	${YACC} -d ${AWKSRC}/awkgram.y
	mv -f y.tab.c ytab.c
	mv -f y.tab.h ytab.h

proctab.c: maketab
	./maketab > proctab.c

build-tools: maketab
maketab: ytab.h ${AWKSRC}/maketab.c
	${CC} ${CFLAGS} ${AWKSRC}/maketab.c -o maketab

.include <bsd.prog.mk>
