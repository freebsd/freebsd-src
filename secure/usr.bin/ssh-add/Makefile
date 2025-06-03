.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-add
SRCS=	ssh-add.c $(SKSRCS)
PACKAGE=	ssh

LIBADD=	ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
