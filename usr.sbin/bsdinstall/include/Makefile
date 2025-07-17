OSNAME?=	FreeBSD
GENHDRS=	opt_osname.h
SRCS+=		${GENHDRS}
CLEANFILES+=	${GENHDRS}

opt_osname.h: ${META_NOPHONY}
	@if ! grep -q "^#define OSNAME \"${OSNAME}\"$"" ${.TARGET} 2> /dev/null; then \
		echo "#define OSNAME \"${OSNAME}\"" > ${.TARGET}; \
	fi

MK_STAGING= no

.include <bsd.prog.mk>
