# PAM module for SSH
# $FreeBSD$

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SHLIB_NAME=	pam_ssh.so
SRCS=		pam_ssh.c
CFLAGS+=	-I${SSHSRC}
DPADD=		${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}
LDADD=		-L${.OBJDIR}/../../../../secure/lib/libssh -lssh -lcrypto \
		-lcrypt -lutil -lz
MAN=		pam_ssh.8

.include <bsd.lib.mk>
