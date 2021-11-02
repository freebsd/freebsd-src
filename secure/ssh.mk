# Common Make variables for OpenSSH

SSHDIR=		${SRCTOP}/crypto/openssh

CFLAGS+= -I${SSHDIR} -include ssh_namespace.h
SRCS+=	 ssh_namespace.h
