# $FreeBSD$

PROG=	xopenssl
PROGNAME=	openssl

DPADD=	${LIBSSL} ${LIBCRYPTO}
LDADD=	-lssl -lcrypto

NOLINT=		true

.include "../../lib/libcrypto/Makefile.inc"

CFLAGS+= -DMONOLITH -I${.CURDIR}

SRCS+=	app_rand.c apps.c asn1pars.c ca.c ciphers.c crl.c crl2p7.c \
	dgst.c dh.c dhparam.c dsa.c dsaparam.c enc.c engine.c errstr.c \
	gendh.c gendsa.c genrsa.c nseq.c ocsp.c openssl.c passwd.c \
	pkcs12.c pkcs7.c pkcs8.c rand.c req.c rsa.c rsautl.c s_cb.c \
	s_client.c s_server.c s_socket.c s_time.c sess_id.c smime.c \
	speed.c spkac.c verify.c version.c x509.c

MAN1=	CA.pl.1 asn1parse.1 ca.1 ciphers.1 config.1 crl.1 crl2pkcs7.1 \
	dgst.1 dhparam.1 dsa.1 dsaparam.1 enc.1 gendsa.1 genrsa.1 \
	nseq.1 ocsp.1 openssl.1 passwd.1 pkcs12.1 pkcs7.1 pkcs8.1 \
	rand.1 req.1 rsa.1 rsautl.1 s_client.1 s_server.1 sess_id.1 \
	smime.1 speed.1 spkac.1 verify.1 version.1 x509.1

.include <bsd.prog.mk>

.PATH:	${LCRYPTO_SRC}/apps \
	${.CURDIR}/man

mann-update:
	for i in `( cd ${LCRYPTO_DOC}/apps ; ls *.pod )` ; do \
		cp ${LCRYPTO_DOC}/apps/$$i . ;\
		pod2man --section=3 --release="0.9.7" --center="OpenSSL" \
			$$i > ${.CURDIR}/man/$${i%%.pod}.1 ;\
		rm $$i ;\
		echo $${i%%.pod} ;\
	done
