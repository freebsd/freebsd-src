# $Id: libnames.mk,v 1.11 2025/08/09 22:42:24 sjg Exp $
#
#	@(#) Copyright (c) 2007-2009, Simon J. Gerraty
#
#	SPDX-License-Identifier: BSD-2-Clause
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

DLIBEXT ?= .a
DSHLIBEXT ?= .so

.-include <local.libnames.mk>
.-include <sjg.libnames.mk>
.-include <fwall.libnames.mk>
.-include <host.libnames.mk>
