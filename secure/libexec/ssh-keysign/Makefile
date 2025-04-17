.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-keysign
SRCS=	ssh-keysign.c readconf.c uidswap.c $(SKSRCS)
MAN=	ssh-keysign.8
BINMODE=4555

LIBADD=	crypto ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
