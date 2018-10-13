#	$OpenBSD: Makefile,v 1.5 2017/12/21 00:41:22 djm Exp $

PROG=test_kex
SRCS=tests.c test_kex.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=atomicio.c sshkey.c authfile.c cipher.c log.c ssh-rsa.c ssh-dss.c
SRCS+=ssh-ecdsa.c ssh-ed25519.c mac.c umac.c umac128.c hmac.c misc.c
SRCS+=ssherr.c uidswap.c cleanup.c xmalloc.c match.c krl.c fatal.c
SRCS+=addrmatch.c bitmap.c packet.c dispatch.c canohost.c ssh_api.c
SRCS+=kex.c kexc25519.c kexc25519c.c kexc25519s.c kexdh.c kexdhc.c kexdhs.c
SRCS+=kexecdh.c kexecdhc.c kexecdhs.c kexgex.c kexgexc.c kexgexs.c
SRCS+=dh.c compat.c
SRCS+=ed25519.c hash.c ge25519.c fe25519.c sc25519.c verify.c
SRCS+=cipher-chachapoly.c chacha.c poly1305.c
SRCS+=smult_curve25519_ref.c

SRCS+=digest-openssl.c
#SRCS+=digest-libc.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>

LDADD+=-lz
