#	pccardc Makefile
#
# $FreeBSD$

PROG=	pccardc
MAN=	pccardc.8
SRCS=	beep.c dumpcis.c enabler.c pccardc.c pccardmem.c power.c printcis.c \
	rdattr.c rdmap.c rdreg.c readcis.c wrattr.c wrreg.c

CFLAGS+= -I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
