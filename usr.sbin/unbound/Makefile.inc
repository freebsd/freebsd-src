# $FreeBSD$

NO_WERROR=	true
NO_WTHREAD_SAFETY= true
PACKAGE=	unbound

.for man in ${MAN}
${man}: ${UNBOUNDDIR}/doc/${man:S/local-//}
	sed -E \
	    -e 's/\<(fI)?unbound\>/\1local-unbound/g' \
	    -e 's/\<(fI)?Unbound\>/Local-unbound/g' \
	    -e 's/\/local-unbound/\/unbound/g' \
	    <${.ALLSRC} >${.TARGET}
CLEANFILES += ${man}
.endfor

.include "../Makefile.inc"
