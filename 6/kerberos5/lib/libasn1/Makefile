# $FreeBSD$

LIB=	asn1
INCS=	asn1_err.h krb5_asn1.h

SRCS=	asn1_err.c \
	asn1_err.h \
	der_copy.c \
	der_free.c \
	der_get.c \
	der_length.c \
	der_put.c \
	krb5_asn1.h \
	timegm.c \
	${GEN:S/.x$/.c/}

CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken -I.

GEN=	asn1_APOptions.x \
	asn1_AP_REP.x \
	asn1_AP_REQ.x \
	asn1_AS_REP.x \
	asn1_AS_REQ.x \
	asn1_Authenticator.x \
	asn1_AuthorizationData.x \
	asn1_CKSUMTYPE.x \
	asn1_Checksum.x \
	asn1_ChangePasswdDataMS.x \
	asn1_ENCTYPE.x \
	asn1_ETYPE_INFO.x \
	asn1_ETYPE_INFO_ENTRY.x \
	asn1_EncAPRepPart.x \
	asn1_EncASRepPart.x \
	asn1_EncKDCRepPart.x \
	asn1_EncKrbCredPart.x \
	asn1_EncKrbPrivPart.x \
	asn1_EncTGSRepPart.x \
	asn1_EncTicketPart.x \
	asn1_EncryptedData.x \
	asn1_EncryptionKey.x \
	asn1_HostAddress.x \
	asn1_HostAddresses.x \
	asn1_KDCOptions.x \
	asn1_KDC_REP.x \
	asn1_KDC_REQ.x \
	asn1_KDC_REQ_BODY.x \
	asn1_KRB_CRED.x \
	asn1_KRB_ERROR.x \
	asn1_KRB_PRIV.x \
	asn1_KRB_SAFE.x \
	asn1_KRB_SAFE_BODY.x \
	asn1_KerberosTime.x \
	asn1_KrbCredInfo.x \
	asn1_LR_TYPE.x \
	asn1_LastReq.x \
	asn1_MESSAGE_TYPE.x \
	asn1_METHOD_DATA.x \
	asn1_NAME_TYPE.x \
	asn1_PADATA_TYPE.x \
	asn1_PA_DATA.x \
	asn1_PA_ENC_TS_ENC.x \
	asn1_Principal.x \
	asn1_PrincipalName.x \
	asn1_Realm.x \
	asn1_TGS_REP.x \
	asn1_TGS_REQ.x \
	asn1_Ticket.x \
	asn1_TicketFlags.x \
	asn1_TransitedEncoding.x \
	asn1_UNSIGNED.x

CLEANFILES= ${GEN} ${GEN:S/.x$/.c/} krb5_asn1.h asn1_files

.ORDER: ${GEN} krb5_asn1.h
${GEN} krb5_asn1.h: k5.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} krb5_asn1

../../tools/asn1_compile/asn1_compile:
	cd ${.CURDIR}/../../tools/asn1_compile && ${MAKE}

.for I in ${GEN}
${I:R}.c: ${I}
	cat ${.ALLSRC} > ${.TARGET}
.endfor

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/asn1
