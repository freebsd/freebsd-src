#	$Id: bsd.own.mk,v 1.4 1996/03/24 00:31:56 wosch Exp $

# where the system object and source trees are kept; can be configurable
# by the user in case they want them in ~/foosrc and ~/fooobj, for example
BSDSRCDIR?=	/usr/src
BSDOBJDIR?=	/usr/obj


# Binaries
BINOWN?=	bin
BINGRP?=	bin
BINMODE?=	555
NOBINMODE?=	444

LIBDIR?=	/usr/lib
LINTLIBDIR?=	/usr/libdata/lint
SHLIBDIR?=	${LIBDIR}
LIBOWN?=	${BINOWN}
LIBGRP?=	${BINGRP}
LIBMODE?=	${NOBINMODE}

KMODDIR?=	/lkm
KMODOWN?=	${BINOWN}
KMODGRP?=	${BINGRP}
KMODMODE?=	${BINMODE}


# Share files
SHAREDIR?=	/usr/share
SHAREOWN?=	bin
SHAREGRP?=	bin
SHAREMODE?=	${NOBINMODE}

MANDIR?=	${SHAREDIR}/man/man
MANOWN?=	${SHAREOWN}
MANGRP?=	${SHAREGRP}
MANMODE?=	${NOBINMODE}

DOCDIR?=	${SHAREDIR}/doc
DOCOWN?=	${SHAREOWN}
DOCGRP?=	${SHAREGRP}
DOCMODE?=	${NOBINMODE}

INFODIR?=	${SHAREDIR}/info
INFOOWN?=	${SHAREOWN}
INFOGRP?=	${SHAREGRP}
INFOMODE?=	${NOBINMODE}

NLSDIR?=	${SHAREDIR}/nls
NLSGRP?=	${SHAREOWN}
NLSOWN?=	${SHAREGRP}
NLSMODE?=	${NONBINMODE}

# Common variables
.if !defined(DEBUG_FLAGS)
STRIP?=		-s
.endif

COPY?=		-c
