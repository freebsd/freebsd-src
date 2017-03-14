# PAM module for SSH
# $FreeBSD$

SSHDIR=	${SRCTOP}/crypto/openssh

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c
PACKAGE=	ssh

WARNS?=	5
CFLAGS+= -I${SSHDIR} -include ssh_namespace.h
.ifdef LIBCHERI
# This library often causes `relocation R_MIPS_GOT_PAGE out of range` errors
CFLAGS+=-mllvm -mxmxgot
.endif
SRCS+=	 ssh_namespace.h

LIBADD=	ssh

.include <bsd.lib.mk>

.PATH: ${SSHDIR}
