#	@(#)Makefile	5.2 (Berkeley) 3/5/91
#
# All library objects contain sccsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_SCCS and -DSYSLIBC_SCCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_SCCS from CFLAGS.
LIB=c
CFLAGS+=-DLIBC_SCCS -DSYSLIBC_SCCS
AINC=	-I${.CURDIR}/${MACHINE}

.include "${.CURDIR}/compat-43/Makefile.inc"
.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/gen/Makefile.inc"
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdlib/Makefile.inc"
.include "${.CURDIR}/string/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"

.include <bsd.lib.mk>
