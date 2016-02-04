# $FreeBSD$

.if !target(__<bsd.init.mk>__)
.error bsd.links.mk cannot be included directly.
.endif

afterinstall: _installlinks
.ORDER: realinstall _installlinks
_installlinks:
.for s t in ${LINKS}
	${INSTALL_LINK} ${DESTDIR}${s} ${DESTDIR}${t}
.endfor
.for s t in ${SYMLINKS}
	${INSTALL_SYMLINK} ${s} ${DESTDIR}${t}
.endfor
