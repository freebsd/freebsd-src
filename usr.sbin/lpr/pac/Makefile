#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.6 1998/09/11 18:49:33 wollman Exp $

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN8=	pac.8
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
