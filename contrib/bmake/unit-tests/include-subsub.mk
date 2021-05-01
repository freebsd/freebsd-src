# $NetBSD: include-subsub.mk,v 1.4 2021/01/26 23:44:56 rillig Exp $

.if ${.INCLUDEDFROMFILE} == "include-sub.mk"
.MAKEFLAGS: -dp
.  info subsub-ok
.MAKEFLAGS: -d0
.else
.  warning subsub-fail(${.INCLUDEDFROMFILE})
.endif
