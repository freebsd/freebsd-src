# $FreeBSD$

SCRIPTS = freebsd-version
MAN = freebsd-version.1
CLEANFILES = freebsd-version.sh
NEWVERS = ${.CURDIR}/../../sys/conf/newvers.sh

freebsd-version.sh.in: ${NEWVERS}
freebsd-version.sh: ${.CURDIR}/freebsd-version.sh.in
	eval $$(egrep '^(TYPE|REVISION|BRANCH)=' ${NEWVERS}) ; \
	if ! sed -e "\
			s/@@TYPE@@/$${TYPE}/g; \
			s/@@REVISION@@/$${REVISION}/g; \
			s/@@BRANCH@@/$${BRANCH}/g; \
		" ${.ALLSRC} >${.TARGET} ; then \
		rm -f ${.TARGET} ; \
		exit 1 ; \
	fi

.include <bsd.prog.mk>
