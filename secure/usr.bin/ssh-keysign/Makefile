# $FreeBSD$
#

PROG=	ssh-keysign
MAN=	ssh-keysign.8

DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}
LDADD+=	-lssh -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
