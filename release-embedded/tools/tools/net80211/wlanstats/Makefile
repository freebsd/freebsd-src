# $FreeBSD$

.include <bsd.compiler.mk>

PROG=	wlanstats
BINDIR=	/usr/local/bin
NO_MAN=

SRCS=	statfoo.c wlanstats.c main.c
CFLAGS.clang+= -fbracket-depth=512

.include <bsd.prog.mk>
