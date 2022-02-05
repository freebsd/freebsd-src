# $NetBSD: varquote.mk,v 1.5 2021/12/28 10:47:00 rillig Exp $
#
# Test VAR:q modifier

.if !defined(REPROFLAGS)
REPROFLAGS+=	-fdebug-prefix-map=\$$NETBSDSRCDIR=/usr/src
REPROFLAGS+=	-fdebug-regex-map='/usr/src/(.*)/obj$$=/usr/obj/\1'
all:
	@${MAKE} -f ${MAKEFILE} REPROFLAGS=${REPROFLAGS:S/\$/&&/g:Q}
	@${MAKE} -f ${MAKEFILE} REPROFLAGS=${REPROFLAGS:q}
.else
all:
	@printf "%s %s\n" ${REPROFLAGS}
.endif
