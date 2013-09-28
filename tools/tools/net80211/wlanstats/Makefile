# $FreeBSD$

.include <bsd.compiler.mk>

PROG=	wlanstats
BINDIR=	/usr/local/bin
NO_MAN=

SRCS=	statfoo.c wlanstats.c main.c
.if ${COMPILER_TYPE} == "clang"
CFLAGS+= -fbracket-depth=512
.endif

.include <bsd.prog.mk>
