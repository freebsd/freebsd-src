#	@(#)Makefile	5.11 (Berkeley) 9/6/90

.PATH: ${.CURDIR}/rpc ${.CURDIR}/.
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/rpc
SRCS=	xdr.c xdr_array.c xdr_mem.c \
	xdr_rec.c xdr_reference.c xdr_stdio.c

MAN3=	xdr/xdr.3

UNSUPPORTED+=	xdr_float.c 

