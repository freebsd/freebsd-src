# $FreeBSD: src/usr.sbin/lpr/chkprintcap/Makefile,v 1.7.2.1 2001/04/25 12:09:52 ru Exp $

PROG=	chkprintcap
MAN=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
