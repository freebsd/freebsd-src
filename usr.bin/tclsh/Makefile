# $Id: Makefile,v 1.3 1997/02/22 19:57:21 peter Exp $

PROG=   tclsh
SRCS=   tclAppInit.c

BINDIR= /usr/bin

.PATH: ${DESTDIR}/usr/libdata/tcl

MAN1=   tclsh.1

CLEANFILES=     ${MAN1}

tclsh.1:        ${.CURDIR}/../../contrib/tcl/doc/tclsh.1
	sed '/\.so *man.macros/s;.*;.so /usr/share/tmac/tcl.macros;' < $> > $@

CFLAGS+= -I/usr/include/tcl
LDADD+= -ltcl -lm
DPADD+= ${LIBTCL} ${LIBM}

.include <bsd.prog.mk>
