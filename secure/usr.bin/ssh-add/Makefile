# $FreeBSD$
#

PROG=	ssh-add
SRCS=	ssh-add.c

LDADD+=	-lssh -lcrypto -lz
DPADD+= ${LIBSSH} ${LIBCRYPTO} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
