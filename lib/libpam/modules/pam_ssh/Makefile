# PAM module for SSH
# $FreeBSD$

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SRCS=		pam_ssh.c
CFLAGS+=	-I${SSHSRC}
DPADD=		${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=		-lssh -lcrypto -lcrypt
MAN=		pam_ssh.8

.include <bsd.lib.mk>
