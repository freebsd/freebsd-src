.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	sftp-server
SRCS=	sftp-common.c sftp-server.c sftp-server-main.c $(SKSRCS)
MAN=	sftp-server.8

LIBADD=	ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
