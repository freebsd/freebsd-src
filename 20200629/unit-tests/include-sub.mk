# $NetBSD: include-sub.mk,v 1.1 2020/05/17 12:36:26 rillig Exp $

.if ${.INCLUDEDFROMFILE} == "include-main.mk"
LOG+=		sub-before-ok
.else
LOG+=		sub-before-fail
.endif

.include "include-subsub.mk"

.if ${.INCLUDEDFROMFILE} == "include-main.mk"
LOG+=		sub-after-ok
.else
.  for f in ${.INCLUDEDFROMFILE}
LOG+=		sub-after-fail\(${f:Q}\)
.  endfor
.endif
