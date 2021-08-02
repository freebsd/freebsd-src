# $FreeBSD$

.include <src.opts.mk>

AWKSRC=	${SRCTOP}/contrib/one-true-awk
.PATH: ${AWKSRC}

PROG=	awk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c awkgram.tab.h

CFLAGS+= -DHAS_ISBLANK -I. -I${AWKSRC} -DFOPEN_MAX=64

WARNS?=	1

LIBADD=	m

LINKS=	${BINDIR}/awk ${BINDIR}/nawk
MLINKS=	awk.1 nawk.1

CLEANFILES= maketab proctab.c awkgram.tab.h

awkgram.tab.h: awkgram.h
	ln -sf ${.ALLSRC:M*.h} ${.TARGET}

proctab.c: maketab awkgram.tab.h
	${BTOOLSPATH:U.}/maketab awkgram.tab.h > proctab.c

DEPENDOBJS+= maketab
build-tools: maketab
maketab: awkgram.tab.h ${BUILD_TOOLS_META}

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
