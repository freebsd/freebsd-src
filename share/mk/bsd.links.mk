# $FreeBSD: src/share/mk/bsd.links.mk,v 1.2.34.1 2008/11/25 02:59:29 kensmith Exp $

.if !target(__<bsd.init.mk>__)
.error bsd.links.mk cannot be included directly.
.endif

afterinstall: _installlinks
.ORDER: realinstall _installlinks
_installlinks:
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		ln -f $$l $$t; \
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
		ln -fs $$l $$t; \
	done; true
.endif
