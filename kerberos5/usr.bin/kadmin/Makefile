# $FreeBSD$

PROG=	kadmin

SRCS=	\
	ank.c				\
	cpw.c				\
	del.c				\
	del_enctype.c			\
	dump.c				\
	ext.c				\
	get.c				\
	init.c				\
	kadmin.c			\
	load.c				\
	mod.c				\
	rename.c			\
	util.c				\
	random_password.c		\
	kadmin_locl.h			\
	${.OBJDIR}/krb5_err.h		\
	${.OBJDIR}/heim_err.h		\
	${.OBJDIR}/hdb_err.h		\
	${.OBJDIR}/kadm5/private.h	\
	${.OBJDIR}/kadm5/admin.h	\
	${.OBJDIR}/kadm5/kadm5_err.h

CFLAGS+=-I${KRB5DIR}/include		\
	-I${KRB5DIR}/lib/roken		\
	-I${KRB5DIR}/lib/krb5		\
	-I${KRB5DIR}/lib/asn1		\
	-I${KRB5DIR}/lib/hdb		\
	-I${KRB5DIR}/lib/sl		\
	-I${KRB5DIR}/lib/kadm5		\
	-I${KRB5DIR}/kadmin		\
	-I${ASN1OBJDIR}			\
	-I${HDBOBJDIR}			\
	-I${.OBJDIR}			\
	${LDAPCFLAGS}

LDADD=	-lkadm5clnt -lkadm5srv -lkrb5 -lroken	\
	${LIBVERS} -lhdb -lasn1			\
	-lcrypto -lcrypt -lcom_err		\
	${LIBSL} -lreadline -lcurses ${LDAPLDADD}

DPADD=	${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBKRB5} ${LIBROKEN}	\
	${LIBVERS} ${LIBHDB} ${LIBASN1}				\
	${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}			\
	${LIBSL} ${LIBREADLINE} ${LIBCURSES} ${LDAPLIBS}

MAN=	kadmin.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kadmin

${.OBJDIR}/kadm5/private.h:	${.OBJDIR}/kadm5
	cat ${KRB5DIR}/lib/kadm5/private.h > ${.TARGET}

${.OBJDIR}/kadm5/admin.h:	${.OBJDIR}/kadm5
	cat ${KRB5DIR}/lib/kadm5/admin.h > ${.TARGET}

${.OBJDIR}/kadm5/kadm5_err.h: ${.OBJDIR}/kadm5 ${.OBJDIR}/kadm5_err.h
	cd ${.OBJDIR}/kadm5 && ln -sf ../kadm5_err.h

${.OBJDIR}/kadm5:
	mkdir ${.TARGET}

CLEANFILES+=	kadm5/private.h kadm5/admin.h kadm5/kadm5_err.h
CLEANDIRS+=	kadm5
