#	$Id: Makefile,v 1.1 1994/09/08 02:51:34 wollman Exp $

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
LDADD+=	-lmrouted
.if exists($S/common/obj)
LDDESTDIR+=	-L$S/common/obj
DPADD+= 	$S/common/obj/libmrouted.a
.else
LDDESTDIR+=	-L$S/common
DPADD+=		$S/common/libmrouted.a
.endif

SRCS=	mrinfo.c
MAN8=	${.CURDIR}/../mrinfo.8

.include <bsd.prog.mk>
