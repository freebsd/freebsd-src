# $FreeBSD$

PROG=	ipropd-master

SRCS=	\
	ipropd_master.c		\
	iprop.h			\
	kadm5_locl.h		\
	${.OBJDIR}/kadm5_err.h	\
	${.OBJDIR}/hdb_err.h

CFLAGS+=-I${KRB5DIR}/include	\
	-I${KRB5DIR}/lib/kadm5	\
	-I${KRB5DIR}/lib/krb5	\
	-I${KRB5DIR}/lib/asn1	\
	-I${KRB5DIR}/lib/hdb	\
	-I${KRB5DIR}/lib/roken	\
	-I${KRB5DIR}/kuser	\
	-I${ASN1OBJDIR}		\
	-I${HDBOBJDIR}		\
	-I${.OBJDIR}		\
	${LDAPCFLAGS}

LDADD=	-lkadm5srv -lkrb5 -lhdb -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

DPADD=	${LIBKADM5SRV} ${LIBKRB5} ${LIBHDB} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPLIBS}

NOMAN=	yes

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/lib/kadm5
