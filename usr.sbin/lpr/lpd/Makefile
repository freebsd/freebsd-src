#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.8 1998/06/11 03:52:41 jb Exp $

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN8=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
