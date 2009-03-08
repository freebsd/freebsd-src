# $FreeBSD$

.PATH: ${BSDSRCTOP}/sbin/geom/misc

LIBDIR = ${STAGEDIR}/${CLASS_DIR}

SHLIB_NAME ?= geom_${CLASS}.so

SRCS += \
	geom_${CLASS}.c \
	subr.c

CFLAGS+= -I${BSDSRCTOP}/sbin/geom

.include "../Buildfile.inc"
