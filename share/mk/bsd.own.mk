#	$Id: bsd.own.mk,v 1.12 1998/01/04 00:00:21 steve Exp $
#
# The include file <bsd.own.mk> set common variables for owner,
# group, mode, and directories. Defaults are in brackets.
#
#
# +++ variables +++
#
# DESTDIR	Change the tree where the file gets installed. [not set]
#
# DISTDIR	Change the tree where the file for a distribution
# 		gets installed (see /usr/src/release/Makefile). [not set]
#
#
# COPY		The flag passed to the install program to cause the binary
#		to be copied rather than moved.  This is to be used when
#		building our own install script so that the entire system
#		can either be installed with copies, or with moves using
#		a single knob. [-c]
#
# COMPRESS_CMD	Program to compress documents. 
#		Output is to stdout. [gzip -cn]
#
# COMPRESS_EXT	File name extension of ${COMPRESS_CMD} command. [.gz]
#
# STRIP		The flag passed to the install program to cause the binary
#		to be stripped.  This is to be used when building your
#		own install script so that the entire system can be made
#		stripped/not-stripped using a single knob. [-s]
#
#
#
# BINOWN	Binary owner. [bin]
#
# BINGRP	Binary group. [bin]
#
# BINMODE	Binary mode. [555]
#
# NOBINMODE	Mode for non-executable files. [444]
#
#
# LIBDIR	Base path for libraries. [/usr/lib]
#
# LIBCOMPATDIR	Base path for compat libraries. [/usr/lib/compat]
#
# LIBDATADIR	Base path for misc. utility data files. [/usr/libdata]
#
# LINTLIBDIR	Base path for lint libraries. [/usr/libdata/lint]
#
# SHLIBDIR	Base path for shared libraries. [${LIBDIR}]
#
# LIBOWN	Library mode. [${BINOWN}]
#
# LIBGRP	Library group. [${BINGRP}]
#
# LIBMODE	Library mode. [${NOBINMODE}]
#
#
# KMODDIR	Base path for loadable kernel modules
#		(see lkm(4)). [/lkm]
#
# KMODOWN	LKM owner. [${BINOWN}]
#
# KMODGRP	LKM group. [${BINGRP}]
#
# KMODMODE	LKM mode. [${BINMODE}]
#
#
# SHAREDIR	Base path for architecture-independent ascii
#		text files. [/usr/share]
#
# SHAREOWN	ASCII text file owner. [bin]
#
# SHAREGRP	ASCII text file group. [bin]
#
# SHAREMODE	ASCII text file mode. [${NOBINMODE}]
#
#
# DOCDIR	Base path for system documentation (e.g. PSD, USD,
#		handbook, FAQ etc.). [${SHAREDIR}/doc]
#
# DOCOWN	Documentation owner. [${SHAREOWN}]
#
# DOCGRP	Documentation group. [${SHAREGRP}]
#
# DOCMODE	Documentation mode. [${NOBINMODE}]
#
#
# INFODIR	Base path for GNU's hypertext system
#		called Info (see info(1)). [${SHAREDIR}/info]
#
# INFOOWN	Info owner. [${SHAREOWN}]
#
# INFOGRP	Info group. [${SHAREGRP}]
#
# INFOMODE	Info mode. [${NOBINMODE}]
#
#
# MANDIR	Base path for manual installation. [${SHAREDIR}/man/man]
#
# MANOWN	Manual owner. [${SHAREOWN}]
#
# MANGRP	Manual group. [${SHAREGRP}]
#
# MANMODE	Manual mode. [${NOBINMODE}]
#
#
# NLSDIR	Base path for National Language Support files
#		installation (see mklocale(1)). [${SHAREDIR}/nls]
#
# NLSGRP	National Language Support files group. [${SHAREOWN}]
#
# NLSOWN	National Language Support files owner. [${SHAREGRP}]
#
# NLSMODE	National Language Support files mode. [${NONBINMODE}]
#
# INCLUDEDIR	Base path for standard C include files [/usr/include]

# Binaries
BINOWN?=	bin
BINGRP?=	bin
BINMODE?=	555
NOBINMODE?=	444

LIBDIR?=	/usr/lib
LIBCOMPATDIR?=	/usr/lib/compat
LIBDATADIR?=	/usr/libdata
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

INCLUDEDIR?=	/usr/include

# Common variables
.if !defined(DEBUG_FLAGS)
STRIP?=		-s
.endif

COPY?=		-c
COMPRESS_CMD?=	gzip -cn
COMPRESS_EXT?=	.gz
