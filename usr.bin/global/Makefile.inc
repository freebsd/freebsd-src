# $FreeBSD: src/usr.bin/global/Makefile.inc,v 1.6.2.1 1999/08/29 15:28:25 peter Exp $

.if !defined(GLOBAL_MAKEFILE_INC_BEEN_HERE)

GLOBAL_MAKEFILE_INC_BEEN_HERE=yes
.if exists(${.OBJDIR}/../lib)
LIBDESTDIR=	${.OBJDIR}/../lib
.else
LIBDESTDIR=	${.CURDIR}/../lib
.endif
DPADD=		${LIBDESTDIR}/libgloutil.a
LDADD=		${LIBDESTDIR}/libgloutil.a
CFLAGS+=	-I${GLOBAL_DIR}/lib -O \
		-Wall -Wwrite-strings -Wmissing-prototypes

.include "../Makefile.inc"

.endif
