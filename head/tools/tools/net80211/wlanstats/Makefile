# $FreeBSD$

.include <bsd.compiler.mk>

PROG=	wlanstats
BINDIR=	/usr/local/bin
MAN=
USEPRIVATELIB=  bsdstat
LDADD=  ${LDBSDSTAT}

SRCS=	wlanstats.c main.c

CFLAGS.clang+= -fbracket-depth=512

.include <bsd.prog.mk>
