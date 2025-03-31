# $NetBSD: varmod-hash.mk,v 1.6 2024/07/20 11:05:12 rillig Exp $
#
# Tests for the :hash variable modifier, which computes a 32-bit hash from
# the value of the expression.

# Test vectors for generating certain hashes.  Found by a brute force
# search over [a-z]{8}.
#
VECTORS+=	00000000 adjbuqnt
VECTORS+=	00000001 beiiyxdp
VECTORS+=	00000002 ajriwzqe
VECTORS+=	00000004 aimszzcb
VECTORS+=	00000008 afffvsgz
VECTORS+=	00000010 alkksbun
VECTORS+=	00000020 arqeianj
VECTORS+=	00000040 acgaltwv
VECTORS+=	00000080 addsjxec
VECTORS+=	00000100 acbozubm
VECTORS+=	00000200 acnbugtp
VECTORS+=	00000400 ajyfkpcl
VECTORS+=	00000800 akobyelz
VECTORS+=	00001000 aclmaggk
VECTORS+=	00002000 aauwlqiq
VECTORS+=	00004000 ankfvoqf
VECTORS+=	00008000 airtytts
VECTORS+=	00010000 bfwwrqfi
VECTORS+=	00020000 actwkzix
VECTORS+=	00040000 alsfbgvo
VECTORS+=	00080000 aioiauem
VECTORS+=	00100000 bxexhpji
VECTORS+=	00200000 awtxcwch
VECTORS+=	00400000 aoqpmqam
VECTORS+=	00800000 akgtvjhz
VECTORS+=	01000000 bcmsuvrm
VECTORS+=	02000000 aqnktorm
VECTORS+=	04000000 aweqylny
VECTORS+=	08000000 crvkuyze
VECTORS+=	10000000 alxiatjv
VECTORS+=	20000000 aezwuukx
VECTORS+=	40000000 abdpnifu
VECTORS+=	80000000 auusgoii
VECTORS+=	ffffffff ahnvmfdw

VECTORS+=	b2af338b ""
VECTORS+=	3360ac65 a
VECTORS+=	7747f046 ab
VECTORS+=	9ca87054 abc
VECTORS+=	880fe816 abcd
VECTORS+=	208fcbd3 abcde
VECTORS+=	d5d376eb abcdef
VECTORS+=	de41416c abcdefghijklmnopqrstuvwxyz

.for hash input in ${VECTORS}
.  if ${input:S,^""$,,:hash} != ${hash}
.    warning Expected ${hash} for ${input}, but was ${input:hash}.
.  endif
.endfor

all: step-{1,2,3,4,5}
step-1:
	@echo ${12345:L:has}			# modifier name too short
step-2:
	@echo ${12345:L:hash}			# ok
step-3:
	@echo ${12345:L:hash=SHA-256}		# :hash does not accept '='
step-4:
	@echo ${12345:L:hasX}			# misspelled
step-5:
	@echo ${12345:L:hashed}			# modifier name too long
