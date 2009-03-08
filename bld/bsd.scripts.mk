
.if !defined(SCRDIR)
.error "You must define SCRDIR!"
.endif

ALLSCRS=

.for script in ${SCRIPTS}
.if defined(SCRIPTSNAME)
SCRIPTSNAME_${script}?=	${SCRIPTSNAME}
.else
SCRIPTSNAME_${script}?=	${script}
.endif
ALLSCRS+= ${SCRDIR}/${SCRIPTSNAME_${script}}
${SCRDIR}/${SCRIPTSNAME_${script}}	: ${script}
	cp ${.ALLSRC} ${.TARGET}; \
	echo "# ${.SRCREL}" > ${.TARGET}.srcrel
.endfor

.if ${__MKLVL__} != 1
all	: allscrs
.endif

allscrs: ${ALLSCRS}

.include <bsd.dirdep.mk>
