#
#	pccardc Makefile
#
# $FreeBSD: src/usr.sbin/pccard/pccardc/Makefile,v 1.7.2.3 1999/08/29 15:44:36 peter Exp $
#
PROG=	pccardc
SRCS=	beep.c dumpcis.c enabler.c pccardc.c pccardmem.c printcis.c \
	rdattr.c rdmap.c rdreg.c readcis.c wrattr.c wrreg.c
MAN8=	pccardc.8

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
