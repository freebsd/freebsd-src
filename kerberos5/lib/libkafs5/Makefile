# $FreeBSD$

LIB=	kafs5
CFLAGS+=-I${KRB5DIR}/lib/kafs \
	-I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/lib/krb5 \
	-I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/include \
	-I${KRB5OBJDIR} \
	-I${ASN1OBJDIR}
SRCS=	afssys.c afskrb5.c common.c

.include <bsd.lib.mk>

.PATH:		${KRB5DIR}/lib/kafs
