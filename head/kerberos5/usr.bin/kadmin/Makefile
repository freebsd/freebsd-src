# $FreeBSD$

PROG=	kadmin
MAN=	kadmin.8

SRCS=	add_enctype.c \
	ank.c \
	check.c \
	cpw.c \
	del.c \
	del_enctype.c \
	dump.c \
	ext.c \
	get.c \
	init.c \
	kadmin.c \
	kadmin-commands.c \
	kadmin-commands.h \
	load.c \
	mod.c \
	pw_quality.c \
	random_password.c \
	rename.c \
	stash.c \
	util.c

CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/lib/sl -I. ${LDAPCFLAGS}
DPADD=	${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} \
	${LIBSL} ${LIBROKEN} ${LIBVERS} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} \
	${LIBREADLINE} ${LIBNCURSES} ${LDAPDPADD}
LDADD=	-lkadm5clnt -lkadm5srv -lhdb -lkrb5 -lhx509 \
	${LIBSL} -lroken ${LIBVERS} -lasn1 \
	-lcrypto -lcrypt -lcom_err \
	-lreadline -lncurses ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

kadmin-commands.c kadmin-commands.h: ${KRB5DIR}/kadmin/kadmin-commands.in ../../tools/slc/slc
	../../tools/slc/slc ${.ALLSRC:M*.in}

../../tools/slc/slc:
	cd ${.CURDIR}/../../tools/slc && ${MAKE}

.PATH: ${KRB5DIR}/kadmin

