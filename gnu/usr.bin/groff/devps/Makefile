DEV=		ps
FONTS=		S ZD ZDR SS AB ABI AI AR BMB BMBI BMI BMR \
		CB CBI CI CR HB HBI HI HR HNB HNBI HNI HNR \
		NB NBI NI NR PB PBI PI PR TB TBI TI TR ZCMI
SUPPORT=	download prologue symbolsl.pfa text.enc zapfdr.pfa
DEVGENFILES=	generate/Makefile generate/afmname generate/dingbatsmap \
		generate/dingbatsrmap generate/lgreekmap generate/symbol.sed \
		generate/symbolchars generate/symbolsl.afm generate/textmap
PSFILES=	prologue symbolsl.pfa zapfdr.pfa
FONTFILES=	$(FONTS) $(SUPPORT) $(DEVGENFILES) DESC
DEVFILES=	$(FONTFILES)

CLEANFILES+=	DESC ${PSFILES}

.include "../Makefile.inc"

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

fonts: DESC
	${MAKE} -f ${DIST_DIR}/generate/Makefile \
	  srcdir=${DIST_DIR}/generate DESC=${DIST_DIR}/DESC.in

prologue:	${DIST_DIR}/prologue.ps
zapfdr.pfa:	${DIST_DIR}/zapfdr.ps
symbolsl.pfa:	${DIST_DIR}/symbolsl.ps

.include "../Makefile.dev"
