# $Id$

PROG=   tclsh
SRCS=   tclAppInit.c

BINDIR= /usr/bin

.PATH: ${DESTDIR}/usr/libdata/tcl

MAN1=   tclsh.1

CLEANFILES=     ${MAN1}

tclsh.1:        ${.CURDIR}/../../contrib/tcl/doc/tclsh.1
	sed '/\.so *man.macros/s;.*;.so /usr/share/tmac/tcl.macros;' < $> > $@

LDADD+= -ltcl -lm
DPADD+= ${LIBTCL} ${LIBM}

.include <bsd.prog.mk>
