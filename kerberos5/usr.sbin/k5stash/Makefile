# $FreeBSD$

PROG=		k5stash
SRCS=		kstash.c hdb_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/kdc \
		-I${ASN1OBJDIR} \
		-I${HDBOBJDIR} \
		-I${.OBJDIR} \
		${LDAPCFLAGS}
.if defined(MAKE_KERBEROS4)
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${HDBOBJDIR} -lhdb \
		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lmd -lcrypt -lcom_err \
		${LDAPLDADD}
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBCRYPTO} ${LIBMD} ${LIBCRYPT} ${LIBCOM_ERR} \
		${LDAPLIBS}
MAN=		kstash.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
