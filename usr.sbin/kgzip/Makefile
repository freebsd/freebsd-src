# $FreeBSD: src/usr.sbin/kgzip/Makefile,v 1.2.2.2 2001/07/19 04:37:24 kris Exp $

PROG=	kgzip
SRCS=	kgzip.c aouthdr.c elfhdr.c kgzcmp.c kgzld.c xio.c
MAN=	kgzip.8
CFLAGS+=-pedantic \
	-W -Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes \
	-Wwrite-strings

.include <bsd.prog.mk>
