# $FreeBSD: src/bin/csh/nls/Makefile.inc,v 1.2.2.1 2000/07/04 05:29:26 obrien Exp $

GENCAT=         gencat -new
BASESRC=        ${.CURDIR}/../../../../contrib/tcsh/nls

.SUFFIXES: .msg .cat

.msg.cat:
	${GENCAT} ${.TARGET} ${.IMPSRC}

CLEANFILES+= tcsh.cat tcsh.msg

all: tcsh.cat

tcsh.cat: tcsh.msg

distribute:
