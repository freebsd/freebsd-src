# $NetBSD: var-readonly.mk,v 1.1 2023/01/24 00:20:00 sjg Exp $

# the answer
N = 42
.READONLY: N
# this should be ignored
N = 666
.if ${N} != 42
.error N ($N) should be 42
.endif

.NOREADONLY: N
# now we can change it
N = 69
.if ${N} == 42
.error N should not be 42
.endif

all:

