#	$OpenBSD: Makefile,v 1.36 2002/06/11 15:23:29 hin Exp $

.PATH:		${.CURDIR}/..

LIB=	ssh
SRCS=	authfd.c authfile.c bufaux.c buffer.c canohost.c channels.c \
	cipher.c compat.c compress.c crc32.c deattack.c fatal.c \
	hostfile.c log.c match.c mpaux.c nchan.c packet.c readpass.c \
	rsa.c tildexpand.c ttymodes.c xmalloc.c atomicio.c \
	key.c dispatch.c kex.c mac.c uuencode.c misc.c \
	rijndael.c ssh-dss.c ssh-rsa.c dh.c kexdh.c kexgex.c \
	scard.c monitor_wrap.c monitor_fdpass.c msg.c

DEBUGLIBS= no
NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV
.endif # KERBEROS5

.if (${KERBEROS:L} == "yes")
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
SRCS+=  radix.c
.endif # AFS
.endif # KERBEROS

.include <bsd.lib.mk>
