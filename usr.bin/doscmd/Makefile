# from BSDI Makefile,v 2.6 1996/04/08 20:06:40 bostic Exp
#
# $Id: Makefile,v 1.6 1997/08/18 18:48:33 jlemon Exp $

PROG=	doscmd
MAN1=   doscmd.1
SRCS=	AsyncIO.c ParseBuffer.c bios.c callback.c cpu.c dos.c cmos.c config.c \
	cwd.c debug.c disktab.c doscmd.c exe.c i386-pinsn.c int.c int10.c \
	int13.c int14.c int16.c int17.c int1a.c int2f.c intff.c mem.c mouse.c \
	net.c port.c setver.c signal.c timer.c trace.c trap.c tty.c xms.c

CLEANFILES=     doscmd.kernel crt0.o doscmd_loader.o instbsdi.exe

BINGRP=	kmem
EXEGRP=	bin
BINMODE=2555
EXEMODE=444

.if exists(${X11BASE}/include) && exists(${X11BASE}/lib/libX11.a)
CFLAGS+= -I. -I${X11BASE}/include -DDISASSEMBLER
LDFLAGS+= -L${X11BASE}/lib
LDADD+=	-lX11 -lgcc -lc
DPADD+= ${X11BASE}/lib/libX11.a ${LIBC}
.else
CFLAGS+= -I. -DDISASSEMBLER -DNO_X
LDADD+= -lgcc -lc
DPADD+= ${LIBC}
.endif

afterinstall:
	install ${COPY} -o ${BINOWN} -g ${EXEGRP} -m ${EXEMODE} \
		doscmd.kernel ${DESTDIR}/usr/libexec/doscmd.kernel
	install -c -o ${BINOWN} -g ${EXEGRP} -m ${EXEMODE} \
		instbsdi.exe ${DESTDIR}/usr/libdata/doscmd/

doscmd: doscmd.kernel ${LIBCRT0} doscmd_loader.o instbsdi.exe
	ld -e start -dc -dp -o doscmd /usr/lib/crt0.o doscmd_loader.o -lgcc -lc

instbsdi.exe:	instbsdi.exe.uu
	uudecode        ${.CURDIR}/instbsdi.exe.uu

.include <bsd.prog.mk>

.depend: doscmd_loader.c

doscmd.kernel: crt0.o ${OBJS}
	ld -N -Bstatic -T 110000 -o doscmd.kernel ${LDFLAGS} \
		crt0.o ${OBJS} ${LDADD}
