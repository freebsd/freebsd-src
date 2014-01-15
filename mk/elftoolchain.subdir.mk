#
# Rules for recursing into directories
# $Id: elftoolchain.subdir.mk 2292 2011-12-04 08:09:17Z jkoshy $

# Pass down 'test' as a valid target.

.include "$(TOP)/mk/elftoolchain.os.mk"

.if ${OS_HOST} == DragonFly
clobber test:: _SUBDIR
.elif ${OS_HOST} == FreeBSD
SUBDIR_TARGETS+=	clobber test
.elif ${OS_HOST} == OpenBSD
clobber test:: _SUBDIRUSE
.else		# NetBSD
TARGETS+=	clobber test
.endif

.include <bsd.subdir.mk>
