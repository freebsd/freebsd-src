# $FreeBSD$

PROG=	ssh-add

DPADD=	${LIBSSH} ${LIBCRYPTO}
LDADD=	-lssh -lcrypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
