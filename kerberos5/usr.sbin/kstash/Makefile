# $FreeBSD$

PROG=		kstash
SRCS=		kstash.c hdb_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/kdc \
		-I${ASN1OBJDIR} \
		-I${HDBOBJDIR} \
		-I${.OBJDIR}
.if defined(KRB5_KRB4_COMPAT)
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 -L${KAFS5OBJDIR} -lkafs5 \
		-L${ROKENOBJDIR} -lroken ${_krb4libs} \
		-L${ASN1OBJDIR} -lasn1 -ldes -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBDES} ${LIBCRYPT} ${LIBCOM_ERR}
MAN8=		kstash.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
