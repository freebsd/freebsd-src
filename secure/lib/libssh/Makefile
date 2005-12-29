# $FreeBSD$

LIB=	ssh
SHLIB_MAJOR=	3
SRCS=	acss.c authfd.c authfile.c bufaux.c buffer.c \
	canohost.c channels.c cipher.c cipher-acss.c cipher-aes.c \
	cipher-bf1.c cipher-ctr.c cipher-3des1.c cleanup.c \
	compat.c compress.c crc32.c deattack.c fatal.c hostfile.c \
	log.c match.c moduli.c nchan.c packet.c \
	readpass.c rsa.c ttymodes.c xmalloc.c \
	atomicio.c key.c dispatch.c kex.c mac.c uidswap.c uuencode.c misc.c \
	monitor_fdpass.c rijndael.c ssh-dss.c ssh-rsa.c dh.c kexdh.c \
	kexgex.c kexdhc.c kexgexc.c scard.c msg.c progressmeter.c dns.c \
	entropy.c scard-opensc.c

# gss-genr.c should be in $SRCS but causes linking problems, so it is
# compiled directly into sshd instead.

# Portability layer
SRCS+=	bsd-closefrom.c bsd-misc.c getrrsetbyname.c \
	strtonum.c vis.c xcrypt.c xmmap.c
# FreeBSD additions
SRCS+=	version.c

.if defined(COMPAT_GETADDRINFO)
SRCS+=	getaddrinfo.c getnameinfo.c name6.c rcmd.c bindresvport.c
.endif

CFLAGS+= -I${SSHDIR}
DPADD=	${LIBZ}
LDADD=	-lz

.if !defined(NO_KERBEROS)
CFLAGS+= -DGSSAPI -DHAVE_GSSAPI_GSSAPI_H=1 -DKRB5 -DHEIMDAL
DPADD+=	${LIBGSSAPI} ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBMD} ${LIBROKEN}
LDADD+=	-lgssapi -lkrb5 -lasn1 -lcom_err -lmd -lroken
.endif

NO_LINT=

DPADD+=	${LIBCRYPTO} ${LIBCRYPT}
LDADD+=	-lcrypto -lcrypt

.include <bsd.lib.mk>

.PATH: ${SSHDIR} ${SSHDIR}/openbsd-compat
