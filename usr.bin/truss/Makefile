PROG=	truss
SRCS=	main.c setup.c i386-fbsd.c i386-linux.c \
	syscalls.c linux_syscalls.h syscalls.h
CFLAGS+= -I${.CURDIR} -I.
CLEANFILES+=i386l-syscalls.master syscalls.master linux_syscalls.h syscalls.h

.SUFFIXES: .master

i386l-syscalls.master:	${.CURDIR}/../../sys/i386/linux/syscalls.master
	cp ${.ALLSRC} i386l-syscalls.master

linux_syscalls.h:	i386l-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh i386l-syscalls.master \
		${.CURDIR}/i386linux.conf

syscalls.master:	${.CURDIR}/../../sys/kern/syscalls.master
	cp ${.ALLSRC} syscalls.master

syscalls.h:	syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh syscalls.master \
		${.CURDIR}/i386.conf

.include <bsd.prog.mk>
