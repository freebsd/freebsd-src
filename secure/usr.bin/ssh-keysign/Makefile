# $FreeBSD$
#

PROG=	ssh-keysign
SRCS=	ssh-keysign.c
MAN=	ssh-keysign.8

LDADD+=	-lssh -lcrypto
DPADD+=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
