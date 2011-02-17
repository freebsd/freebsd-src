# $FreeBSD$

PAMDIR=		${.CURDIR}/../../../../contrib/openpam

NO_INSTALLLIB=
NO_PROFILE=

CFLAGS+= -I${PAMDIR}/include -I${.CURDIR}/../../libpam

# This is nasty.
# For the static case, libpam.a depends on the modules.
# For the dynamic case, the modules depend on libpam.so.N
.if defined(_NO_LIBPAM_SO_YET)
NO_PIC=
.else
SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
DPADD+=		${LIBPAM}
LDADD+=		-lpam
.endif

.c.o:
	${CC} ${CFLAGS} -DOPENPAM_STATIC_MODULES -c ${.IMPSRC}

.include "../Makefile.inc"
