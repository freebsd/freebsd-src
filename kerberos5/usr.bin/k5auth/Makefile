# $FreeBSD$

PROG=		k5auth
SRCS=		kauth.c rkinit.c encdata.c marshall.c roken.h
CFLAGS+=	-I${KRB5DIR}/include \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/appl/kauth \
		-I${.OBJDIR}
.if defined(KRB5_KRB4_COMPAT)
_krb4libs=	-lkrb -lkafs
_krb4deps=	${LIBKRB} ${LIBKAFS}
.endif
LDADD=		-L${KRB5OBJDIR} -lkrb5 -L${ROKENOBJDIR} -lroken ${_krb4libs} \
		-L${ASN1OBJDIR} -lasn1 -lcrypto -lmd -lcrypt -lcom_err
DPADD=		${LIBKRB5} ${LIBROKEN} ${_krb4deps} \
		${LIBASN1} ${LIBCRYPTO} ${LIBMD} ${LIBCRYPT} ${LIBCOM_ERR}
NOMAN=		yes

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/appl/kauth
