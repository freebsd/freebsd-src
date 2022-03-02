# Common Make variables for OpenSSH

.include <src.opts.mk>

SSHDIR=		${SRCTOP}/crypto/openssh

CFLAGS+= -I${SSHDIR} -include ssh_namespace.h

.if ${MK_GSSAPI} != "no" && ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -include krb5_config.h
.endif

CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE:U/usr/local}/bin/xauth\"

.if ${MK_USB} != "no"
# Built-in security key support
CFLAGS+= -include sk_config.h
.endif
