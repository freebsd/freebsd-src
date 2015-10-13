# $FreeBSD$

.include <src.opts.mk>

PROG=	ssh-keysign
SRCS=	ssh-keysign.c roaming_dummy.c readconf.c
MAN=	ssh-keysign.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
BINMODE=4555

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
