# $FreeBSD$

PROG=		kpasswdd
SRCS=		kpasswdd.c krb5_err.h heim_err.h hdb_err.h \
		kadm5/admin.h kadm5/private.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/kadm5 \
		-I${KRB5DIR}/kpasswd \
		-I${ASN1OBJDIR} \
		-I${HDBOBJDIR} \
		-I${.OBJDIR} \
		${LDAPCFLAGS}
LDADD=		-L${KRB5OBJDIR} -lkrb5 -L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		-L${KADM5SOBJDIR} -lkadm5srv -L${HDBOBJDIR} -lhdb \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
DPADD=		${LIBKRB5} ${LIBROKEN} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPLIBS}
MAN=		kpasswdd.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kpasswd

kadm5/admin.h:	kadm5
	cat ${KRB5DIR}/lib/kadm5/admin.h > ${.TARGET}

kadm5/private.h: kadm5
	cat ${KRB5DIR}/lib/kadm5/private.h > ${.TARGET}

kadm5:
	mkdir ${.TARGET}

CLEANFILES+=	kadm5/admin.h kadm5/private.h
CLEANDIRS+=	kadm5
