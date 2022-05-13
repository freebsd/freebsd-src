# $FreeBSD$

OSNAME?=	FreeBSD
SUBDIR= distextract distfetch partedit scripts
SUBDIR_PARALLEL=
SCRIPTS= bsdinstall
MAN= bsdinstall.8
PACKAGE=	bsdinstall
GENHDRS=	opt_osname.h
SRCS+=		${GENHDRS}
CLEANFILES+=	${GENHDRS}

opt_osname.h: .PHONY
	if ! grep -q "^#define OSNAME \"${OSNAME}\"$"" ${.TARGET}; then \
		echo "#define OSNAME \"${OSNAME}\"" > ${.TARGET}; \
	fi

.include <bsd.prog.mk>
