# $FreeBSD$

.include <bsd.compiler.mk>

PROG=	wlanstats
BINDIR=	/usr/local/bin
MAN=
LIBADD=	bsdstat

SRCS=	wlanstats.c main.c

CFLAGS.clang+= -fbracket-depth=512

.include <bsd.prog.mk>
