# $FreeBSD$

PROG=		ktutil
SRCS=		\
		add.c				\
		change.c			\
		copy.c				\
		get.c				\
		ktutil.c			\
		list.c				\
		purge.c				\
		remove.c			\
		rename.c \
		kadm5/private.h \
		kadm5/admin.h \
		kadm5/kadm5_err.h \
		krb5_err.h heim_err.h k524_err.h

CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/sl \
		-I${KRB5DIR}/lib/kadm5 \
		-I${KRB5DIR}/admin \
		-I${ASN1OBJDIR} \
		-I${.OBJDIR}
LDADD=		-L${KADM5COBJDIR} -lkadm5clnt \
		-L${KRB5OBJDIR} -lkrb5 \
		-L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		-L${SLOBJDIR} -lsl -L${ASN1OBJDIR} -lasn1 \
		-lcrypto -lcrypt -lcom_err -lreadline -lcurses
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBROKEN} \
		${LIBSL} ${LIBASN1} ${LIBKADM5CLNT} \
		${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LIBREADLINE} \
		${LIBCURSES}
MAN=		ktutil.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/admin

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
