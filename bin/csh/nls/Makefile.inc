# $FreeBSD: src/bin/csh/nls/Makefile.inc,v 1.2.2.2 2002/07/16 14:05:30 ru Exp $

GENCAT=         gencat -new
BASESRC=        ${.CURDIR}/../../../../contrib/tcsh/nls

.SUFFIXES: .msg .cat

.msg.cat:
	${GENCAT} ${.TARGET} ${.IMPSRC}

CLEANFILES+= tcsh.cat tcsh.msg

all: tcsh.cat

tcsh.cat: tcsh.msg
