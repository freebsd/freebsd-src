#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	bdes
SRCS=	bdes.c

.if exists(${.OBJDIR}/../../lib/libcipher)
LDFLAGS+= -L${.OBJDIR}/../../lib/libcipher
.else
LDFLAGS+= -L${.CURDIR}/../../lib/libcipher
.endif

LDADD+= -lcipher
DPADD+= libcipher.a

.include <bsd.prog.mk>
