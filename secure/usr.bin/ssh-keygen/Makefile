# $FreeBSD$
#

PROG=	ssh-keygen
SRCS=	ssh-keygen.c

LDADD+=	-lssh -lcrypto -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
