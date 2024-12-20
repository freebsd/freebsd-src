PACKAGE=runtime
SCRIPTS = freebsd-version
MAN = freebsd-version.1
CLEANFILES = freebsd-version freebsd-version.sh
NEWVERS = ${SRCTOP}/sys/conf/newvers.sh

freebsd-version.sh: ${.CURDIR}/freebsd-version.sh.in ${NEWVERS}
	eval $$(sh ${NEWVERS} -v); \
	if ! sed -e "\
			s/@@TYPE@@/$${TYPE}/g; \
			s/@@REVISION@@/$${REVISION}/g; \
			s/@@BRANCH@@/$${BRANCH}/g; \
		" ${.CURDIR}/freebsd-version.sh.in >${.TARGET} ; then \
		rm -f ${.TARGET} ; \
		exit 1 ; \
	fi

.include <bsd.prog.mk>
