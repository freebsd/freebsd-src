# from BSDI Makefile,v 2.6 1996/04/08 20:06:40 bostic Exp
#
# $Id: Makefile,v 1.13.2.1 1999/01/22 18:40:55 jdp Exp $

PROG=	doscmd
SRCS=	AsyncIO.c ParseBuffer.c bios.c callback.c cpu.c dos.c cmos.c config.c \
	cwd.c debug.c disktab.c doscmd.c ems.c emuint.c exe.c i386-pinsn.c \
	int.c int10.c int13.c int14.c int16.c int17.c int1a.c int2f.c intff.c \
	mem.c mouse.c net.c port.c setver.c signal.c timer.c trace.c trap.c \
	tty.c xms.c

.if ${OBJFORMAT} == "aout"
CLEANFILES=     doscmd.kernel crt0.o doscmd_loader.o redir.com emsdriv.sys
.else
CLEANFILES=     redir.com emsdriv.sys
.endif

BINGRP=	kmem
EXEGRP=	bin
BINMODE=555
# Use the following mode if you're less concerned about security.
#BINMODE=2555
EXEMODE=444

.if exists(${X11BASE}/include) && exists(${X11BASE}/lib/libX11.a)
CFLAGS+= -I. -I${X11BASE}/include -DDISASSEMBLER
LDADD= -L${X11BASE}/lib -lX11
DPADD= ${X11BASE}/lib/libX11.a
.else
CFLAGS+= -I. -DDISASSEMBLER -DNO_X
.endif
.if ${OBJFORMAT} == "aout"
LDADD+= -L${DESTDIR}${LIBDIR} -lgcc -lc
DPADD+= ${LIBGCC} ${LIBC}
.endif

beforeinstall:
.if ${OBJFORMAT} == "aout"
	${INSTALL} ${COPY} -o ${BINOWN} -g ${EXEGRP} -m ${EXEMODE} \
	    doscmd.kernel ${DESTDIR}/usr/libexec/
.endif
	${INSTALL} ${COPY} -o ${BINOWN} -g ${EXEGRP} -m ${EXEMODE} \
	    emsdriv.sys redir.com ${DESTDIR}/usr/libdata/doscmd/

.if ${OBJFORMAT} == "aout"
doscmd: ${LIBCRT0} doscmd_loader.o ${LIBGCC} ${LIBC}
	ld -e start -dc -dp ${LDFLAGS:M-static:S/-static/-Bstatic/} \
	    -o doscmd ${LIBCRT0} doscmd_loader.o -L${DESTDIR}${LIBDIR} -lgcc -lc

# Bogusly generated dependency to get doscmd_loader.c looked at by mkdep.
.depend: doscmd_loader.c

# Bogus dependencies to get more than one binary created by `make all'.
doscmd: doscmd.kernel emsdriv.sys redir.com
.else
doscmd: emsdriv.sys redir.com
.endif

redir.com: redir.com.uu
	uudecode ${.CURDIR}/redir.com.uu

emsdriv.sys: emsdriv.sys.uu
	uudecode ${.CURDIR}/emsdriv.sys.uu

# Make sure the library names are defined.  We want to specify the full
# path to the standard crt0.o, and building two binaries in one directory
# breaks the automatic generation of dependencies for binaries.
NEED_LIBNAMES=	yes

.include <bsd.prog.mk>

.if ${OBJFORMAT} == "aout"
# This must be after bsd.prog.mk is included so that ${OBJS} in the
# dependency gets expanded.
doscmd.kernel: crt0.o ${OBJS} ${DPADD}
	ld -N -Bstatic -T 110000 -o doscmd.kernel ${LDFLAGS} \
	    crt0.o ${OBJS} ${LDADD}
.endif
