# $FreeBSD$

PROG=	kstash

SRCS=	\
	kstash.c		\
	${.OBJDIR}/hdb_err.h

CFLAGS+=-I${KRB5DIR}/include	\
	-I${KRB5DIR}/lib/roken	\
	-I${KRB5DIR}/lib/krb5	\
	-I${KRB5DIR}/lib/asn1	\
	-I${KRB5DIR}/lib/hdb	\
	-I${KRB5DIR}/kdc	\
	-I${ASN1OBJDIR}		\
	-I${HDBOBJDIR}		\
	-I${.OBJDIR}		\
	${LDAPCFLAGS}

LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPLIBS}

MAN=	kstash.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
