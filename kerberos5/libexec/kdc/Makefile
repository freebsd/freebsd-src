# $FreeBSD$

PROG=	kdc

SRCS=	\
	config.c		\
	connect.c		\
	kdc_locl.h		\
	kerberos5.c		\
	log.c			\
	main.c			\
	misc.c

CFLAGS+=-I${KRB5DIR}/include	\
	-I${KRB5DIR}/kdc	\
	-I${KRB5DIR}/lib/krb5	\
	-I${KRB5DIR}/lib/asn1	\
	-I${KRB5DIR}/lib/hdb	\
	-I${KRB5DIR}/lib/roken	\
	-I${KRB5DIR}/kuser	\
	-I${ASN1OBJDIR}		\
	-I${HDBOBJDIR}		\
	-I${.OBJDIR}		\
	${LDAPCFLAGS}

LDADD=	-lkrb5 -lhdb -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

DPADD=	${LIBKRB5} ${LIBHDB} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPLIBS}

MAN=	kdc.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
