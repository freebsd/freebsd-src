# from BSDI Makefile,v 2.6 1996/04/08 20:06:40 bostic Exp
#
# $FreeBSD$

PROG=	doscmd
SRCS=	AsyncIO.c ParseBuffer.c bios.c callback.c cmos.c config.c cpu.c cwd.c \
	debug.c disktab.c dos.c doscmd.c ems.c emuint.c exe.c i386-pinsn.c \
	int.c int10.c int13.c int14.c int16.c int17.c int1a.c int2f.c intff.c \
	mem.c mouse.c net.c port.c setver.c signal.c timer.c trace.c trap.c \
	tty.c video.c xms.c ${FONTHDRS}
CFLAGS+=	-I. -DDISASSEMBLER
FONTFILES=	cp437-8x8.pcf.gz cp437-8x14.pcf.gz cp437-8x16.pcf.gz
FONTHDRS=	font8x8.h font8x14.h font8x16.h
CLEANFILES=	${FONTFILES} ${FONTHDRS} emsdriv.sys redir.com

XINCDIR=	${DESTDIR}${X11BASE}/include
XLIBDIR=	${DESTDIR}${X11BASE}/lib

EXEGRP:=	${BINGRP}
EXEMODE=	${NOBINMODE}
#BINGRP=		kmem
#BINMODE=	2555

.if !defined(NO_X) && exists(${XINCDIR}/X11/X.h) && exists(${XLIBDIR}/libX11.a)
CFLAGS+=	-I${XINCDIR}
LDADD+=		-L${XLIBDIR} -lX11
DPADD+=		${XLIBDIR}/libX11.a
.else
CFLAGS+=	-DNO_X
.endif

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${EXEGRP} -m ${EXEMODE} \
	    emsdriv.sys redir.com ${DESTDIR}/usr/libdata/doscmd/
	${INSTALL} -o ${BINOWN} -g ${EXEGRP} -m ${SHAREMODE} \
	    ${FONTFILES} ${DESTDIR}/usr/libdata/doscmd/fonts
	cd ${.CURDIR} && \
	    ${INSTALL} -o ${BINOWN} -g ${EXEGRP} -m ${SHAREMODE} \
		fonts.dir ${DESTDIR}/usr/libdata/doscmd/fonts

doscmd: ${FONTFILES} ${FONTHDRS} emsdriv.sys redir.com

cp437-8x8.pcf.gz: cp437-8x8.pcf.gz.uu
	uudecode ${.CURDIR}/cp437-8x8.pcf.gz.uu

cp437-8x14.pcf.gz: cp437-8x14.pcf.gz.uu
	uudecode ${.CURDIR}/cp437-8x14.pcf.gz.uu

cp437-8x16.pcf.gz: cp437-8x16.pcf.gz.uu
	uudecode ${.CURDIR}/cp437-8x16.pcf.gz.uu

emsdriv.sys: emsdriv.sys.uu
	uudecode ${.CURDIR}/emsdriv.sys.uu

font8x8.h: ${.CURDIR}/../../share/syscons/fonts/cp437-8x8.fnt
	uudecode -p ${.ALLSRC} | \
		file2c 'u_int8_t font8x8[] = {' '};' > ${.TARGET}

font8x14.h: ${.CURDIR}/../../share/syscons/fonts/cp437-8x14.fnt
	uudecode -p ${.ALLSRC} | \
		file2c 'u_int8_t font8x14[] = {' '};' > ${.TARGET}

font8x16.h: ${.CURDIR}/../../share/syscons/fonts/cp437-8x16.fnt
	uudecode -p ${.ALLSRC} | \
		file2c 'u_int8_t font8x16[] = {' '};' > ${.TARGET}

redir.com: redir.com.uu
	uudecode ${.CURDIR}/redir.com.uu

# Make sure the library names are defined.  We want to specify the full
# path to the standard crt0.o, and building two binaries in one directory
# breaks the automatic generation of dependencies for binaries.
NEED_LIBNAMES=	yes

.include <bsd.prog.mk>
