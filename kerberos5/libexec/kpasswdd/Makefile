# $FreeBSD$

PROG=	kpasswdd
MAN=	kpasswdd.8
CFLAGS+=-I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/lib/krb5 \
	-I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/lib/hdb \
	-I${.OBJDIR:H:H}/lib/libroken \
	-I${.OBJDIR:H:H}/lib/libkrb5 \
	-I${.OBJDIR:H:H}/lib/libasn1 \
	-I${.OBJDIR:H:H}/lib/libhdb \
	${LDAPCFLAGS}
LIBADD=	kadm5srv hdb krb5 roken vers asn1
DPADD=	${LDAPDPADD}
LDADD=	${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
