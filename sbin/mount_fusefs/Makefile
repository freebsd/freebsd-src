.if defined(DEBUG)
DEBUG_FLAGS+= -D_DEBUG -g
.endif

.if defined(DEBUG2G)
DEBUG_FLAGS+= -D_DEBUG2G -g
.endif

.if defined(DEBUG3G)
DEBUG_FLAGS+= -D_DEBUG3G -g
.endif

.if defined(DEBUG_MSG)
DEBUG_FLAGS+= -D_DEBUG_MSG
.endif

.if defined(F4BVERS)
DEBUG_FLAGS+= -DFUSE4BSD_VERSION="\"${F4BVERS}\""
.endif

PACKAGE=runtime
PROG=	mount_fusefs
MAN=	mount_fusefs.8
LIBADD=	util

.include <bsd.prog.mk>
