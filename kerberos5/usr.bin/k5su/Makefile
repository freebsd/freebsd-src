# $FreeBSD$

PROG=		k5su
SRCS=		su.c asn1_err.h krb5_err.h heim_err.h k524_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/kafs \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/kuser \
		-I${ASN1OBJDIR} \
		-I${.OBJDIR}
NOMAN=		true
.if defined(MAKE_KERBEROS4)
_krb4libs=	-L${KAFS5OBJDIR} -lkafs -lkrb -lkafs5
_krb4deps=	${LIBKAFS5} ${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lmd -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBCRYPTO} ${LIBMD} ${LIBCRYPT} ${LIBCOM_ERR}
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
.endif
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/appl/su
