#
#	pccardc Makefile
#
#	$Id: Makefile,v 1.3.2.1 1997/10/26 04:13:38 nate Exp $
#
PROG=	pccardc
NOMAN=	noman
SRCS=	dumpcis.c enabler.c pccardc.c pccardmem.c printcis.c \
	rdattr.c rdmap.c rdreg.c readcis.c wrattr.c wrreg.c

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
