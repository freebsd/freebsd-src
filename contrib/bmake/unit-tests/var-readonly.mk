# $NetBSD: var-readonly.mk,v 1.3 2023/06/19 15:37:48 sjg Exp $

# the answer
N = 42
.READONLY: N
# this should be ignored
N = 666
.if ${N} != 42
.error N ($N) should be 42
.endif

# undef should fail
.undef N
.ifndef N
.error N should not be undef'd
.endif

.NOREADONLY: N
# now we can change it
N = 69
.if ${N} == 42
.error N should not be 42
.endif

all:
