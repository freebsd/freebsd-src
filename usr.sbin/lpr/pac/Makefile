#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.4 1997/12/16 17:53:39 bde Exp $

PROG=	pac
CFLAGS+=-I${.CURDIR}/../common_source
MAN8=	pac.8
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
