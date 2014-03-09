# $FreeBSD$

.include <bsd.compiler.mk>

PROG=	wlanstats
BINDIR=	/usr/local/bin
NO_MAN=
USEPRIVATELIB=

SRCS=	wlanstats.c main.c
LDADD=	-lbsdstat
CFLAGS.clang+= -fbracket-depth=512

.include <bsd.prog.mk>
