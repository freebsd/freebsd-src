#
# This file configures debug options for compiled targets.  It is meant
# to consolidate common logic in bsd.prog.mk and bsd.lib.mk.  It should
# not be included directly by Makefiles.
#

.include <bsd.opts.mk>

.if ${MK_ASSERT_DEBUG} == "no"
CFLAGS+= -DNDEBUG
# XXX: shouldn't we ensure that !asserts marks potentially unused variables as
# __unused instead of disabling -Werror globally?
MK_WERROR=	no
.endif

.if defined(DEBUG_FLAGS)
CFLAGS+=${DEBUG_FLAGS}
CXXFLAGS+=${DEBUG_FLAGS}

.if ${MK_CTF} != "no" && ${DEBUG_FLAGS:M-g} != ""
CTFFLAGS+= -g
.endif
.else
STRIP?= -s
.endif

.if ${MK_DEBUG_FILES} != "no" && empty(DEBUG_FLAGS:M-g) && \
    empty(DEBUG_FLAGS:M-gdwarf*)
.if !${COMPILER_FEATURES:Mcompressed-debug}
CFLAGS+= ${DEBUG_FILES_CFLAGS:N-gz*}
CXXFLAGS+= ${DEBUG_FILES_CFLAGS:N-gz*}
.else
CFLAGS+= ${DEBUG_FILES_CFLAGS}
CXXFLAGS+= ${DEBUG_FILES_CFLAGS}
.endif
CTFFLAGS+= -g
.endif

_debuginstall:
.if ${MK_DEBUG_FILES} != "no" && defined(DEBUGFILE)
.if defined(DEBUGMKDIR)
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},dbg} -d ${DESTDIR}${DEBUGFILEDIR}/
.endif
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},dbg} -o ${DEBUGOWN} -g ${DEBUGGRP} -m ${DEBUGMODE} \
	    ${DEBUGFILE} ${DESTDIR}${DEBUGFILEDIR}/${DEBUGFILE}
.endif
