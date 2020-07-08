# $NetBSD: include-main.mk,v 1.1 2020/05/17 12:36:26 rillig Exp $
#
# Demonstrates that the .INCLUDEDFROMFILE magic variable does not behave
# as described in the manual page.
#
# The manual page says that it is the "filename of the file this Makefile
# was included from", while in reality it is the "filename in which the
# latest .include happened".
#

.if !defined(.INCLUDEDFROMFILE)
LOG+=		main-before-ok
.else
.  for f in ${.INCLUDEDFROMFILE}
LOG+=		main-before-fail\(${f:Q}\)
.  endfor
.endif

.include "include-sub.mk"

.if !defined(.INCLUDEDFROMFILE)
LOG+=		main-after-ok
.else
.  for f in ${.INCLUDEDFROMFILE}
LOG+=		main-after-fail\(${f:Q}\)
.  endfor
.endif

all:
	@printf '%s\n' ${LOG}
