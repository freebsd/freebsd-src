# $FreeBSD$

GENCAT=         gencat -new
BASESRC=        ${.CURDIR}/../../../../contrib/tcsh/nls

.SUFFIXES: .msg .cat

.msg.cat:
	${GENCAT} ${.TARGET} ${.IMPSRC}

CLEANFILES+= tcsh.cat tcsh.msg

all: tcsh.cat

tcsh.cat: tcsh.msg

distribute:
