# $Id: Makefile,v 1.1 1996/06/26 17:52:19 phk Exp $

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
