#
# Rules to build LateX documentation.
#
# $Id: elftoolchain.tex.mk 2552 2012-08-28 03:39:09Z jkoshy $
#

.include "${TOP}/mk/elftoolchain.os.mk"

.if defined(MKTEX) && ${MKTEX} == "yes" && exists(${MPOST}) && exists(${PDFLATEX})

TEXINPUTS=	`kpsepath tex`:${.CURDIR}
_TEX=		TEXINPUTS=${TEXINPUTS} ${PDFLATEX} -file-line-error \
			-halt-on-error

DOCSUBDIR=	elftoolchain	# Destination directory.

.MAIN:	all

all:	${DOC}.pdf .PHONY

# Build an index.
#
# First, we need to remove the existing ".ind" file and run `latex` once
# to generate it afresh.  This generates the appropriate ".idx" files used
# by `makeindex`.
# Next, `makeindex` is used to create the ".ind" file.
# Then another set of `latex` runs serves to typeset the index.
index:	.PHONY
	rm -f ${DOC}.ind
	${_TEX} ${DOC}.tex
	${MAKEINDEX} ${DOC}.idx
	${_TEX} ${DOC}.tex
	@if grep 'Rerun to get' ${DOC}.log > /dev/null; then \
		${_TEX} ${DOC}.tex; \
	fi

# Recognize additional suffixes.
.SUFFIXES:	.mp .eps .tex .pdf

# Rules to build MetaPost figures.
.mp.eps:
	@if [ "${.OBJDIR}" != "${.CURDIR}" ]; then cp ${.CURDIR}/${.IMPSRC:T} ${.OBJDIR}/; fi
	TEX=${MPOSTTEX} ${MPOST} -halt-on-error ${.IMPSRC:T} || (rm ${.IMPSRC:T:R}.1; false)
	mv ${.IMPSRC:T:R}.1 ${.TARGET}
.eps.pdf:
	${EPSTOPDF} ${.IMPSRC} > ${.TARGET} || (rm ${.TARGET}; false)

.for f in ${IMAGES_MP}
${f:R}.eps: ${.CURDIR}/${f}
CLEANFILES+=	${f:R}.eps ${f:R}.log ${f:R}.pdf ${f:R}.mpx
.endfor

CLEANFILES+=	mpxerr.tex mpxerr.log makempx.log missfont.log

${DOC}.pdf:	${SRCS} ${IMAGES_MP:S/.mp$/.pdf/g}
	${_TEX} ${.CURDIR}/${DOC}.tex > /dev/null || \
		(cat ${DOC}.log; rm -f ${.TARGET}; exit 1)
	@if grep 'undefined references' ${DOC}.log > /dev/null; then \
		${_TEX} ${.CURDIR}/${DOC}.tex > /dev/null; \
	fi
	@if grep 'Rerun to get' ${DOC}.log > /dev/null; then \
		${_TEX} ${.CURDIR}/${DOC}.tex > /dev/null; \
	fi

.for f in aux log out pdf toc ind idx ilg
CLEANFILES+=	${DOC}.${f}
.endfor

# Do something sensible for the `depend` and `cleandepend` targets.
depend:		.depend
	@true
.depend:
	@echo ${DOC}.pdf: ${SRCS} ${IMAGES_MP:S/.mp$/.pdf/g} > ${.TARGET}
cleandepend:	.PHONY
	rm -f .depend

clean clobber:		.PHONY
	rm -f ${CLEANFILES}

install:	all
	@mkdir -p ${DESTDIR}/${DOCDIR}/${DOCSUBDIR}
	${INSTALL} -g ${DOCGRP} -o ${DOCOWN} ${DOC}.pdf \
		${DESTDIR}/${DOCDIR}/${DOCSUBDIR}

# Include rules for `make obj`
.include <bsd.obj.mk>

.else

all clean clobber depend install obj:	.PHONY .SILENT
	echo -n WARNING: make \"${.TARGET}\" in \"${.CURDIR:T}\" skipped:
.if	defined(MKTEX) && ${MKTEX} == "yes"
	echo " missing tools."
.else
	echo " builds of TeX documentation are disabled."
.endif
	true
.endif
