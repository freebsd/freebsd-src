.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-keygen
SRCS=	ssh-keygen.c sshsig.c $(SKSRCS)
PACKAGE=	ssh

LIBADD=	crypto ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
