#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
#	$Id: Makefile.inc,v 1.1 1996/01/22 00:23:58 julian Exp $

# yp sources
.PATH: ${.CURDIR}/../libc/yp

SRCS+=	xdryp.c yplib.c

