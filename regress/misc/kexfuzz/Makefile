#	$OpenBSD: Makefile,v 1.4 2019/01/21 12:50:12 djm Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

# XXX detect from ssh binary?
SSH1?=		no
OPENSSL?=	yes

PROG=	kexfuzz
SRCS=	kexfuzz.c

SSHREL=../../../../../usr.bin/ssh
.PATH: ${.CURDIR}/${SSHREL}
# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=atomicio.c sshkey.c authfile.c cipher.c log.c ssh-rsa.c ssh-dss.c
SRCS+=ssh-ecdsa.c ssh-ed25519.c mac.c umac.c umac128.c hmac.c misc.c
SRCS+=ssherr.c uidswap.c cleanup.c xmalloc.c match.c krl.c fatal.c
SRCS+=addrmatch.c bitmap.c packet.c dispatch.c canohost.c ssh_api.c
SRCS+=compat.c ed25519.c hash.c ge25519.c fe25519.c sc25519.c verify.c
SRCS+=cipher-chachapoly.c chacha.c poly1305.c

SRCS+=	kex.c
SRCS+=	dh.c
SRCS+=	kexdh.c
SRCS+=	kexecdh.c
SRCS+=	kexgex.c
SRCS+=	kexgexc.c
SRCS+=	kexgexs.c
SRCS+=	kexc25519.c
SRCS+=	smult_curve25519_ref.c
SRCS+=	kexgen.c
SRCS+=	kexsntrup4591761x25519.c
SRCS+=	sntrup4591761.c

SRCS+=digest-openssl.c
#SRCS+=digest-libc.c

NOMAN=	1

.if (${OPENSSL:L} == "yes")
CFLAGS+=	-DWITH_OPENSSL
.else
# SSH v.1 requires OpenSSL.
SSH1=		no
.endif

.if (${SSH1:L} == "yes")
CFLAGS+=	-DWITH_SSH1
.endif

# enable warnings
WARNINGS=Yes

DEBUG=-g
CFLAGS+=	-fstack-protector-all
CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wchar-subscripts
CDIAGFLAGS+=	-Wcomment
CDIAGFLAGS+=	-Wformat
CDIAGFLAGS+=	-Wformat-security
CDIAGFLAGS+=	-Wimplicit
CDIAGFLAGS+=	-Winline
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wparentheses
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wreturn-type
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wstrict-aliasing
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wswitch
CDIAGFLAGS+=	-Wtrigraphs
CDIAGFLAGS+=	-Wuninitialized
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wno-unused-parameter
.if ${COMPILER_VERSION:L} != "gcc3"
CDIAGFLAGS+=	-Wold-style-definition
.endif


CFLAGS+=-I${.CURDIR}/${SSHREL}

LDADD+= -lutil -lz
DPADD+= ${LIBUTIL} ${LIBZ}

.if (${OPENSSL:L} == "yes")
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
.endif

.include <bsd.prog.mk>

