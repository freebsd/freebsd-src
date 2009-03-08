
.if defined(INCS) && !defined(INCDIR)
.error "You must define INCDIR!"
.endif

.if defined(ONLY_MACHINE) && !empty(ONLY_MACHINE:M${MACHINE})
.if defined(INCDIR) && !exists(${INCDIR}) && ${__MKLVL__} == 2
.error "INCDIR ${INCDIR} does not exist. Do you need to add it to the stage mtree files?"
.endif
.endif

ALLINCS=
.for _F in ${INCS}
ALLINCS+= ${INCDIR}/${_F:T}
${INCDIR}/${_F:T}	: ${_F}
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

.if defined(COMMONDIR) && ${MACHINE} == common
_RELINCDIR!= echo ${INCDIR} | sed -e s,${STAGEDIR},,
_RMFILES=
.for m in ${MACHINE_LIST:Ncommon}
.for i in ${INCS}
.if exists(${.OBJROOT}/stage/${m}${_RELINCDIR}/${i})
_RMFILES:= ${_RMFILES} ${.OBJROOT}/stage/${m}${_RELINCDIR}/${i}
.endif
.if exists(${.OBJROOT}/stage/${m}${_RELINCDIR}/${i}.srcrel)
_RMFILES:= ${_RMFILES} ${.OBJROOT}/stage/${m}${_RELINCDIR}/${i}.srcrel
.endif
.endfor
.endfor
.if !empty(_RMFILES)
X!= rm -f ${RMFILES} >&2; echo
.endif
.endif

allincs: ${ALLINCS}
