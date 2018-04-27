#
# Rules for recursing into directories
# $Id: elftoolchain.subdir.mk 3608 2018-04-14 21:23:04Z jkoshy $

# Pass down 'test' as a valid target.

.include "$(TOP)/mk/elftoolchain.os.mk"

.if ${OS_HOST} == FreeBSD
SUBDIR_TARGETS+=	clobber test
.elif ${OS_HOST} == OpenBSD
clobber test:: _SUBDIRUSE
.else		# NetBSD, pmake on Linux
TARGETS+=	cleandepend clobber test
.endif

.include <bsd.subdir.mk>
