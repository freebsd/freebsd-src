# $FreeBSD$

.PATH: ${.CURDIR}/support

SOBJS+=	thr_libc.So
CLEANFILES+= thr_support.So

thr_libc.So: thr_support.So
	${CC} -nostdlib -o ${.TARGET} -Wl,-r thr_support.So -lc_pic
