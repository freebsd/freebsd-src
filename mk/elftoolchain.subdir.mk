#
# Rules for recursing into directories
# $Id: elftoolchain.subdir.mk 3720 2019-03-23 08:40:59Z jkoshy $

# Pass down 'test' as a valid target.

.include "$(TOP)/mk/elftoolchain.os.mk"

.if ${OS_HOST} == FreeBSD
SUBDIR_TARGETS+=	clobber test
.elif ${OS_HOST} == OpenBSD
clobber: _SUBDIRUSE
.else		# NetBSD, pmake on Linux
TARGETS+=	cleandepend clobber test
.endif

.include <bsd.subdir.mk>
