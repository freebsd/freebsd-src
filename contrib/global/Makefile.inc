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
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../lib -O \
		-Wall -Wwrite-strings -Wmissing-prototypes

.endif
