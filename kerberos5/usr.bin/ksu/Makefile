# $FreeBSD$

PROG=		ksu
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
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
.endif
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/appl/su
