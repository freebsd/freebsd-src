DEV=		lj4
LJ4RES=		300
LJ4PRINT=	$(PSPRINT)
DEVFILES=	$(FONTS) $(SUPPORT) $(DEVGENFILES) DESC
FONTS=		ALBB ALBR AOB AOI AOR CB CBI CI CR GB GBI GI GR \
		LGB LGI LGR OB OBI OI OR TB TBI TI TR UB UBI UI UR \
		UCB UCBI UCI UCR CLARENDON CORONET MARIGOLD S
#DEVGENFILES=	generate/Makefile generate/text.map generate/special.map
CLEANFILES+=	DESC

.include "../Makefile.inc"

DESC: DESC.in
	-rm -f DESC
	echo "res $(LJ4RES)" >DESC
	echo "unitwidth `expr 7620000 / $(LJ4RES)`" >>DESC
	cat ${DIST_DIR}/DESC.in >>DESC
	if test "$(PAGE)" = A4; then \
	  echo "papersize a4" >>DESC; \
	else \
	  echo "papersize letter" >>DESC; \
	fi
	test -z '$(LJ4PRINT)' || echo print '$(LJ4PRINT)' >>DESC

fonts:	DESC
	$(MAKE) -f ${DIST_DIR}/generate/Makefile \
		srcdir=${DIST_DIR}/generate DESC=${DIST_DIR}/DESC.in \
	  	HPFTODIT=${.CURDIR}/../hpftodit/hpftodit

.include "../Makefile.dev"
