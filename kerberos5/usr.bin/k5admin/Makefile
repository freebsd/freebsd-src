# $FreeBSD$

PROG=		k5admin
SRCS=		\
	ank.c					\
	cpw.c					\
	del.c					\
	del_enctype.c				\
	dump.c					\
	ext.c					\
	get.c					\
	init.c					\
	kadmin.c				\
	load.c					\
	mod.c					\
	rename.c				\
	util.c					\
	random_password.c			\
	kadmin_locl.h				\
	krb5_err.h heim_err.h hdb_err.h		\
	kadm5/private.h				\
	kadm5/admin.h				\
	kadm5/kadm5_err.h

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
.if defined(MAKE_KERBEROS4)
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${KADM5COBJDIR} -lkadm5clnt -L${KADM5SOBJDIR} -lkadm5srv \
		-L${HDBOBJDIR} -lhdb \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err \
		-L${SLOBJDIR} -lsl -lreadline
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} ${_krb4deps} \
		${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBHDB} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} \
		${LIBSL} ${LIBREADLINE}
MAN=		k5admin.8

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

k5admin.8:	kadmin.8
	ln -sf ${.OODATE} ${.TARGET}

CLEANFILES+=	kadm5/private.h kadm5/admin.h kadm5/kadm5_err.h
CLEANDIRS+=	kadm5
