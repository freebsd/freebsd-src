# $FreeBSD$
#

PROG=	scp

LDADD+=	${LIBSSH} -lcrypto -lutil -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
