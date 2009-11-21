# $FreeBSD: src/gnu/lib/libreadline/readline/doc/Makefile,v 1.5.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SRCDIR= ${.CURDIR}/../../../../../contrib/libreadline/doc

INFO = readline rluserman

INFOSECTION= "Programming & development tools."
INFOENTRY_readline= "* Readline: (readline).		The GNU Readline library"
INFOENTRY_rluserman= "* Readline: (readline).       The GNU readline library API"

CLEANFILES += readline.texi

readline.info: rlman.texi rluser.texi rltech.texi version.texi fdl.texi

readline.texi: rlman.texi
	cp ${SRCDIR}/rlman.texi ${.TARGET}

rluserman.info: rluserman.texi version.texi rluser.texi fdl.texi

.include <bsd.info.mk>
