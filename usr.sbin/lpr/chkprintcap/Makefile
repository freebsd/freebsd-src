#	$Id: Makefile,v 1.5 1998/06/11 03:51:34 jb Exp $

PROG=	chkprintcap
MAN8=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
