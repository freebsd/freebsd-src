#$FreeBSD$

LIB=	kdc
PRIVATELIB=	true
LDFLAGS=	-Wl,--no-undefined
VERSION_MAP= ${KRB5DIR}/kdc/version-script.map
LIBADD=	roken hdb hx509 krb5 heimntlm asn1 crypto

INCS=	kdc.h \
	kdc-protos.h

KRB5INCS=	windc_plugin.h
KRB5INCSDIR=	${INCLUDEDIR}/krb5
INCSGROUPS=	INCS KRB5INCS

SRCS=	\
	default_config.c \
	set_dbinfo.c \
	digest.c \
	kerberos5.c \
	krb5tgs.c \
	pkinit.c \
	log.c \
	misc.c \
	kx509.c \
	process.c \
	windc.c

CFLAGS+=	-I${KRB5DIR}/lib/roken -I${.OBJDIR:H}/libroken \
		-I${KRB5DIR}/lib/asn1 -I${.OBJDIR:H}/libasn1 \
		-I${KRB5DIR}/lib/krb5 -I${.OBJDIR:H}/libkrb5 \
		-I${KRB5DIR}/lib/wind -I${.OBJDIR:H}/libwind \
		-I${KRB5DIR}/lib/hx509 -I${.OBJDIR:H}/libhx509 \
		-I${KRB5DIR}/lib/hdb -I${.OBJDIR:H}/libhdb \
		-I${KRB5DIR}/lib/ntlm -I${.OBJDIR:H}/libheimntlm \
		-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/base

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/kdc
