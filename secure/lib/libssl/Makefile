# $FreeBSD$

LIB=		ssl
SHLIB_MAJOR=	2

NOLINT=		true

MAINTAINER=	kris

SRCS=	bio_ssl.c s23_clnt.c s23_lib.c s23_meth.c s23_pkt.c s23_srvr.c \
	s2_clnt.c s2_enc.c s2_lib.c s2_meth.c s2_pkt.c s2_srvr.c \
	s3_both.c s3_clnt.c s3_enc.c s3_lib.c s3_meth.c s3_pkt.c \
	s3_srvr.c ssl_algs.c ssl_asn1.c ssl_cert.c ssl_ciph.c \
	ssl_err.c ssl_err2.c ssl_lib.c ssl_rsa.c ssl_sess.c ssl_stat.c \
	ssl_txt.c t1_clnt.c t1_enc.c t1_lib.c t1_meth.c t1_srvr.o \

INCS=	ssl.h ssl2.h ssl23.h ssl3.h tls1.h
INCSDIR=${INCLUDEDIR}/openssl
HDRS=	${INCS:S;^;../ssl/;}

.include "../libcrypto/Makefile.inc"

.PATH: ${LCRYPTO_SRC}/../ssl

.include <bsd.lib.mk>
