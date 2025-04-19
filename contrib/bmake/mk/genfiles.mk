# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: genfiles.mk,v 1.4 2025/04/18 00:04:41 sjg Exp $
#
#	@(#) Copyright (c) 2024-2025, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

# Pipe the sources though egrep -v if EXCLUDES.${.TARGET} is defined
# and/or sed if SED_CMDS.${.TARGET} is defined
# Note: this works best in meta mode as any change to EXCLUDES or
# SED_CMDS will make the target out-of-date.
_GENFILES_USE:	.USE
	@cat ${SRCS.${.TARGET}:U${.ALLSRC:u}} \
	${EXCLUDES.${.TARGET}:D| ${EGREP:Uegrep} -v '${EXCLUDS.${.TARGET}:ts|}'} \
	${SED_CMDS.${.TARGET}:D| ${SED:Used} ${SED_CMDS.${.TARGET}}} \
	${FILTER.${.TARGET}:D| ${FILTER.${.TARGET}}} \
	> ${.TARGET}
