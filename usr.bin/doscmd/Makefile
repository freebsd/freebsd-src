# from BSDI Makefile,v 2.6 1996/04/08 20:06:40 bostic Exp
#
# $Id: Makefile,v 1.4 1997/08/12 16:08:02 ache Exp $

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

################################################################################
# For FreeBSD
# (Note ./machine/ include not required once new kernel headers installed)
CFLAGS+=-I. -I/usr/X11R6/include -DDISASSEMBLER
LDFLAGS+=-L/usr/X11R6/lib
LDADD+=	-lX11 -lgcc -lc -lgcc
DPADD+=/usr/X11R6/lib/libX11.a ${LIBC}

# For FreeBSD, no X
#CFLAGS+=-I/usr/X11R6/include -I./machine/ -DDISASSEMBLER -DNO_X
#LDADD+= -lgcc -lc -lgcc
#DPADD+=${LIBC}

################################################################################
# For NetBSD
#CFLAGS+=-I/usr/X11/include -DDISASSEMBLER -g
#LDFLAGS+=-L/usr/X11/lib
#LDADD+=	-lX11 -li386 -lgcc -lc -lgcc
#DPADD+=	/usr/X11/lib/libX11.a ${LIBC} ${LIBGCC}

afterinstall:
	install ${COPY} -o ${BINOWN} -g ${EXEGRP} -m ${EXEMODE} \
		doscmd.kernel ${DESTDIR}/usr/libexec/doscmd.kernel
	install -c -o ${BINOWN} -g ${EXEGRP} -m ${EXEMODE} \
		instbsdi.exe ${DESTDIR}/usr/libdata/doscmd/

doscmd: doscmd.kernel ${LIBCRT0} doscmd_loader.o instbsdi.exe
# for FreeBSD
	ld -e start -dc -dp -o doscmd ${LDFLAGS} /usr/lib/crt0.o doscmd_loader.o ${LDADD}
# for NetBSD
#	ld -N -Bstatic -o doscmd ${LDFLAGS} ${LIBCRT0} doscmd_loader.o ${LDADD}

instbsdi.exe:	instbsdi.exe.uu
	uudecode        ${.CURDIR}/instbsdi.exe.uu

.include <bsd.prog.mk>

.depend: doscmd_loader.c

# NetBSD/FreeBSD
doscmd.kernel: crt0.o ${OBJS}
	ld -N -Bstatic -T 110000 -o doscmd.kernel ${LDFLAGS} crt0.o ${OBJS} ${LDADD}
