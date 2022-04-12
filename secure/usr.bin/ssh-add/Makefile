.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-add
SRCS+=	ssh-add.c ssh-sk-client.c
PACKAGE=	ssh

LIBADD=	ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
