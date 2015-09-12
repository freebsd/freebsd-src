# $FreeBSD$

.include <src.opts.mk>

PROG=	ssh-agent
SRCS=	ssh-agent.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
PACKAGE=	ssh

.if !defined(NO_SHARED)
# required when linking with a dynamic libssh
SRCS+=	roaming_dummy.c
.endif

LIBADD=	ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
.endif

LIBADD+=	crypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
