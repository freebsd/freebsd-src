#
#	pccardc Makefile
#
#	$Id: Makefile,v 1.2 1996/04/09 23:31:51 nate Exp $
#
PROG=	pccardc
NOMAN=	noman
SRCS=	pccardc.c dumpcis.c readcis.c printcis.c enabler.c pccardmem.c \
	rdmap.c rdreg.c wrattr.c wrreg.c 

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
