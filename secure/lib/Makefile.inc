#	$Id: Makefile.inc,v 1.3 1994/11/14 20:45:28 phk Exp $

.include "${.CURDIR}/../../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
