# $FreeBSD$
#

PROG=	scp
SRCS=	scp.c scp-common.c

LDADD+=	-lssh -lcrypto -lutil -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
