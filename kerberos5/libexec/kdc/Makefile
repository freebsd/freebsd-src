# $FreeBSD$

PROG=		kdc
SRCS=		\
	config.c	\
	connect.c	\
	kdc_locl.h	\
	kerberos5.c	\
	log.c		\
	main.c		\
	misc.c

CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/kuser \
		-I${ASN1OBJDIR} \
		-I${HDBOBJDIR} \
		-I${.OBJDIR}
.if defined(MAKE_KERBEROS4)
SRCS+=	524.c kerberos4.c kaserver.c rx.h
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 \
		-L${HDBOBJDIR} -lhdb -L${ROKENOBJDIR} -lroken \
		-L${VERSOBJDIR} -lvers \
		${_krb4libs} \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBKAFS5} ${LIBHDB} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
MAN=		kdc.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
