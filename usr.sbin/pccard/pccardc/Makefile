#
#	pccardc Makefile
#
#	$Id: Makefile,v 1.6 1997/02/22 16:08:35 peter Exp $
#
PROG=	pccardc
NOMAN=	noman
SRCS=	dumpcis.c enabler.c pccardc.c pccardmem.c printcis.c \
	rdmap.c rdreg.c readcis.c wrattr.c wrreg.c

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
