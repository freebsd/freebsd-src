# $FreeBSD$
#

PROG=	ssh-keygen
SRCS=	ssh-keygen.c

DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}
LDADD+=	-lssh -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
