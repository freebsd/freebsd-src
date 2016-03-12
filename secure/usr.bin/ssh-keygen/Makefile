# $FreeBSD$

.include <bsd.own.mk>

PROG=	ssh-keygen
SRCS=	ssh-keygen.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h

DPADD=	${LIBSSH}
LDADD=	-lssh
USEPRIVATELIB= ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
DPADD+=	${LIBLDNS}
LDADD+=	-lldns
USEPRIVATELIB+= ldns
.endif

DPADD+= ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD+= -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
