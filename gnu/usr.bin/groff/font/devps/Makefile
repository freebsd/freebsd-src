# $FreeBSD$

DEV=		ps
DISTFILES=	text.enc download \
		S ZD ZDR SS AB ABI AI AR BMB BMBI BMI BMR \
		CB CBI CI CR HB HBI HI HR HNB HNBI HNI HNR \
		NB NBI NI NR PB PBI PI PR TB TBI TI TR ZCMI
PSFILES=	prologue symbolsl.pfa zapfdr.pfa
DEVGENFILES=	generate/Makefile generate/afmname generate/dingbats.map \
		generate/dingbats.rmap generate/lgreekmap generate/symbol.sed \
		generate/symbolchars generate/symbolsl.afm generate/textmap
DEVFILES=	DESC ${PSFILES} ${DISTFILES} ${DEVGENFILES}

CLEANFILES+=	DESC ${PSFILES}

DESC: DESC.in
	-rm -f DESC
	cat ${DIST_DIR}/DESC.in >DESC
	echo broken ${BROKEN_SPOOLER_FLAGS} >>DESC
	if test "${PAGE}" = A4; then \
	  echo "paperlength 841890" >>DESC; \
	else \
	  echo "paperlength 792000" >>DESC; \
	fi
	test -z '${PSPRINT}' ||	echo print '${PSPRINT}' >>DESC

${PSFILES}:
	-rm -f $@
	sed -f ${DIST_DIR}/psstrip.sed $? >$@

.include "../Makefile.dev"

prologue:	${DIST_DIR}/prologue.ps
zapfdr.pfa:	${DIST_DIR}/zapfdr.ps
symbolsl.pfa:	${DIST_DIR}/symbolsl.ps
