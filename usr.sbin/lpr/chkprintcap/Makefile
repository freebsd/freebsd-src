# $FreeBSD$

.PATH: ${.CURDIR}/../common_source

PROG=	chkprintcap
MAN=	chkprintcap.8

CFLAGS+= -I${.CURDIR}/../common_source ${CWARNFLAGS}

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
