#	$Id: Makefile,v 1.5 1997/02/22 16:07:26 peter Exp $

PROG=	map-mbone

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
.if exists(${.OBJDIR}/../common)
LDADD+=	-L${.OBJDIR}/../common
DPADD+=	${.OBJDIR}/../common/libmrouted.a
.else
LDADD+=	-L$S/common
DPADD+=	$S/common/libmrouted.a
.endif
LDADD+=	-lmrouted

SRCS=	mapper.c
MAN8=	${.CURDIR}/../map-mbone.8

.include <bsd.prog.mk>
