# $FreeBSD$

.include "../libcrypto/Makefile.inc"

.PATH:	${LCRYPTO_SRC}/../rsaref

MAINTAINER=	kris

LIB=		rsaUSA
SHLIB_MAJOR=	1

CFLAGS+=	-I${.OBJDIR}

LDADD+=		-L${.OBJDIR}/../libcrypto -lcrypto

# rsaref
SRCS+=	rsar_err.c rsaref.c rsaref_stubs.c

HDRS=	asn1/asn1.h asn1/asn1_mac.h bio/bio.h bf/blowfish.h bn/bn.h \
	buffer/buffer.h cast/cast.h comp/comp.h conf/conf.h crypto.h \
	des/des.h dh/dh.h dsa/dsa.h ../e_os.h ../e_os2.h ebcdic.h \
	err/err.h evp/evp.h hmac/hmac.h lhash/lhash.h md2/md2.h \
	md5/md5.h mdc2/mdc2.h objects/objects.h opensslv.h pem/pem.h \
	pem/pem2.h pkcs12/pkcs12.h pkcs7/pkcs7.h rand/rand.h rc2/rc2.h \
	rc4/rc4.h rc5/rc5.h ripemd/ripemd.h rsa/rsa.h ../rsaref/rsaref.h \
	stack/safestack.h sha/sha.h stack/stack.h tmdiff.h txt_db/txt_db.h \
	x509/x509.h x509/x509_vfy.h x509v3/x509v3.h

.for h in ${HDRS}
CRYPTO_HDRS+= ${LCRYPTO_SRC}/${h}
.endfor

.include <bsd.lib.mk>
