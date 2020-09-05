# $NetBSD: include-subsub.mk,v 1.1 2020/05/17 12:36:26 rillig Exp $

.if ${.INCLUDEDFROMFILE:T} == "include-sub.mk"
LOG+=		subsub-ok
.else
LOG+=		subsub-fail
.endif
