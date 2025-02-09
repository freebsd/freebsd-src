# Common Make variables for OpenSSH

.include <src.opts.mk>

SSHDIR=		${SRCTOP}/crypto/openssh

CFLAGS+= -I${SSHDIR} -include ssh_namespace.h
SRCS+=	 ssh_namespace.h

CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE:U/usr/local}/bin/xauth\"

.if ${MK_USB} != "no"
# Built-in security key support
CFLAGS+= -include sk_config.h
.endif
