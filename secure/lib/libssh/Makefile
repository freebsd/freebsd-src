# $FreeBSD$
#

LIB=	ssh
SRCS=   authfd.c authfile.c bufaux.c buffer.c canohost.c channels.c \
	cipher.c compat.c compress.c crc32.c deattack.c \
	hostfile.c log.c match.c mpaux.c nchan.c packet.c readpass.c \
	rsa.c tildexpand.c ttymodes.c uidswap.c xmalloc.c atomicio.c \
	key.c dispatch.c kex.c mac.c uuencode.c misc.c \
	cli.c rijndael.c ssh-dss.c ssh-rsa.c dh.c kexdh.c kexgex.c \
	version.c

.if defined(COMPAT_GETADDRINFO)
SRCS+=	getaddrinfo.c getnameinfo.c name6.c rcmd.c bindresvport.c
.endif

.if defined(MAKE_KERBEROS4) && \
	((${MAKE_KERBEROS4} == "yes") || (${MAKE_KERBEROS4} == "YES"))
CFLAGS+= -DKRB4
.endif # MAKE_KERBEROS4

.if defined(MAKE_KERBEROS5) && \
	((${MAKE_KERBEROS5} == "yes") || (${MAKE_KERBEROS5} == "YES"))
CFLAGS+= -DKRB5
.endif # MAKE_KERBEROS5

DPADD=	${LIBCRYPTO} ${LIBZ}
LDADD=	-lcrypto -lz

.include <bsd.lib.mk>

.PATH:	${SSHDIR} ${SSHDIR}/lib
