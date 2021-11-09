.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpatls
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/tls

SRCS=	tlsv1_server.c

.if defined(TLS_FUNCS)
.if defined(CONFIG_INTERNAL_TLS)
SRCS+=	asn1.c \
	bignum.c \
	pkcs1.c \
	pkcs5.c \
	pkcs8.c \
	rsa.c \
	tlsv1_client.c \
	tlsv1_client_ocsp.c \
	tlsv1_client_read.c \
	tlsv1_client_write.c \
	tlsv1_common.c \
	tlsv1_cred.c \
	tlsv1_record.c \
	tlsv1_server_read.c \
	tlsv1_server_write.c \
	x509v3.c

CFLAGS+=-DCONFIG_INTERNAL_LIBTOMMATH \
	-DCONFIG_CRYPTO_INTERNAL
.else
CFLAGS+=-DCONFIG_TLSV11 \
	-DCONFIG_TLSV12
.endif
.endif

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
