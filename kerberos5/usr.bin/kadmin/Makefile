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

LDADD=	-lkadm5clnt -lkadm5srv -lhdb -lkrb5	\
	${LIBSL} -lroken ${LIBVERS} -lasn1	\
	-lcrypto -lcrypt -lcom_err		\
	-lreadline -lncurses ${LDAPLDADD}

LDFLAGS=${LDAPLDFLAGS}

DPADD=	${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5}	\
	${LIBSL} ${LIBROKEN} ${LIBVERS} ${LIBASN1}		\
	${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}			\
	${LIBREADLINE} ${LIBNCURSES} ${LDAPDPADD}

MAN=	kadmin.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kadmin
