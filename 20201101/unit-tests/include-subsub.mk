# $NetBSD: include-subsub.mk,v 1.3 2020/09/05 18:13:47 rillig Exp $

.if ${.INCLUDEDFROMFILE} == "include-sub.mk"
.  info subsub-ok
.else
.  warning subsub-fail(${.INCLUDEDFROMFILE})
.endif
