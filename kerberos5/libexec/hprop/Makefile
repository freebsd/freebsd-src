# $FreeBSD$

PROG=		hprop
SRCS=		hprop.c mit_dump.c v4_dump.c hprop.h kadb.h hdb_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/kuser \
		-I${ASN1OBJDIR} \
		-I${HDBOBJDIR} \
		-I${.OBJDIR}
LDADD=		-lkrb5 -lkafs5 -lhdb -lroken ${LIBVERS} \
		-lasn1 -lcrypto -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBHDB} ${LIBROKEN} ${LIBVERS} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
MAN=		hprop.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
