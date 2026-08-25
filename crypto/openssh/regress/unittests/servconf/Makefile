#	$OpenBSD: Makefile,v 1.2 2026/06/14 04:08:06 djm Exp $

PROG=test_servconf
SRCS=tests.c

SRCS+=servconf.c groupaccess.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=sshbuf-io.c atomicio.c sshkey.c authfile.c cipher.c log.c ssh-rsa.c
SRCS+=ssh-ecdsa.c ssh-ed25519.c mac.c umac.c umac128.c hmac.c misc.c
SRCS+=ssherr.c uidswap.c cleanup.c xmalloc.c match.c krl.c fatal.c
SRCS+=addr.c addrmatch.c bitmap.c
SRCS+=cipher-chachapoly.c chacha.c poly1305.c ssh-ecdsa-sk.c ssh-sk.c
SRCS+=ssh-ed25519-sk.c sk-usbhid.c ssh-pkcs11-client.c
SRCS+=utf8.c ssherr-libcrypto.c
SRCS+=kex.c kex-names.c packet.c dispatch.c compat.c canohost.c
SRCS+=digest-openssl.c ed25519-openssl.c
SRCS+=libcrux-mlkem-mldsa.c ssh-mldsa-eddsa.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS}

.include <bsd.regress.mk>
