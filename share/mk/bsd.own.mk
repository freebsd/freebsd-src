# $FreeBSD$
#
# The include file <src.opts.mk> set common variables for owner,
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
# COMPRESS_CMD	Program to compress documents.
#		Output is to stdout. [gzip -cn]
#
# COMPRESS_EXT	File name extension of ${COMPRESS_CMD} command. [.gz]
#
# BINOWN	Binary owner. [root]
#
# BINGRP	Binary group. [wheel]
#
# BINMODE	Binary mode. [555]
#
# NOBINMODE	Mode for non-executable files. [444]
#
# LIBDIR	Base path for libraries. [/usr/lib]
#
# LIBCOMPATDIR	Base path for compat libraries. [/usr/lib/compat]
#
# LIBPRIVATEDIR	Base path for private libraries. [/usr/lib/private]
#
# LIBDATADIR	Base path for misc. utility data files. [/usr/libdata]
#
# LIBEXECDIR	Base path for system daemons and utilities. [/usr/libexec]
#
# LINTLIBDIR	Base path for lint libraries. [/usr/libdata/lint]
#
# SHLIBDIR	Base path for shared libraries. [${LIBDIR}]
#
# LIBOWN	Library owner. [${BINOWN}]
#
# LIBGRP	Library group. [${BINGRP}]
#
# LIBMODE	Library mode. [${NOBINMODE}]
#
#
# DEBUGDIR	Base path for standalone debug files. [/usr/lib/debug]
#
# DEBUGMODE	Mode for debug files. [${NOBINMODE}]
#
#
# KMODDIR	Base path for loadable kernel modules
#		(see kld(4)). [/boot/kernel]
#
# KMODOWN	Kernel and KLD owner. [${BINOWN}]
#
# KMODGRP	Kernel and KLD group. [${BINGRP}]
#
# KMODMODE	KLD mode. [${BINMODE}]
#
#
# SHAREDIR	Base path for architecture-independent ascii
#		text files. [/usr/share]
#
# SHAREOWN	ASCII text file owner. [root]
#
# SHAREGRP	ASCII text file group. [wheel]
#
# SHAREMODE	ASCII text file mode. [${NOBINMODE}]
#
#
# CONFDIR	Base path for configuration files. [/etc]
#
# CONFOWN	Configuration file owner. [root]
#
# CONFGRP	Configuration file group. [wheel]
#
# CONFMODE	Configuration file mode. [644]
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
#		installation. [${SHAREDIR}/nls]
#
# NLSOWN	National Language Support files owner. [${SHAREOWN}]
#
# NLSGRP	National Language Support files group. [${SHAREGRP}]
#
# NLSMODE	National Language Support files mode. [${NOBINMODE}]
#
# INCLUDEDIR	Base path for standard C include files [/usr/include]

.if !target(__<bsd.own.mk>__)
__<bsd.own.mk>__:

.include <bsd.opts.mk>		# options now here or src.opts.mk

.if !defined(_WITHOUT_SRCCONF)

.if ${MK_CTF} != "no"
CTFCONVERT_CMD=	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.elif defined(.PARSEDIR) || (defined(MAKE_VERSION) && ${MAKE_VERSION} >= 5201111300)
CTFCONVERT_CMD=
.else
CTFCONVERT_CMD=	@:
.endif 

.if ${MK_INSTALL_AS_USER} != "no"
_uid!=	id -u
.if ${_uid} != 0
.if !defined(USER)
USER!=	id -un
.endif
_gid!=	id -g
.for x in BIN CONF DOC DTB INFO KMOD LIB MAN NLS SHARE
$xOWN=	${USER}
$xGRP=	${_gid}
.endfor
.endif
.endif

.endif # !_WITHOUT_SRCCONF

# Binaries
BINOWN?=	root
BINGRP?=	wheel
BINMODE?=	555
NOBINMODE?=	444

.if defined(MODULES_WITH_WORLD)
KMODDIR?=	/boot/modules
.else
KMODDIR?=	/boot/kernel
.endif
KMODOWN?=	${BINOWN}
KMODGRP?=	${BINGRP}
KMODMODE?=	${BINMODE}
DTBDIR?=	/boot/dtb
DTBOWN?=	root
DTBGRP?=	wheel
DTBMODE?=	444

LIBDIR?=	/usr/lib
LIBCOMPATDIR?=	/usr/lib/compat
LIBPRIVATEDIR?=	/usr/lib/private
LIBDATADIR?=	/usr/libdata
LIBEXECDIR?=	/usr/libexec
LINTLIBDIR?=	/usr/libdata/lint
SHLIBDIR?=	${LIBDIR}
LIBOWN?=	${BINOWN}
LIBGRP?=	${BINGRP}
LIBMODE?=	${NOBINMODE}

DEBUGDIR?=	/usr/lib/debug
DEBUGMODE?=	${NOBINMODE}


# Share files
SHAREDIR?=	/usr/share
SHAREOWN?=	root
SHAREGRP?=	wheel
SHAREMODE?=	${NOBINMODE}

CONFDIR?=	/etc
CONFOWN?=	root
CONFGRP?=	wheel
CONFMODE?=	644

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
NLSOWN?=	${SHAREOWN}
NLSGRP?=	${SHAREGRP}
NLSMODE?=	${NOBINMODE}

INCLUDEDIR?=	/usr/include

#
# install(1) parameters.
#
HRDLINK?=	-l h
SYMLINK?=	-l s
RSYMLINK?=	-l rs

INSTALL_LINK?=		${INSTALL} ${HRDLINK}
INSTALL_SYMLINK?=	${INSTALL} ${SYMLINK}
INSTALL_RSYMLINK?=	${INSTALL} ${RSYMLINK}

# Common variables
.if !defined(DEBUG_FLAGS)
STRIP?=		-s
.endif

COMPRESS_CMD?=	gzip -cn
COMPRESS_EXT?=	.gz

# Set XZ_THREADS to 1 to disable multi-threading.
XZ_THREADS?=	0

.if !empty(XZ_THREADS)
XZ_CMD?=	xz -T ${XZ_THREADS}
.else
XZ_CMD?=	xz
.endif

# Pointer to the top directory into which tests are installed.  Should not be
# overriden by Makefiles, but the user may choose to set this in src.conf(5).
TESTSBASE?= /usr/tests

# Compat for the moment
.include <bsd.compiler.mk>

.endif	# !target(__<bsd.own.mk>__)
