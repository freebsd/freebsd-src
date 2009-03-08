
.if defined(RELFILES) && !defined(RELDIR)
.error "You must define RELDIR!"
.endif

ALLFILES=

.for _F in ${RELFILES}
ALLFILES+= ${RELDIR}/${_F:T}
${RELDIR}/${_F:T}	: ${_F}
	if [ -f ${.TARGET}.srcrel ]; \
	then \
		x=`head -n 1 ${.TARGET}.srcrel`; \
		if [ "$$x" != "# ${.SRCREL}" ]; \
		then \
			echo "${.TARGET} has been staged from a different directory $$x"; \
			exit 1; \
		fi; \
	fi; \
	cp ${.ALLSRC} ${.TARGET}; \
	echo "# ${.SRCREL}" > ${.TARGET}.srcrel
.endfor

relfiles : ${ALLFILES}
