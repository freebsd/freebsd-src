
PROG=crunchgen
SRCS=crunchgen.c crunched_skel.c
CFLAGS+=-g -Wall

crunched_skel.c: crunched_main.c
	${.CURDIR}/mkskel.sh ${.CURDIR}/crunched_main.c >crunched_skel.c

.include <bsd.prog.mk>
