# $NetBSD: include-subsub.inc,v 1.1 2023/01/19 23:26:14 rillig Exp $

.if ${.INCLUDEDFROMFILE} == "include-sub.inc"
.MAKEFLAGS: -dp
.  info subsub-ok
.MAKEFLAGS: -d0
.else
.  warning subsub-fail(${.INCLUDEDFROMFILE})
.endif
