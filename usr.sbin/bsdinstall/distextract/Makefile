
BINDIR= ${LIBEXECDIR}/bsdinstall
PROG=	distextract
CFLAGS+= -I${SRCTOP}/contrib/bsddialog/lib -I.
LIBADD=	archive bsddialog m
SRCS=	distextract.c

MAN=
GENHDRS=	opt_osname.h
SRCS+=		${GENHDRS}
CLEANFILES+=	${GENHDRS}

opt_osname.h: .PHONY
	if ! grep -q "^#define OSNAME \"${OSNAME}\"$"" ${.TARGET}; then \
		echo "#define OSNAME \"${OSNAME}\"" > ${.TARGET}; \
	fi

.include <bsd.prog.mk>
