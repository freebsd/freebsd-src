PROG=	truss
SRCS=	main.c setup.c i386-fbsd.c i386-linux.c \
	syscalls.c linux_syscalls.h syscalls.h ioctl.c
CFLAGS+= -I${.CURDIR} -I. -I${.CUDIR}/../../sys
CLEANFILES+=i386l-syscalls.master syscalls.master linux_syscalls.h \
	syscalls.h ioctl.c

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

ioctl.c:	${.CURDIR}/../../usr.bin/kdump/mkioctls
	/bin/sh ${.CURDIR}/../../usr.bin/kdump/mkioctls > ioctl.c

.include <bsd.prog.mk>
