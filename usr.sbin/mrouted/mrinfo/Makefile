#	$Id: Makefile,v 1.7 1997/02/22 16:07:29 peter Exp $

PROG=	mrinfo

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

SRCS=	mrinfo.c
MAN8=	${.CURDIR}/../mrinfo.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
