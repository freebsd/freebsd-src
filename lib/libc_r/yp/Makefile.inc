#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
#	$Id: Makefile.inc,v 1.1 1994/08/07 23:04:53 wollman Exp $

# yp sources
.PATH: ${.CURDIR}/../libc/yp

SRCS+=	xdryp.c yplib.c

