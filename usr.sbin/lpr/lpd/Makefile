#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.5 1997/12/16 17:53:25 bde Exp $

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source -Wall -Werror
MAN8=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
