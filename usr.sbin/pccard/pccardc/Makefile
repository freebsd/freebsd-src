#
#	pccardc Makefile
#
#	$Id: Makefile,v 1.1 1995/08/24 09:02:45 phk Exp $
#
PROG=	pccardc
NOMAN=	noman
SRCS=	pccardc.c dumpcis.c readcis.c printcis.c enabler.c pccardmem.c \
	rdmap.c rdreg.c wrattr.c wrreg.c 

CFLAGS+=	-I. -I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
