# $FreeBSD$

PROG=	openssl

MAINTAINER=	kris

LDADD=	-lssl -lcrypto
NOMAN=	noman 

OPENSSL_SRC= ${.CURDIR}/../../../crypto/openssl/apps

CFLAGS+= -DMONOLITH -DNO_IDEA -I${.CURDIR}

LOCALBASE?=	/usr/local

.if !defined(USA_RESIDENT) || ${USA_RESIDENT} != NO
.if !exists(${LOCALBASE}/lib/librsaref.a)
CFLAGS+= -DNO_RSA -DNO_SSL2
.else
CFLAGS+= -DRSAref
LDADD+=	${LOCALBASE}/lib/librsaref.a
.endif
.endif

SRCS=	apps.c asn1pars.c ca.c ciphers.c crl.c crl2p7.c dgst.c dh.c \
	dsa.c dsaparam.c enc.c errstr.c gendh.c gendsa.c genrsa.c \
	nseq.c openssl.c pkcs12.c pkcs7.c pkcs8.c req.c rsa.c s_cb.c \
	s_client.c s_server.c s_socket.c s_time.c sess_id.c speed.c \
	verify.c version.c x509.c

.PATH:	${OPENSSL_SRC}

.include <bsd.prog.mk>
