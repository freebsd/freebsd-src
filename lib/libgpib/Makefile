# $FreeBSD: src/lib/libgpib/Makefile,v 1.7.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

LIB=		gpib
SHLIB_MAJOR=	3
INCS=		gpib.h
INCSDIR=	${INCLUDEDIR}/gpib
SRCS=		ibfoo.c
WARNS?=		6

MAN=		gpib.3

# MLINKS are only provided for functions that are actually
# implemented; update this if missing pieces have been filled in.
MLINKS+=	gpib.3 ibclr.3
MLINKS+=	gpib.3 ibdev.3
MLINKS+=	gpib.3 ibdma.3
MLINKS+=	gpib.3 ibeos.3
MLINKS+=	gpib.3 ibeot.3
MLINKS+=	gpib.3 ibloc.3
MLINKS+=	gpib.3 ibonl.3
MLINKS+=	gpib.3 ibpad.3
MLINKS+=	gpib.3 ibrd.3
MLINKS+=	gpib.3 ibsad.3
MLINKS+=	gpib.3 ibsic.3
MLINKS+=	gpib.3 ibtmo.3
MLINKS+=	gpib.3 ibtrg.3
MLINKS+=	gpib.3 ibwrt.3

.include <bsd.lib.mk>
