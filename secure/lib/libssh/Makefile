# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

LIB=	ssh
SRCS=   authfd.c authfile.c bufaux.c buffer.c canohost.c channels.c \
	cipher.c compat.c compress.c crc32.c deattack.c \
	hostfile.c log.c match.c mpaux.c nchan.c packet.c readpass.c \
	rsa.c tildexpand.c ttymodes.c uidswap.c xmalloc.c atomicio.c \
	key.c dispatch.c dsa.c kex.c hmac.c uuencode.c util.c

NOPIC=	yes

install:

.if defined(COMPAT_GETADDRINFO)
SRCS+=	getaddrinfo.c getnameinfo.c name6.c rcmd.c bindresvport.c
.endif

.include <bsd.own.mk>

.PATH:		${SSHSRC}
.PATH:		${SSHSRC}/lib

.if defined(MAKE_KERBEROS4) && \
	((${MAKE_KERBEROS4} == "yes") || (${MAKE_KERBEROS4} == "YES"))
CFLAGS+= -DKRB4
.endif # MAKE_KERBEROS4

.if defined(MAKE_KERBEROS5) && \
	((${MAKE_KERBEROS5} == "yes") || (${MAKE_KERBEROS5} == "YES"))
CFLAGS+= -DKRB5
.endif # MAKE_KERBEROS5

CFLAGS+= -DSKEY
DPADD+=	${LIBOPIE}
LDADD+= -lopie
SRCS+=	auth-skey.c
.include <bsd.lib.mk>
