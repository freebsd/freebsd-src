# $FreeBSD$

.if !target(__<bsd.init.mk>__)
.error bsd.links.mk cannot be included directly.
.endif

afterinstall: _installlinks
.ORDER: realinstall _installlinks
_installlinks:
.ifdef .PARSEDIR
.for s t in ${LINKS}
	@${ECHO} "$t -> $s" ;\
	${INSTALL_LINK} ${DESTDIR}$s ${DESTDIR}$t
.endfor
.for s t in ${SYMLINKS}
	@${ECHO} "$t -> $s" ;\
	${INSTALL_SYMLINK} $s ${DESTDIR}/$t
.endfor
.else # The following logic is needed for FMAKE in the bootstrapping process
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		${INSTALL_LINK} $$l $$t; \
	done; true
.endif
.if defined(SYMLINKS) && !empty(SYMLINKS)
	@set ${SYMLINKS}; \
	while test $$# -ge 2; do \
		l=$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		${INSTALL_SYMLINK} $$l $$t; \
	done; true
.endif
.endif
