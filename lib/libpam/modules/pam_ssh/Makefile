# PAM module for SSH
# $FreeBSD$

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SHLIB_NAME=	pam_ssh.so
SRCS=		pam_ssh.c
CFLAGS+=	-I${SSHSRC}
DPADD=		${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}
LDADD=		-lssh -lcrypto -lcrypt -lutil -lz
MAN=		pam_ssh.8

.include <bsd.lib.mk>
