# $FreeBSD$
#

PROG=	ssh-agent
SRCS=	ssh-agent.c

LDADD+=	-lssh -lcrypto -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
