#	@(#)Makefile	5.11 (Berkeley) 9/6/90

.PATH: ${.CURDIR}/../libc/xdr ${.CURDIR}/../libc
SRCS+=	xdr.c xdr_array.c xdr_float.c xdr_mem.c \
	xdr_rec.c xdr_reference.c xdr_stdio.c
