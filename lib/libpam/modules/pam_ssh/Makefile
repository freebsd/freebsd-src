# PAM module for SSH
# $FreeBSD$

SSHDIR=	${.CURDIR}/../../../../crypto/openssh

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c

WARNS?=	5
CFLAGS+= -I${SSHDIR} -include ssh_namespace.h
SRCS+=	 ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lssh -lcrypto -lcrypt
USEPRIVATELIB= ssh

.include <bsd.lib.mk>

.PATH: ${SSHDIR}
