# $FreeBSD$
#

PROG=	ssh-add
SRCS=	ssh-add.c

LDADD+=	-lssh -lcrypto
DPADD+= ${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
