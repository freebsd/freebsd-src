#
#	Makefile pccardc
#
#	$Id$
#
.PATH:	${.CURDIR}/../pccardd
PROG=	pccardc
SRCS=	pccardc.c \
	dumpcis.c readcis.c printcis.c \
	enabler.c \
	pccardmem.c \
	rdmap.c \
	rdreg.c \
	wrattr.c \
	wrreg.c 

CFLAGS+=	-I. -I${.CURDIR}/../pccardd

.include <bsd.prog.mk>

##
##	Makefile for PCMCIA card programs.
##
#
#PROGS = rdmap rdreg wrreg pccardmem wrattr enabler
#
#enabler: enabler.c
#	cc $(CFLAGS) -o enabler enabler.c
#
#rdmap: rdmap.c
#	cc $(CFLAGS) -o rdmap rdmap.c
#
#rdreg: rdreg.c
#	cc $(CFLAGS) -o rdreg rdreg.c
#
#wrreg: wrreg.c
#	cc $(CFLAGS) -o wrreg wrreg.c
