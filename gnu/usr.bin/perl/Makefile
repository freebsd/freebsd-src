#
# Bmake file for perl 4.036
#
# Note: I'm not sure what to do with c2ph located in misc...
#

SUBDIR= perl tperl usub lib x2p

.if !defined(NOSUIDPERL) && exists(${.CURDIR}/sperl)
SUBDIR+=sperl
.endif

.include <bsd.subdir.mk>

