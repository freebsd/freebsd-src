#	$Id: Makefile,v 1.2 1995/03/31 21:16:56 wollman Exp $

PROG=	map-mbone

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
LDADD+=	-lmrouted
.if exists(${.OBJDIR}/../common)
LDDESTDIR+=	-L${.OBJDIR}/../common
DPADD+= 	${.OBJDIR}/../common/libmrouted.a
.else
LDDESTDIR+=	-L$S/common
DPADD+=		$S/common/libmrouted.a
.endif

SRCS=	mapper.c
MAN8=	${.CURDIR}/../map-mbone.8

.include <bsd.prog.mk>
