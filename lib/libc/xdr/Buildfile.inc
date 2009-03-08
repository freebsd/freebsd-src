# $FreeBSD$

.PATH: ${.CURDIR}/xdr

SRCS += \
	xdr.c \
	xdr_array.c \
	xdr_float.c \
	xdr_mem.c \
	xdr_rec.c \
	xdr_reference.c \
	xdr_stdio.c

SYM_MAPS += ${.CURDIR}/xdr/Symbol.map
