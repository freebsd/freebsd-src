#	$Id$

.PATH:	${.CURDIR}/../../i386/svr4 ${.CURDIR}/../../svr4
KMOD=	svr4
SRCS=	svr4_sysent.c svr4_sysvec.c opt_compat.h opt_vmpage.h vnode_if.h \
	imgact_svr4.c svr4_signal.c svr4_fcntl.c svr4_misc.c svr4_ioctl.c \
	svr4_stat.c svr4_filio.c svr4_ttold.c svr4_termios.c svr4_stream.c \
	svr4_socket.c svr4_sockio.c svr4_machdep.c svr4_resource.c \
	svr4_ipc.c
OBJS=	svr4_locore.o 
NOMAN=1
MAN8=	svr4.8
CFLAGS+= -DKERNEL

CFLAGS+= -DCOMPAT_SVR4
.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif

EXPORT_SYMS=_svr4_mod
CLEANFILES+= vnode_if.h vnode_if.c svr4_genassym.o svr4_genassym \
	svr4_assym.h opt_compat.h opt_vmpage.h .depend

build-tools: svr4_genassym

svr4_assym.h:	svr4_genassym
	./svr4_genassym > svr4_assym.h

svr4_locore.o:	svr4_locore.s svr4_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE -DKERNEL ${CFLAGS} \
		${.IMPSRC} -o ${.TARGET}

svr4_genassym.o:	svr4_genassym.c svr4.h @ machine
	${CC} -c ${CFLAGS} -UKERNEL ${.IMPSRC}

svr4_genassym:	svr4_genassym.o
	${CC} ${CFLAGS} ${.ALLSRC} -o ${.TARGET}

svr4_sysvec.o: opt_vmpage.h opt_compat.h

opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

opt_vmpage.h:
	touch opt_vmpage.h

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/svr4.sh ${DESTDIR}/usr/bin/svr4

.include <bsd.kmod.mk>
