# $FreeBSD$
#

PROG=	ssh-add
SRCS=	ssh-add.c

LDADD+=	${LIBSSH} -lcrypto
DPADD+= ${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
