# $FreeBSD$
#

PROG=	ssh-keygen
SRCS=	key.c log-client.c ssh-keygen.c

LDADD+=	${LIBSSH} -lcrypto -lcrypt -lutil -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
