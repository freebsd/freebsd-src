# $FreeBSD$

OPENSSL_SRC= ${.CURDIR}/../../../crypto/openssl/apps
LCRYPTO_SRC= ${.CURDIR}/../../../crypto/openssl/crypto

.PATH:	${OPENSSL_SRC} ${.CURDIR}/../../lib/libcrypto/man

PROG=	openssl

DPADD=	${LIBSSL} ${LIBCRYPTO}
LDADD=	-lssl -lcrypto
MLINKS=	openssl.1 ssl.8

CFLAGS+= -DMONOLITH -I${.CURDIR}

WITH_RSA?=	YES
.if ${WITH_RSA} == NO
CFLAGS+=	-DNO_RSA -DNO_SSL2
.endif

SRCS=	app_rand.c apps.c asn1pars.c ca.c ciphers.c crl.c crl2p7.c \
	dgst.c dh.c dhparam.c dsa.c dsaparam.c enc.c errstr.c gendh.c \
	gendsa.c genrsa.c nseq.c openssl.c passwd.c pkcs12.c pkcs7.c \
	pkcs8.c rand.c req.c rsa.c rsautl.c s_cb.c s_client.c \
	s_server.c s_socket.c s_time.c sess_id.c smime.c speed.c \
	spkac.c verify.c version.c x509.o

.include <bsd.prog.mk>
