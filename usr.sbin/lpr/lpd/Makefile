#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source -Wall -Werror
MAN8=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include "../../Makefile.inc"
.include <bsd.prog.mk>
