# $FreeBSD$
#

PROG=	ssh-agent
SRCS=	ssh-agent.c

LDADD+=	${LIBSSH} -lcrypto
DPADD+=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
