GEN=	profile.h ${GEN_PROF_ERR_C} ${GEN_PROF_ERR_H}
GEN_PROF_ERR=	prof_err.et
GEN_PROF_ERR_C=	${GEN_PROF_ERR:S/.et$/.c/}
GEN_PROF_ERR_H=	${GEN_PROF_ERR:S/.et$/.h/}
${GEN_PROF_ERR:[2..-1]}: .NOMETA
CLEANFILES+=	et-h-prof_err.et et-h-prof_err.c et-h-prof_err.h \
	${GEN}

.include "${KRB5_SRCTOP}/Makefile.et"

${GEN_PROF_ERR_H}: ${GEN_PROF_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_PROF_ERR_C}: ${GEN_PROF_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

profile.h:	profile.hin prof_err.h
	cat ${.ALLSRC} > ${.TARGET}
