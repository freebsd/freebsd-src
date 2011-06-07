# $FreeBSD$

LIB=	hx509
VERSION_MAP= ${KRB5DIR}/lib/hx509/version-script.map

INCS=	hx509-private.h \
	hx509-protos.h \
	hx509.h \
	hx509_err.h

SRCS=	ca.c \
	cert.c \
	cms.c \
	collector.c \
	crypto.c \
	doxygen.c \
	error.c \
	env.c \
	file.c \
	hx509-private.h \
	hx509-protos.h \
	hx509.h \
	hx_locl.h \
	keyset.c \
	ks_dir.c \
	ks_file.c \
	ks_mem.c \
	ks_null.c \
	ks_p11.c \
	ks_p12.c \
	ks_keychain.c \
	lock.c \
	name.c \
	peer.c \
	print.c \
	softp11.c \
	ref/pkcs11.h \
	req.c \
	revoke.c

SRCS+=	hx509_err.c \
	hx509_err.h

SRCS+=	${GEN:S/.x$/.c/}

CFLAGS+=-I${KRB5DIR}/lib/hx509
CFLAGS+=-I${KRB5DIR}/lib/hx509/ref
CFLAGS+=-I${KRB5DIR}/lib/asn1
CFLAGS+=-I${KRB5DIR}/lib/roken -I.

GEN_OCSP= \
	asn1_OCSPBasicOCSPResponse.x \
	asn1_OCSPCertID.x \
	asn1_OCSPCertStatus.x \
	asn1_OCSPInnerRequest.x \
	asn1_OCSPKeyHash.x \
	asn1_OCSPRequest.x \
	asn1_OCSPResponderID.x \
	asn1_OCSPResponse.x \
	asn1_OCSPResponseBytes.x \
	asn1_OCSPResponseData.x \
	asn1_OCSPResponseStatus.x \
	asn1_OCSPSignature.x \
	asn1_OCSPSingleResponse.x \
	asn1_OCSPTBSRequest.x \
	asn1_OCSPVersion.x \
	asn1_id_pkix_ocsp.x \
	asn1_id_pkix_ocsp_basic.x \
	asn1_id_pkix_ocsp_nonce.x

GEN_PKCS10= \
	asn1_CertificationRequestInfo.x \
	asn1_CertificationRequest.x

GEN+=	${GEN_OCSP}
GEN+=	${GEN_PKCS10}

CLEANFILES= ${GEN} ${GEN:S/.x$/.c/} asn1_files

GEN_ASN1=ocsp_asn1.h pkcs10_asn1.h
CLEANFILES+=${GEN_ASN1}
SRCS+=${GEN_ASN1}
INCS+=${GEN_ASN1}

.ORDER: ${GEN} ${GEN_ASN1}

${GEN_OCSP} ocsp_asn1.h: ocsp.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile --preserve-binary=OCSPTBSRequest --preserve-binary=OCSPResponseData ${.ALLSRC:M*.asn1} ocsp_asn1

${GEN_PKCS10} pkcs10_asn1.h: pkcs10.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} pkcs10_asn1

../../tools/asn1_compile/asn1_compile:
	cd ${.CURDIR}/../../tools/asn1_compile && ${MAKE}

.for I in ${GEN}
${I:R}.c: ${I}
	cat ${.ALLSRC} > ${.TARGET}
.endfor

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/hx509 ${KRB5DIR}/lib/asn1
