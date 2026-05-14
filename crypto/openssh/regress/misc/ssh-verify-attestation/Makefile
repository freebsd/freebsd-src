#	$OpenBSD: Makefile,v 1.4 2025/11/06 01:33:03 djm Exp $

.include <bsd.own.mk>
.include <bsd.obj.mk>

PROG=	ssh-verify-attestation
NOMAN=

SSHREL=../../../../../usr.bin/ssh
.PATH: ${.CURDIR}/${SSHREL}

SRCS=ssh-verify-attestation.c
# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=sshbuf-io.c atomicio.c sshkey.c authfile.c cipher.c log.c ssh-rsa.c
SRCS+=ssh-ecdsa.c ssh-ed25519.c mac.c umac.c umac128.c hmac.c misc.c
SRCS+=ssherr.c uidswap.c cleanup.c xmalloc.c match.c krl.c fatal.c
SRCS+=addr.c addrmatch.c bitmap.c
SRCS+=cipher-chachapoly.c chacha.c poly1305.c ssh-ecdsa-sk.c ssh-sk.c
SRCS+=ssh-ed25519-sk.c sk-usbhid.c

SRCS+=digest-openssl.c ed25519-openssl.c
#SRCS+=digest-libc.c ed25519.c
SRCS+=utf8.c

OPENSSL?=	yes

.if (${OPENSSL:L} == "yes")
CFLAGS+=	-DWITH_OPENSSL
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
CDIAGFLAGS+=	-Wold-style-definition
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

CFLAGS+=-I${.CURDIR}/${SSHREL}

.if (${OPENSSL:L} == "yes")
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
.endif

LDADD+= -lfido2 -lcbor
DPADD+= ${LIBFIDO2} ${LIBCBOR}

LDADD+= -lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>

