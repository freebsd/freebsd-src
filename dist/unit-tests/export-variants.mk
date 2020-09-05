# $NetBSD: export-variants.mk,v 1.2 2020/08/08 13:00:07 rillig Exp $
#
# Test whether exported variables apply to each variant of running
# external commands:
#
# The != assignments.
# The :!cmd! modifier.
# The :sh modifier.

SHVAR!=	env | grep ^UT_ || true
.if ${SHVAR} != ""
.warning At this point, no variable should be exported.
.endif

.if ${:!env | grep ^UT_ || true!} != ""
.warning At this point, no variable should be exported.
.endif

.if ${env | grep ^UT_ || true:L:sh} != ""
.warning At this point, no variable should be exported.
.endif

UT_VAR=		value
.export UT_VAR

SHVAR!=	env | grep ^UT_ || true
.if ${SHVAR} != "UT_VAR=value"
.warning At this point, no variable should be exported.
.endif

.if ${:!env | grep ^UT_ || true!} != "UT_VAR=value"
.warning At this point, some variables should be exported.
.endif

.if ${env | grep ^UT_ || true:L:sh} != "UT_VAR=value"
.warning At this point, some variables should be exported.
.endif

all:
	@:;
