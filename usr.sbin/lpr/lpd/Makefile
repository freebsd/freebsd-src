#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.6 1998/03/07 09:47:59 bde Exp $

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source -Wall
MAN8=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-Werror
.endif

.include <bsd.prog.mk>
