#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.3 1997/12/02 20:46:18 wollman Exp $

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source
MAN8=	pac.8
SRCS=	pac.c
.PATH:	${.CURDIR}/../common_source
DPADD=	${.OBJDIR}/../common_source/liblpr.a
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include "../../Makefile.inc"
.include <bsd.prog.mk>
