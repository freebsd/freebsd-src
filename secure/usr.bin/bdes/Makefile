#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

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
