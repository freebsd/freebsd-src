#	@(#)Makefile	8.1 (Berkeley) 6/27/93

# -DSLOWSYS	invoke xread() for system time quirk on PDP, others?
# -DNOKANJI	default is for Japanese Unix.  undef only for raw
#		parity-marked search capability, not standard w/grep.
# -DCHINESE	for systems using EUC Chinese2 codes
# -Dstrrchr=rindex, -Dstrchr=index	as necessary

PROG=	egrep
CFLAGS+=-Dstrrchr=rindex -Dstrchr=index -DNOKANJI
LDADD= -lcompat	# must search compat to get spencers early regexp package
MAN1=	grep.0
LINKS=	${BINDIR}/egrep ${BINDIR}/grep ${BINDIR}/egrep ${BINDIR}/fgrep
MLINKS=	grep.1 egrep.1 grep.1 fgrep.1

.include "../../Makefile.inc"
.include <bsd.prog.mk>
