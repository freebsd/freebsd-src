# $FreeBSD$
#

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c

DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}
LDADD+=	-lssh -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
