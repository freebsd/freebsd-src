#	$Id: Makefile,v 1.6 1998/09/11 18:49:30 wollman Exp $

PROG=	chkprintcap
MAN8=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
