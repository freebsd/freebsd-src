# $FreeBSD$
#

PROG=	ssh-agent
SRCS=	ssh-agent.c log-client.c

LDADD+=	${LIBSSH} -lcrypto -lcrypt -lutil -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
