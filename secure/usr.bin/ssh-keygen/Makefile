# $FreeBSD$

.include <src.opts.mk>

PROG=	ssh-keygen
# XXX ssh-sk-client.c in libssh maybe?
SRCS=	ssh-keygen.c sshsig.c ssh-sk-client.c
PACKAGE=	ssh
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h

LIBADD=	ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
.endif

LIBADD+=	crypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
