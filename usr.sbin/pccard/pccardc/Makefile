#
#	pccardc Makefile
#
#	$Id: Makefile,v 1.7 1998/02/26 14:36:00 hosokawa Exp $
#
PROG=	pccardc
NOMAN=	noman
SRCS=	beep.c dumpcis.c enabler.c pccardc.c pccardmem.c printcis.c \
	rdattr.c rdmap.c rdreg.c readcis.c wrattr.c wrreg.c

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
