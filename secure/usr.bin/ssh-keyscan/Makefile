# $FreeBSD$
#

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c

LDADD+=	-lssh -lcrypto
DPADD+=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
