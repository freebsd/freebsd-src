# $FreeBSD$

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c \
	change.c \
	copy.c \
	get.c \
	ktutil.c \
	ktutil-commands.c \
	ktutil-commands.h \
	list.c \
	purge.c \
	remove.c \
	rename.c

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKADM5CLNT} ${LIBKRB5} ${LIBHX509} ${LIBSL} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} \
	${LIBREADLINE} ${LIBNCURSES}
LDADD=	-lkadm5clnt -lkrb5 -lhx509 ${LIBSL} -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err \
	-lreadline -lncurses

.include <bsd.prog.mk>

ktutil-commands.c ktutil-commands.h: ${KRB5DIR}/admin/ktutil-commands.in ../../tools/slc/slc
	../../tools/slc/slc ${.ALLSRC:M*.in}

../../tools/slc/slc:
	cd ${.CURDIR}/../../tools/slc && ${MAKE}

.PATH: ${KRB5DIR}/admin
