# $FreeBSD$

PROG=		k5admind
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
		-I${.OBJDIR}
.if defined(MAKE_KERBEROS4) && \
	(${MAKE_KERBEROS4} == "YES" || ${MAKE_KERBEROS4} == "yes")
SRCS+=		version4.c
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${KADM5COBJDIR} -lkadm5clnt -L${KADM5SOBJDIR} -lkadm5srv \
		-L${HDBOBJDIR} -lhdb \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lmd -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} ${_krb4deps} \
		${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBHDB} \
		${LIBASN1} ${LIBCRYPTO} ${LIBMD} ${LIBCRYPT} ${LIBCOM_ERR}
NOMAN=		yes

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kadmin

kadm5/private.h:	kadm5
	cp ${KRB5DIR}/lib/kadm5/private.h ${.TARGET}

kadm5/admin.h:	kadm5
	cp ${KRB5DIR}/lib/kadm5/admin.h ${.TARGET}

kadm5/kadm5_err.h: kadm5 kadm5_err.h
	cd ${.OBJDIR}/kadm5 && ln -sf ../kadm5_err.h

kadm5:
	mkdir ${.TARGET}

CLEANFILES+=	kadm5/private.h kadm5/admin.h kadm5/kadm5_err.h
CLEANDIRS+=	kadm5
