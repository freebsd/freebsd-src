#	$OpenBSD: Makefile,v 1.4 2014/12/03 22:16:02 bcook Exp $

PROG=	openssl
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
MAN1=	openssl.1

CFLAGS+= -Wall -Werror
CFLAGS+= -Wformat
CFLAGS+= -Wformat-security
CFLAGS+= -Wimplicit
CFLAGS+= -Wreturn-type
#CFLAGS+= -Wshadow
CFLAGS+= -Wtrigraphs
CFLAGS+= -Wuninitialized
CFLAGS+= -Wunused

CFLAGS+= -DLIBRESSL_INTERNAL

SRCS=	apps.c apps_posix.c asn1pars.c ca.c certhash.c ciphers.c cms.c crl.c \
	crl2p7.c dgst.c dh.c dhparam.c dsa.c dsaparam.c ec.c ecparam.c enc.c \
	engine.c errstr.c gendh.c gendsa.c genpkey.c genrsa.c nseq.c ocsp.c \
	openssl.c passwd.c pkcs12.c pkcs7.c pkcs8.c pkey.c pkeyparam.c \
	pkeyutl.c prime.c rand.c req.c rsa.c rsautl.c s_cb.c s_client.c \
	s_server.c s_socket.c s_time.c sess_id.c smime.c speed.c spkac.c ts.c \
	verify.c version.c x509.c

.include <bsd.prog.mk>
