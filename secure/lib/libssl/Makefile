# $FreeBSD$

.include "../libcrypto/Makefile.inc"

LCRYPTOSSL_SRC=${LCRYPTO_SRC}/../ssl

.PATH:	${LCRYPTOSSL_SRC}

LIB=		ssl
SHLIB_MAJOR=	1

MAINTAINER=	kris

SRCS+=	bio_ssl.c s23_clnt.c s23_lib.c s23_meth.c s23_pkt.c s23_srvr.c \
	s2_clnt.c s2_enc.c s2_lib.c s2_meth.c s2_pkt.c s2_srvr.c s3_both.c \
	s3_clnt.c s3_enc.c s3_lib.c s3_meth.c s3_pkt.c s3_srvr.c ssl_algs.c \
	ssl_asn1.c ssl_cert.c ssl_ciph.c ssl_err.c ssl_err2.c ssl_lib.c \
	ssl_rsa.c ssl_sess.c ssl_stat.c ssl_txt.c t1_clnt.c t1_enc.c \
	t1_lib.c t1_meth.c t1_srvr.c

HDRS=	ssl.h ssl2.h ssl23.h ssl3.h ssl_locl.h tls1.h

CRYPTOHDRS= asn1/asn1.h asn1/asn1_mac.h bio/bio.h bf/blowfish.h bn/bn.h \
	buffer/buffer.h cast/cast.h comp/comp.h conf/conf.h crypto.h \
	des/des.h dh/dh.h dsa/dsa.h ../e_os.h ../e_os2.h ebcdic.h \
	err/err.h evp/evp.h hmac/hmac.h lhash/lhash.h md2/md2.h \
	md5/md5.h mdc2/mdc2.h objects/objects.h opensslv.h pem/pem.h \
	pem/pem2.h pkcs12/pkcs12.h pkcs7/pkcs7.h rand/rand.h rc2/rc2.h \
	rc4/rc4.h rc5/rc5.h ripemd/ripemd.h rsa/rsa.h stack/safestack.h \
	sha/sha.h stack/stack.h tmdiff.h txt_db/txt_db.h x509/x509.h \
	x509/x509_vfy.h x509v3/x509v3.h

.for h in ${HDRS}
CRYPTO_HDRS+= ${LCRYPTOSSL_SRC}/${h}
.endfor

.for h in ${CRYPTOHDRS}
EXTRA_HDRS+= ${LCRYPTO_SRC}/${h}
.endfor

beforeinstall:  openssl/opensslconf.h
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${CRYPTO_HDRS} openssl/opensslconf.h \
	    ${DESTDIR}/usr/include/openssl

.include <bsd.lib.mk>
