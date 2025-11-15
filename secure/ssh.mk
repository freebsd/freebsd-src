# Common Make variables for OpenSSH

.include <src.opts.mk>

SSHDIR=		${SRCTOP}/crypto/openssh

SFTP_CLIENT_SRCS=sftp-common.c sftp-client.c sftp-glob.c
SKSRCS=	ssh-sk-client.c

CFLAGS+= -I${SSHDIR} -include ssh_namespace.h

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -include krb5_config.h
.if ${MK_MITKRB5} == "no"
CFLAGS+= -DHEIMDAL=1
.endif
.endif

CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE:U/usr/local}/bin/xauth\"

.if ${MK_LDNS} != "no"
CFLAGS+= -DHAVE_LDNS=1
.endif

.if ${MK_TCP_WRAPPERS} != "no"
CFLAGS+= -DLIBWRAP=1
.endif

.if ${MK_USB} != "no"
# Built-in security key support
CFLAGS+= -include sk_config.h
.endif

CFLAGS+= -DOPENSSL_API_COMPAT=0x10100000L
