# $FreeBSD$
#

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c

LDADD+=	-lssh -lcrypto -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
