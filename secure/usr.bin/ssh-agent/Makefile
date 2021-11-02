# $FreeBSD$

.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-agent
SRCS=	ssh-agent.c
PACKAGE=	ssh

LIBADD=	ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
.endif

LIBADD+=	crypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
