# $FreeBSD$

PROG=	kdc
MAN=	kdc.8

SRCS=	524.c \
	config.c \
	connect.c \
	default_config.c \
	digest.c \
	kaserver.c \
	kerberos4.c \
	kerberos5.c \
	krb5tgs.c \
	kx509.c \
	log.c \
	main.c \
	misc.c \
	pkinit.c \
	process.c \
	set_dbinfo.c \
	windc.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kdc ${LDAPCFLAGS}
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lhx509 -lheimntlm -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
