# $FreeBSD$

PROG=		ipropd-master
SRCS=		ipropd_master.c iprop.h kadm5_locl.h kadm5_err.h hdb_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/kadm5 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/kuser \
		-I${ASN1OBJDIR} \
		-I${HDBOBJDIR} \
		-I${.OBJDIR}
.if defined(MAKE_KERBEROS4)
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${HDBOBJDIR} -lhdb -L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${KADM5COBJDIR} -lkadm5clnt -L${KADM5SOBJDIR} -lkadm5srv \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lmd -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBHDB} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBCRYPTO} ${LIBMD} ${LIBCRYPT} ${LIBCOM_ERR}
NOMAN=		yes

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/lib/kadm5
