# $FreeBSD$

.if !target(__<bsd.init.mk>__)
.error bsd.links.mk cannot be included directly.
.endif

afterinstall: _installlinks
.ORDER: realinstall _installlinks
_installlinks:
.for s t in ${LINKS}
	@${ECHO} "$t -> $s" ;\
	${INSTALL_LINK} $s $t
.endfor
.for s t in ${SYMLINKS}
	@${ECHO} "$t -> $s" ;\
	${INSTALL_SYMLINK} $s $t
.endfor
