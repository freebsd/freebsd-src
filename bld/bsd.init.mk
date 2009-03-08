# $FreeBSD$

.if !target(__<bsd.init.mk>__)
__<bsd.init.mk>__:

.if make(clean) || make(cleandir) || make(cleanstage)
MACHINES= 	${MACHINE_LIST}
.else
.if defined(ALLMACHINES)
MACHINES= 	${MACHINE_LIST:Ncommon:Nhost}
.else
MACHINES?= 	${MACHINE}
.endif
.endif

STAGEDIR=	${.OBJROOT}/stage/${MACHINE}
COMMONSTAGEDIR=	${.OBJROOT}/stage/common

SHAREDSTAGEDIR=	${.OBJROOT}/../shared/stage/${MACHINE}
COMMONSHAREDSTAGEDIR=	${.OBJROOT}/../shared/stage/common

SHAREDHOSTTOOL_STAGEDIR = ${.OBJROOT}/../shared/stage/host

STAGEDIRS=	${STAGEDIR} ${COMMONSTAGEDIR} ${SHAREDSTAGEDIR} ${COMMONSHAREDSTAGEDIR}

.if ${MACHINE} == host
HOSTPROG=	yes
.endif

.if exists(${.CURDIR}/../Buildfile.inc)
.include "${.CURDIR}/../Buildfile.inc"
.endif

.MAIN: all

afterinstall beforedepend beforeinstall obj depend: .PHONY
	@echo "${.TARGET}: Sorry, we don't do that anymore!"

.if ${__MKLVL__} == 1
clean: .PHONY
.for m in ${MACHINES}
	cd ${.CURDIR} && MACHINE=${m} ${MAKE} ${.MAKEFLAGS:NMACHINE=*:N-DALLMACHINES} clean
.endfor
.else
clean: .PHONY
	echo "Cleaning ${.OBJDIR}"
	find ${.OBJDIR} -depth 1 -type f -exec rm {} \;
	find ${.OBJDIR} -depth 1 -type l -exec rm {} \;
	for _d in ${CLEANDIRS}; \
	do \
		if [ -d ${.OBJDIR}/$${_d} ]; then \
			rm -rf ${.OBJDIR}/$${_d}; \
		fi; \
	done
.endif

.if ${__MKLVL__} == 1
cleandir: .PHONY
.for m in ${MACHINES}
	cd ${.CURDIR} && MACHINE=${m} ${MAKE} ${.MAKEFLAGS:NMACHINE=*:N-DALLMACHINES} cleandir
.endfor
.else
cleandir: .PHONY
	echo "Cleaning ${.OBJDIR}"
	rm -rf ${.OBJDIR}/*
.endif

cleanstage:
	echo "Cleaning ${.OBJROOT}/stage"
	rm -rf ${.OBJROOT}/stage
.for m in ${MACHINES}
	cd ${.SRCTOP}/stage && ${MAKE} MACHINE=${m} cleandir
.endfor

.else
.error "Do NOT include bsd.init.mk yourself! sys.mk includes it for you!"
.endif	# !target(__<bsd.init.mk>__)
