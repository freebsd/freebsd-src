#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.4 1997/12/02 20:45:52 wollman Exp $

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source -Wall -Werror
MAN8=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c
DPADD=	${.OBJDIR}/../common_source/liblpr.a
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include "../../Makefile.inc"
.include <bsd.prog.mk>
