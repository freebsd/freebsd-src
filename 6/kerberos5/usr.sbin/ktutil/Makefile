# $FreeBSD$

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c \
	change.c \
	copy.c \
	get.c \
	ktutil.c \
	list.c \
	purge.c \
	remove.c \
	rename.c

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl
DPADD=	${LIBKADM5CLNT} ${LIBKRB5} ${LIBSL} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} \
	${LIBREADLINE} ${LIBNCURSES}
LDADD=	-lkadm5clnt -lkrb5 ${LIBSL} -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err \
	-lreadline -lncurses

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/admin
