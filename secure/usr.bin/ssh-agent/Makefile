# $FreeBSD$
#

PROG=	ssh-agent
SRCS=	ssh-agent.c

LDADD+=	-lssh -lcrypto
DPADD+=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
