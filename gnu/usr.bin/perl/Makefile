# $FreeBSD$

.ifmake !install && !distribute
_MINI=	miniperl
.endif

SUBDIR= libperl ${_MINI} perl suidperl library pod utils x2p BSDPAN

MAINTAINER=markm@freebsd.org

.BEGIN:
.ifmake depend || all
	if [ \! -f ${.OBJDIR}/build-tools ] ; then \
		cd ${.CURDIR}/libperl && ${MAKE} depend && ${MAKE} all ; \
		cd ${.CURDIR}/miniperl && ${MAKE} depend && ${MAKE} all ; \
		touch ${.OBJDIR}/build-tools ; \
	fi
.endif
.ifmake cleandir || clean
	@rm -f ${.OBJDIR}/build-tools
.endif

.include <bsd.subdir.mk>
