#
# Bmakefile for sgmls
#
# $FreeBSD$
#

PROG=	sgmls

SRCS=	lexrf.c pcbrf.c synrf.c context.c md1.c md2.c pars1.c pars2.c serv.c
SRCS+=	sgml1.c sgml2.c sgmlmsg.c sgmlxtrn.c traceset.c entgen.c sgmlio.c
SRCS+=	xfprintf.c main.c unixproc.c sgmldecl.c version.c strerror.c getopt.c
SRCS+=	lineout.c ambig.c lextaba.c catalog.c

CFLAGS+= -I${.CURDIR}/../libsgmls

.include "../Makefile.inc"
.include <bsd.prog.mk>


