# $FreeBSD$

DEV=		dvi
DEVFILES=	DESC \
		TR TI TB TBI CW CWI HR HI HB HBI \
		TREC TIEC TBEC TBIEC CWEC CWIEC HREC HIEC HBEC HBIEC \
		TRTC TITC TBTC TBITC CWTC CWITC HRTC HITC HBTC HBITC \
		MI S EX SA SB SC \
		generate/Makefile \
		generate/msam.map generate/msbm.map \
		generate/texb.map generate/texex.map generate/texi.map \
		generate/texmi.map generate/texr.map generate/texsy.map \
		generate/textt.map generate/textex.map generate/ec.map \
		generate/tc.map
DEVSCRIPTS=	generate/CompileFonts

CLEANFILES+=	DESC

DESC:	DESC.in
	cat ${DIST_DIR}/DESC.in >${.TARGET}
	if test "${PAGE}" = A4; then \
	  echo "papersize a4" >>${.TARGET}; \
	else \
	  echo "papersize letter" >>${.TARGET}; \
	fi
	test -z '${DVIPRINT}' || echo print '${DVIPRINT}' >>${.TARGET}

.include "../Makefile.dev"
