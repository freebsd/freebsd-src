#	$Id: Makefile,v 1.6 1997/02/22 16:07:34 peter Exp $

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
.if exists(${.OBJDIR}/../common)
LDADD+=	-L${.OBJDIR}/../common
DPADD+= ${.OBJDIR}/../common/libmrouted.a
.else
LDADD+=	-L$S/common
DPADD+=	$S/common/libmrouted.a
.endif
LDADD+=	-lmrouted

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c
CLEANFILES+= y.tab.h
MAN8=	${.CURDIR}/../mrouted.8

.include <bsd.prog.mk>
