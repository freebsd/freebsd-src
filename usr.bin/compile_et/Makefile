# $FreeBSD$

.PATH: ${SRCTOP}/contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l
LIBADD=	roken vers
KRB5DIR=	${SRCTOP}/crypto/heimdal
CFLAGS+=-I. -I${SRCTOP}/contrib/com_err \
	-I${KRB5DIR}/lib/roken \
	-I${.OBJDIR:H:H}/kerberos5/lib/libroken \
	-I${.OBJDIR:H:H}/kerberos5/lib/libasn1

WARNS?=	0

.include <bsd.prog.mk>
