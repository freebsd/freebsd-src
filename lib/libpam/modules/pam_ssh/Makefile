# PAM module for SSH
# $FreeBSD$

SSHSRC=	${.CURDIR}/../../../../crypto/openssh
LIBSSH=	${.OBJDIR}/../../../../secure/lib/libssh/libssh.a

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c

WARNS?=	0
CFLAGS+= -I${SSHSRC}

DPADD=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	${LIBSSH} -lcrypto -lcrypt

.include <bsd.lib.mk>
