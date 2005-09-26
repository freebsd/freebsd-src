# PAM module for SSH
# $FreeBSD$

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c

WARNS?=	3
CFLAGS+= -I${SSHSRC}

DPADD=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lssh -lcrypto -lcrypt

.include <bsd.lib.mk>
