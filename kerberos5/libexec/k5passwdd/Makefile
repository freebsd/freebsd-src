# $FreeBSD$

PROG=		k5passwdd
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
.if defined(MAKE_KERBEROS4)
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 -L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${KADM5SOBJDIR} -lkadm5srv -L${HDBOBJDIR} -lhdb \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
DPADD=		${LIBKRB5} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPLIBS}
MAN=		kpasswdd.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kpasswd

kadm5/admin.h:	kadm5
	cp ${KRB5DIR}/lib/kadm5/admin.h ${.TARGET}

kadm5/private.h: kadm5
	cp ${KRB5DIR}/lib/kadm5/private.h ${.TARGET}

kadm5:
	mkdir ${.TARGET}

CLEANFILES+=	kadm5/admin.h kadm5/private.h
CLEANDIRS+=	kadm5
