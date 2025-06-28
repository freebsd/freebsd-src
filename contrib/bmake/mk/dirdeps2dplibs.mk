# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: dirdeps2dplibs.mk,v 1.3 2025/05/29 18:32:53 sjg Exp $

# DIRDEPS generally reflects things *actually used* by RELDIR.
# dirdeps2dplibs allows us to turn DIRDEPS into a DPLIBS list
# The order will however be sorted, so some
# manual tweaking may be needed.
#

dirdeps2dplibs:

.if ${.MAKE.LEVEL} > 0
# for customization
.-include <local.dirdeps2dplibs.mk>

_DEPENDFILE ?= ${.MAKE.DEPENDFILE}

.dinclude "${_DEPENDFILE}"

INCS_DIRS += h include incs
DIRDEPS2DPLIBS_FILTER += C;/(${INCS_DIRS:O:u:ts|})(\.common.*)*$$;;

dirdeps2dplibs:
	@echo
.if ${DEBUG_DIRDEPS2DPLIBS:Uno:@x@${RELDIR:M$x}@} != ""
	@echo "# DIRDEPS=${DIRDEPS:M*lib*}"
.endif
	@echo -n 'DPLIBS += \'; \
	echo '${DIRDEPS:M*lib*:${DIRDEPS2DPLIBS_FILTER:ts:}:T:O:u:tu:@d@${.newline}${.tab}_{LIB${d:S,^LIB,,}} \\@}' | \
	sed 's,_{,$${,g'; \
	echo


.endif
