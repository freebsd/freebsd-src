# $FreeBSD$

PROG =		testprg
NOMAN =		no
NOOBJ =		youbet
CFLAGS +=	-g -W -Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS +=	-Wpointer-arith -static -I/usr/src/sys -I../..
CFLAGS +=	-DKERNELSIM -pthread
LDADD +=	-L../../GLib -lG
LDADD +=	-L/usr/local/lib -lexpat
DPADD +=	../../GLib/libG.a
CLEANFILES +=	_* *.core
WARNS=		2

foo:
	echo ${SRCS}
	echo ${OBJS}

ttest:	${PROG}
	./${PROG} -t 2>&1 | tee _test

tbtest:	${PROG}
	./${PROG} -t -b 2>&1 | tee _test

test:	${PROG}
	./${PROG}
.if exists(ref.conf)
	../../ConfCmp/ConfCmp _1.conf ref.conf
.endif
	echo "Passed ${.CURDIR}"

mkref:
	mv _1.conf ref.conf

gdb:
	gdb ${PROG} ${PROG}.core
