# $Id: Makefile.inc,v 1.5 1999/01/18 07:38:09 simokawa Exp $

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
