# $FreeBSD$
#

PROG=	ssh-add
SRCS=	ssh-add.c

DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}
LDADD+=	-lssh -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
