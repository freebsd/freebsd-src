# $FreeBSD$

PROG=		kpasswd
SRCS=		kpasswd.c krb5_err.h heim_err.h k524_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/kpasswd \
		-I${ASN1OBJDIR} \
		-I${.OBJDIR}
LDADD=		-L${KRB5OBJDIR} -lkrb5 -L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBROKEN} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kpasswd
