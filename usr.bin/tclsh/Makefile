# $Id: Makefile,v 1.4 1997/08/18 06:08:56 andreas Exp $

PROG=   tclsh
SRCS=   tclAppInit.c

BINDIR= /usr/bin

.PATH: ${DESTDIR}/usr/libdata/tcl

MAN1=   tclsh.1

CLEANFILES=     ${MAN1}

tclsh.1:        ${.CURDIR}/../../contrib/tcl/doc/tclsh.1
	sed '/\.so *man.macros/s;.*;.so /usr/share/tmac/tcl.macros;' < $> > $@

CFLAGS+= -I${DESTDIR}/usr/include/tcl
LDADD+= -ltcl -lm
DPADD+= ${LIBTCL} ${LIBM}

.include <bsd.prog.mk>
