# $FreeBSD$
#

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c

LDADD+=	${LIBSSH} -lcrypto
DPADD+=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
