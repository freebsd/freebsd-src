#	$Id: Makefile,v 1.5 1996/06/24 04:25:17 jkh Exp $

PROG=	mrinfo

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

SRCS=	mrinfo.c
MAN8=	${.CURDIR}/../mrinfo.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
