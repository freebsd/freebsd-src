# Common Make variables for OpenSSH

.include <src.opts.mk>

SSHDIR=		${SRCTOP}/crypto/openssh

CFLAGS+= -I${SSHDIR} -include ssh_namespace.h
SRCS+=	 ssh_namespace.h

.if ${MK_USB} != "no"
CFLAGS+=	-DENABLE_SK_INTERNAL=1
.endif
