DEVICE=		ps
FONTFILES=	$(FONTS) $(SUPPORT) $(DEVGENFILES) DESC
FONTS=		S ZD ZDR SS AB ABI AI AR BMB BMBI BMI BMR \
		CB CBI CI CR HB HBI HI HR HNB HNBI HNI HNR \
		NB NBI NI NR PB PBI PI PR TB TBI TI TR ZCMI
SUPPORT=	download prologue symbolsl.pfa text.enc zapfdr.pfa
DEVGENFILES=	generate/Makefile generate/afmname generate/dingbatsmap \
		generate/dingbatsrmap generate/lgreekmap generate/symbol.sed \
		generate/symbolchars generate/symbolsl.afm generate/textmap
PSFILES=	prologue symbolsl.pfa zapfdr.pfa

CLEANFILES+=	DESC ${PSFILES}

DESC: DESC.in
	-rm -f DESC
	cat ${.CURDIR}/DESC.in >DESC
	echo broken ${BROKEN_SPOOLER_FLAGS} >>DESC
	if test "${PAGE}" = A4; then \
	  echo "paperlength 841890" >>DESC; \
	else \
	  echo "paperlength 792000" >>DESC; \
	fi
	test -z '${PSPRINT}' ||	echo print '${PSPRINT}' >>DESC

${PSFILES}:
	-rm -f $@
	sed -f ${.CURDIR}/psstrip.sed $? >$@

fonts: DESC
	${MAKE} -f ${.CURDIR}/generate/Makefile \
	  srcdir=${.CURDIR}/generate DESC=${.CURDIR}/DESC.in

prologue: prologue.ps
zapfdr.pfa: zapfdr.ps
symbolsl.pfa: symbolsl.ps

.include "../Makefile.dev"
