#	$Id: Makefile,v 1.1.6.1 1995/09/06 10:34:18 davidg Exp $

PROG=	mrouted

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

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c
CLEANFILES+= y.tab.h
MAN8=	${.CURDIR}/../mrouted.8

.include <bsd.prog.mk>
