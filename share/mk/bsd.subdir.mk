#	from: @(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91
#	$Id: bsd.subdir.mk,v 1.9 1996/04/05 22:22:44 wosch Exp $

.MAIN: all

_SUBDIRUSE: .USE
	@for entry in ${SUBDIR}; do \
		(if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			${ECHODIR} "===> ${DIRPRFX}$${entry}.${MACHINE}"; \
			edir=$${entry}.${MACHINE}; \
			cd ${.CURDIR}/$${edir}; \
		else \
			${ECHODIR} "===> ${DIRPRFX}$$entry"; \
			edir=$${entry}; \
			cd ${.CURDIR}/$${edir}; \
		fi; \
		${MAKE} ${.TARGET:realinstall=install} DIRPRFX=${DIRPRFX}$$edir/); \
	done

${SUBDIR}::
	@if test -d ${.TARGET}.${MACHINE}; then \
		cd ${.CURDIR}/${.TARGET}.${MACHINE}; \
	else \
		cd ${.CURDIR}/${.TARGET}; \
	fi; \
	${MAKE} all


.for __target in all clean cleandir cleandepend obj objlink depend maninstall lint tags 
.if !target(__target)
${__target}: _SUBDIRUSE
.endif
.endfor

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: afterinstall
afterinstall: realinstall
realinstall: beforeinstall _SUBDIRUSE
.endif

DISTRIBUTION?=	bin
.if !target(afterdistribute)
afterdistribute:
.endif
.if !target(distribute)
distribute: _SUBDIRUSE 
	cd ${.CURDIR} ; ${MAKE} afterdistribute DESTDIR=${DISTDIR}/${DISTRIBUTION}
.endif
