# $FreeBSD$

PROG=	kimpersonate
MAN=	kimpersonate.8
CFLAGS+=	-I${KRB5DIR}/lib/hx509 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lkafs5 -lkrb5 -lheimntlm -lroken -lasn1 -lcrypto -lcrypt \
	${LIBVERS}

NO_PIE=	yes

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
