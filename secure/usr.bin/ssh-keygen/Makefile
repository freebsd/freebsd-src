.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-keygen
# XXX ssh-sk-client.c in libssh maybe?
SRCS=	ssh-keygen.c sshsig.c ssh-sk-client.c
PACKAGE=	ssh

LIBADD=	crypto ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
