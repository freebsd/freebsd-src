#
#	pccardc Makefile
#
#	$FreeBSD$
#
PROG=	pccardc
NOMAN=	noman
SRCS=	dumpcis.c enabler.c pccardc.c pccardmem.c printcis.c \
	rdmap.c rdreg.c readcis.c wrattr.c wrreg.c

CFLAGS+=	-I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardd
