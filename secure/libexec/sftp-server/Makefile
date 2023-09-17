
.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=   sftp-server
SRCS=   sftp-server.c sftp-common.c sftp-server-main.c
MAN=	sftp-server.8

LIBADD=	ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
