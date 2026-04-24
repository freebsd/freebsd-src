# SPDX-License-Identifier: ISC
#
# Copyright (c) 2026 Lexi Winter <ivy@FreeBSD.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# Variable definitions used by bsd.pkg.mk.  All of these may be overridden:
#
# PKG_CMD	The pkg command.  [pkg]
# PKG_FORMAT	The archive format used when creating packages.  One of
# 		"tzst", "txz", "tbz", "tgz" or "tar".  [tzst]
# PKG_LEVEL	The compression level for compressed package formats.
# 		The meaning depends on the exact format, or one of the
# 		special values "fast" or "best" may be used. [-1]
# PKG_CTHREADS	How many threads to use when creating compressed packages.
# 		Set to "0" or "auto" to auto-detect based on the number
# 		of CPUs.  [0]
# PKG_ABI_FILE	The file used to determine the ABI to use when creating
# 		packages.  This should refer to the system being built,
# 		not the host system. [${WSTAGEDIR}/usr/bin/uname}
#
#
# Package metadata:
#
# PKG_NAME_PREFIX	The prefix to use for package names.  [FreeBSD]
# PKG_MAINTAINER	The package maintainer.  [re@FreeBSD.org]
# PKG_WWW		The package website.  [https://www.FreeBSD.org]
#
#
# Only if _PKG_NEED_ABI is defined:
#
# PKG_ABI	The ABI string to use when creating packages.  [autodetected]
#
.if !target(__<bsd.pkg.pre.mk>__)
__<bsd.pkg.pre.mk>__:  .NOTMAIN

# How we invoke pkg.
PKG_CMD?=	pkg
PKG_FORMAT?=	tzst
PKG_LEVEL?=	-1
PKG_CLEVEL=	${"${PKG_FORMAT:Mtar}" != "":?:-l ${PKG_LEVEL}}
PKG_CTHREADS?=	0
PKG_ABI_FILE?=	${WSTAGEDIR}/usr/bin/uname

# These are used in the generated packages, and can be overridden for
# downstream builds.
PKG_NAME_PREFIX?=	FreeBSD
PKG_MAINTAINER?=	re@FreeBSD.org
PKG_WWW?=		https://www.FreeBSD.org

# These can be set per-package.
PKG_LICENSELOGIC?=	single
PKG_LICENSES?=		BSD2CLAUSE
PKG_SETS?=		optional optional-jail

# The set annotation may be removed in the future, so don't rely on
# it being here.
PKG_ANNOTATIONS+=	set
PKG_ANNOTATIONS.set=	${PKG_SETS:ts,:[*]}

.endif	# !target(__<bsd.pkg.pre.mk>__)

# This always needs to be evaluated since something may have previously
# included us without setting _PKG_NEED_ABI.
.if defined(_PKG_NEED_ABI)

. if !defined(PKG_ABI)
PKG_ABI!=		${PKG_CMD} -o ABI_FILE=${PKG_ABI_FILE} config ABI
. endif

# Usually SRCRELDATE comes from Makefile.inc1, but if it's missing,
# find it ourselves.
. if !defined(PKG_OSVERSION)
.  if defined(SRCRELDATE)
PKG_OSVERSION=	${SRCRELDATE}
.  else
PKG_OSVERSION!=	awk '/^\#define[[:space:]]*__FreeBSD_version/ { print $$3 }' \
		${SRCTOP}/sys/sys/param.h
.  endif
. endif

.endif	# defined(_PKG_NEED_ABI)
