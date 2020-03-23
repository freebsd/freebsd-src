# $FreeBSD$

KYUA_LIBDIR:=	${.PARSEDIR}

.include "${KYUA_LIBDIR}/Makefile.kyua"

LIB_CXX=        kyua_${KYUA_LIB}
INTERNALLIB=

.PATH: ${KYUA_SRCDIR}/${KYUA_LIB}
CFLAGS+=	-I${KYUA_LIBDIR} \
		-I${KYUA_SRCDIR} \
		-I${SRCTOP}/contrib/lutok/include

CFLAGS+=	-DHAVE_CONFIG_H

# kyua uses auto_ptr
CFLAGS+=        -Wno-deprecated-declarations

.include "${.PARSEDIR}/../Makefile.inc"
