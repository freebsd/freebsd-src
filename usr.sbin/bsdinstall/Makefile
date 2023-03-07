# $FreeBSD$

OSNAME?=	FreeBSD
SUBDIR=	distextract distfetch partedit runconsoles scripts
SUBDIR_PARALLEL=
SCRIPTS= bsdinstall
MAN= bsdinstall.8
PACKAGE=	bsdinstall
GENHDRS=	opt_osname.h
SRCS+=		${GENHDRS}
CLEANFILES+=	${GENHDRS}

SCRIPTS+=	startbsdinstall
SCRIPTSDIR_startbsdinstall=	${LIBEXECDIR}/bsdinstall

opt_osname.h: .PHONY
	if ! grep -q "^#define OSNAME \"${OSNAME}\"$"" ${.TARGET}; then \
		echo "#define OSNAME \"${OSNAME}\"" > ${.TARGET}; \
	fi

.include <bsd.prog.mk>
