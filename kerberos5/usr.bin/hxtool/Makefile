# $FreeBSD$

PROG=	hxtool
MAN=
CFLAGS+=	-I${KRB5DIR}/lib/hx509 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
LIBADD=	hx509 roken asn1 crypto sl vers edit
SRCS=	hxtool.c hxtool-commands.c hxtool-commands.h

CLEANFILES=	hxtool-commands.h hxtool-commands.c

hxtool-commands.h: hxtool-commands.in
	${SLC} ${.ALLSRC:M*.in}

.for ext in c o
hxtool-commands.${ext}: hxtool-commands.h
.endfor

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/hx509
