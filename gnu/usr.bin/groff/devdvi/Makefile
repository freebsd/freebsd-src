# $FreeBSD: src/gnu/usr.bin/groff/devdvi/Makefile,v 1.5.8.1 2000/12/07 09:56:59 ru Exp $

DEV=		dvi
DEVFILES=	DESC R I B BI CW CWI MI S EX H HI HB SA SB \
		generate/CompileFonts generate/Makefile generate/msam.map\
		generate/msbm.map generate/texb.map generate/texex.map \
		generate/texi.map generate/texmi.map generate/texr.map \
		generate/texsy.map generate/textt.map

CLEANFILES+=	DESC

DESC:	DESC.in
	cat ${DIST_DIR}/DESC.in >DESC
	test -z '${DVIPRINT}' || echo print '${DVIPRINT}' >>DESC

.include "../Makefile.dev"
