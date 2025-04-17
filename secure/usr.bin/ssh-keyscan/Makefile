.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c $(SKSRCS)
PACKAGE=	ssh

LIBADD=	ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
