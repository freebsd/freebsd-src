
PROG=	mkctm
SRCS=	mkctm.c bdiff.c
LDADD= -lmd
CFLAGS= -g -Wall

test:	mkctm
	rm -f tst.out*
	time ./mkctm /usr/src /a/stable/src 2>a | md5 -p > ${.CURDIR}/tst.out 
	ls -l ${.CURDIR}/tst.out
	gzip -9 -v tst.out
	ls -l ${.CURDIR}/tst.out.gz
	# cd /usr/src/release && ctm -c -v -v  ${.CURDIR}/tst.out

test1:	mkctm
	time ./mkctm 2>&1 | more

.include <bsd.prog.mk>
