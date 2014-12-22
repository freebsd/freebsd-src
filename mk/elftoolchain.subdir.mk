#
# Rules for recursing into directories
# $Id: elftoolchain.subdir.mk 3023 2014-04-17 18:06:06Z jkoshy $

# Pass down 'test' as a valid target.

.include "$(TOP)/mk/elftoolchain.os.mk"

.if ${OS_HOST} == DragonFly
clobber test:: _SUBDIR
.elif ${OS_HOST} == FreeBSD
SUBDIR_TARGETS+=	clobber test
.elif ${OS_HOST} == OpenBSD
clobber test:: _SUBDIRUSE
.else		# NetBSD, pmake on Linux
TARGETS+=	cleandepend clobber test
.endif

.include <bsd.subdir.mk>
