# $FreeBSD$

PROG=		kadmind
SRCS=		kadmind.c server.c kadm_conn.c \
		kadm5/private.h kadm5/admin.h kadm5/kadm5_err.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/lib/sl \
		-I${KRB5DIR}/lib/kadm5 \
		-I${KRB5DIR}/kadmin \
		-I${ASN1OBJDIR} \
		-I${HDBOBJDIR} \
		-I${.OBJDIR} \
		${LDAPCFLAGS}
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		-L${KADM5SOBJDIR} -lkadm5srv \
		-L${HDBOBJDIR} -lhdb \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} \
		${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBHDB} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPLIBS}
NOMAN=		yes

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kadmin

kadm5/private.h:	kadm5
	cat ${KRB5DIR}/lib/kadm5/private.h > ${.TARGET}

kadm5/admin.h:	kadm5
	cat ${KRB5DIR}/lib/kadm5/admin.h > ${.TARGET}

kadm5/kadm5_err.h: kadm5 kadm5_err.h
	cd ${.OBJDIR}/kadm5 && ln -sf ../kadm5_err.h

kadm5:
	mkdir ${.TARGET}

CLEANFILES+=	kadm5/private.h kadm5/admin.h kadm5/kadm5_err.h
CLEANDIRS+=	kadm5
