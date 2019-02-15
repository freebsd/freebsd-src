# $FreeBSD$

PROG=	kinit
CFLAGS+=-I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/lib/krb5 \
	-I${KRB5DIR}/lib/kafs \
	-I${KRB5DIR}/lib/ntlm \
	-I${.OBJDIR:H:H}/lib/libroken \
	-I${.OBJDIR:H:H}/lib/libkrb5 \
	-I${.OBJDIR:H:H}/lib/libheimntlm \
	-I${.OBJDIR:H:H}/lib/libasn1
LIBADD=	kafs5 krb5 heimntlm roken crypto vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
