OBJTOP=../../../..
SYSINSTALL=     ${OBJTOP}/release/sysinstall/sysinstall

all: ${SYSINSTALL}

${SYSINSTALL}:
	cd ${.CURDIR}/../../../../release/sysinstall ; make
	@if ! test -f ${SYSINSTALL}  ; then \
	 ${ECHO} "sysinstall not made" ; \
	 ${ECHO} "Possibly need to 'make obj' for floppies and sysinstall" ; \
	 exit 1; \
	fi

.include "../../mk/makecrunch.mk"

