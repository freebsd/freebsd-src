#
#	pccardc Makefile
#
#	$Id: Makefile,v 1.8 1999/02/13 11:32:01 kuriyama Exp $
#
PROG=	pccardc
SRCS=	beep.c dumpcis.c enabler.c pccardc.c pccardmem.c printcis.c \
	rdattr.c rdmap.c rdreg.c readcis.c wrattr.c wrreg.c
MAN8=	pccardc.8

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
