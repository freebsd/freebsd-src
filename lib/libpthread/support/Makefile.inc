# $FreeBSD$

.PATH:	 ${.CURDIR}/support

SRCS += thr_support.c
SOBJS+= thr_libc.So

thr_libc.So: thr_support.So
	${CC} -nostdlib -L/usr/lib -o ${.TARGET} -Wl,-x,-r ${.ALLSRC} -lc_pic

