#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.6 1995/12/21 17:57:16 ache Exp $

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
