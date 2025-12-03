# $Id: host.libnames.mk,v 1.6 2025/08/09 22:42:24 sjg Exp $
#
#	@(#) Copyright (c) 2007-2009, Simon J. Gerraty
#
#	SPDX-License-Identifier: BSD-2-Clause
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#


DLIBEXT ?= .a
DSHLIBEXT ?= ${DLIBEXT}
HOST_LIBEXT ?= ${DSHLIBEXT}
HOST_LIBDIRS ?= /usr/lib /lib
HOST_LIBS ?=

.for x in ${HOST_LIBS:O:u}
.for d in ${HOST_LIBDIRS}
.if exists($d/lib$x${HOST_LIBEXT})
LIB${x:tu} ?= $d/lib$x${HOST_LIBEXT}
.endif
.endfor
.endfor
