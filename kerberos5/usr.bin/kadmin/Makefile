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
	random_password.c

CFLAGS+=-I${KRB5DIR}/lib/roken		\
	-I${KRB5DIR}/lib/krb5		\
	-I${KRB5DIR}/lib/asn1		\
	-I${KRB5DIR}/lib/sl		\
	${LDAPCFLAGS}

LDADD=	-lkadm5clnt -lkadm5srv -lkrb5 -lroken	\
	${LIBVERS} -lhdb -lasn1			\
	-lcrypto -lcrypt -lcom_err		\
	${LIBSL} -lreadline -lncurses ${LDAPLDADD}

LDFLAGS=${LDAPLDFLAGS}

DPADD=	${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBKRB5} ${LIBROKEN}	\
	${LIBVERS} ${LIBHDB} ${LIBASN1}				\
	${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}			\
	${LIBSL} ${LIBREADLINE} ${LIBNCURSES} ${LDAPDPADD}

MAN=	kadmin.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kadmin
