# $FreeBSD: src/gnu/usr.bin/man/apropos/Makefile,v 1.15.2.1 2001/04/25 14:04:13 ru Exp $
MAINTAINER= wosch

SCRIPTS=apropos
MAN=	apropos.1
CLEANFILES+= ${SCRIPTS} ${MAN}

LINKS=	${BINDIR}/apropos ${BINDIR}/whatis
MLINKS=	apropos.1 whatis.1

.sh:
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' \
		${.ALLSRC} > ${.TARGET}

apropos.1: apropos.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
		${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
