# $FreeBSD: src/lib/libpthread/support/Makefile.inc,v 1.1 2003/05/30 00:21:52 kan Exp $

.PATH:	 ${.CURDIR}/support

SRCS += thr_support.c
SOBJS+= thr_libc.So

thr_libc.So: thr_support.So
	${CC} -nostdlib -L/usr/lib -o ${.TARGET} -Wl,-x,-r ${.ALLSRC} -lc_pic

