# $FreeBSD$
#

PROG=	ssh-keygen
SRCS=	ssh-keygen.c

LDADD+=	-lssh -lcrypto
DPADD+=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
