# $FreeBSD$

.include "../libcrypto/Makefile.inc"

LCRYPTOSSL_SRC=${LCRYPTO_SRC}/../ssl

.PATH:	${LCRYPTOSSL_SRC}

LIB=		ssl
SHLIB_MAJOR=	2

MAINTAINER=	kris

SRCS+=	bio_ssl.c s23_clnt.c s23_lib.c s23_meth.c s23_pkt.c s23_srvr.c \
	s2_clnt.c s2_enc.c s2_lib.c s2_meth.c s2_pkt.c s2_srvr.c \
	s3_both.c s3_clnt.c s3_enc.c s3_lib.c s3_meth.c s3_pkt.c \
	s3_srvr.c ssl_algs.c ssl_asn1.c ssl_cert.c ssl_ciph.c \
	ssl_err.c ssl_err2.c ssl_lib.c ssl_rsa.c ssl_sess.c ssl_stat.c \
	ssl_txt.c t1_clnt.c t1_enc.c t1_lib.c t1_meth.c t1_srvr.o \

HDRS=	ssl.h ssl2.h ssl23.h ssl3.h ssl_locl.h tls1.h

.for h in ${HDRS}
CRYPTO_HDRS+= ${LCRYPTOSSL_SRC}/${h}
.endfor

.for h in ${HDRS}
SSL_HDRS+= ${LCRYPTOSSL_SRC}/${h}
.endfor

beforeinstall:  openssl/opensslconf.h openssl/evp.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${SSL_HDRS} ${DESTDIR}/usr/include/openssl

.include <bsd.lib.mk>
