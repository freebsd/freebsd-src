#	$Id: Makefile,v 1.1.6.1 1995/09/06 10:34:19 davidg Exp $

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
BINMODE=4755

.include <bsd.prog.mk>
