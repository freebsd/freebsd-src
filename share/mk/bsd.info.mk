BINDIR?=	/usr/share/info
MAKEINFO?=	makeinfo
MAKEINFOFLAGS?=	# --no-split would simplify some things, e.g., compression

.SUFFIXES: .info .texi
.texi.info:
	${MAKEINFO} ${MAKEINFOFLAGS} ${.IMPSRC} -o ${.TARGET}

all: ${INFO:S/$/.info/g}

# Hacks to interface to bsd.doc.mk.
DOC=		${INFO}
PRINTER=	texi
depend:

.include <bsd.doc.mk>
