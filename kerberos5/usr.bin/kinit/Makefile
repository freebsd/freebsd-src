# $FreeBSD$

PROG=		k5init
SRCS=		kinit.c asn1_err.h krb5_err.h heim_err.h \
       		k524_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/kafs \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/kuser \
		-I${ASN1OBJDIR} \
		-I${.OBJDIR}
.if defined(MAKE_KERBEROS4)
_krb4libs=	-L${KAFS5OBJDIR} -lkafs5 -lkrb -lkafs
_krb4deps=	${LIBKAFS5} ${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kuser

k5init.1:	kinit.1
	ln -sf ${.OODATE} ${.TARGET}
