# $FreeBSD$

OPENSSL_SRC= ${.CURDIR}/../../../crypto/openssl/apps
LCRYPTO_SRC= ${.CURDIR}/../../../crypto/openssl/crypto

.PATH:	${OPENSSL_SRC} ${OPENSSL_SRC}/../doc/apps/

PROG=	openssl

MAINTAINER=	kris

LDADD=	-lssl -lcrypto
MAN1=	openssl.1
MLINKS=	openssl.1 ssl.8

CFLAGS+= -DMONOLITH -I${.CURDIR}

WITH_RSA?=	YES
.if ${WITH_RSA} == NO
CFLAGS+=	-DNO_RSA -DNO_SSL2
.endif

SRCS=	app_rand.c apps.c asn1pars.c ca.c ciphers.c crl.c crl2p7.c dgst.c \
	dh.c dhparam.c dsa.c dsaparam.c enc.c errstr.c gendh.c gendsa.c \
	genrsa.c nseq.c openssl.c passwd.c pkcs12.c pkcs7.c pkcs8.c rand.c \
	req.c rsa.c s_cb.c s_client.c s_server.c s_socket.c s_time.c \
	sess_id.c smime.c speed.c spkac.c verify.c version.c x509.c

.include <bsd.prog.mk>

.SUFFIXES: .out .o .c .cc .cpp .cxx .C .m .y .l .s .S .pod

.pod.1:
	pod2man ${.IMPSRC} > ${.TARGET}
