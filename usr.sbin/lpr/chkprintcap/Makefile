# $FreeBSD: src/usr.sbin/lpr/chkprintcap/Makefile,v 1.7 1999/08/28 01:16:46 peter Exp $

PROG=	chkprintcap
MAN8=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
