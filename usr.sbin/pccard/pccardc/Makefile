#
#	pccardc Makefile
#
#	$Id: Makefile,v 1.3 1996/09/04 22:25:28 bde Exp $
#
PROG=	pccardc
NOMAN=	noman
SRCS=	pccardc.c dumpcis.c readcis.c printcis.c enabler.c pccardmem.c \
	rdmap.c rdreg.c wrattr.c wrreg.c 

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
