# $Id: Makefile.inc,v 1.4 1997/12/17 15:18:48 cwt Exp $

.if !defined(GLOBAL_MAKEFILE_INC_BEEN_HERE)

GLOBAL_MAKEFILE_INC_BEEN_HERE=yes
BINDIR?=	/usr/bin
.if exists(${.OBJDIR}/../lib)
LIBDESTDIR=	${.OBJDIR}/../lib
.else
LIBDESTDIR=	${.CURDIR}/../lib
.endif
LDDESTDIR=	-L${LIBDESTDIR}
LDADD=		-lgloutil
DPADD=		${LIBDESTDIR}/libgloutil.a
CFLAGS+=	-I${GLOBAL_DIR}/lib -O \
		-Wall -Wwrite-strings -Wmissing-prototypes

.endif
