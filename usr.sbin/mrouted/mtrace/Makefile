#	$Id: Makefile,v 1.2 1995/06/21 18:30:16 wollman Exp $

PROG=	mtrace

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

SRCS=	mtrace.c
MAN8=	${.CURDIR}/../mtrace.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
