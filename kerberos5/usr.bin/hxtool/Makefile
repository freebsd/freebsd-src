# $FreeBSD$

PROG=	hxtool
NO_MAN=	1
CFLAGS+=	-I${KRB5DIR}/lib/hx509 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBHX509} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBSL} ${LIBVERS} ${LIBEDIT}
LDADD=	-lhx509 -lroken -lasn1 -lcrypto -lcrypt ${LIBSL} ${LIBVERS} -ledit
SRCS=	hxtool.c hxtool-commands.c hxtool-commands.h

hxtool-commands.h: hxtool-commands.in
	${SLC} ${.ALLSRC:M*.in}

.for ext in c o
hxtool-commands.${ext}: hxtool-commands.h
.endfor

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/hx509
