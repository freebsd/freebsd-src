# $FreeBSD: src/usr.sbin/lpr/chkprintcap/Makefile,v 1.6.2.1 1999/08/29 15:43:03 peter Exp $

PROG=	chkprintcap
MAN8=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
