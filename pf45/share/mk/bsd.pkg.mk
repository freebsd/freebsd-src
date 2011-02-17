# $FreeBSD$

WRKDIR=${.OBJDIR}
.if ${.OBJDIR} == ${.CURDIR}
WRKDIR=${.CURDIR}/work
.endif
NO_WRKSUBDIR=YES
NO_CHECKSUM=YES
NO_BUILD=YES

fetch:
extract:
patch:
configure:
build:

.if target(__<bsd.obj.mk>__)
clean: do-clean
.if ${CANONICALOBJDIR} != ${.CURDIR} && exists(${CANONICALOBJDIR}/)
	@rm -rf ${CANONICALOBJDIR}
.else
	@if [ -L ${.CURDIR}/obj ]; then rm -f ${.CURDIR}/obj; fi
.if defined(CLEANFILES) && !empty(CLEANFILES)
	rm -f ${CLEANFILES}
.endif
.if defined(CLEANDIRS) && !empty(CLEANDIRS)
	rm -rf ${CLEANDIRS}
.endif
.endif
.endif

.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

install: install-message check-categories check-conflicts \
         run-depends lib-depends pre-install pre-install-script \
	 generate-plist check-already-installed \
	 check-umask install-mtree pre-su-install \
	 pre-su-install-script \
	 beforeinstall realinstall afterinstall \
	 add-plist-info post-install post-install-script \
	 compress-man run-ldconfig fake-pkg

.include <bsd.port.mk>
