#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.5 1994/11/14 20:45:35 phk Exp $

PROG=	bdes
SRCS=	bdes.c

.if exists(${.CURDIR}/../../lib/libcipher/obj)
LDFLAGS+= -L${.CURDIR}/../../lib/libcipher/obj
.else
LDFLAGS+= -L${.CURDIR}/../../lib/libcipher
.endif

LDADD+= -lcipher
DPADD+= libcipher.a

.include <bsd.prog.mk>
