#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
#	$Id: Makefile.inc,v 1.1 1993/11/01 23:56:27 paul Exp $

# yp sources
.PATH: ${.CURDIR}/arch/${MACHINE}/yp ${.CURDIR}/yp

SRCS+=	xdryp.c yplib.c

