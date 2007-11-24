# $FreeBSD$

PROG=	ssh-keyscan
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBMD} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lmd -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
