#	@(#)Makefile	5.8 (Berkeley) 5/11/90
#
# PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
# --------------------         -----   ----------------------
# CURRENT PATCH LEVEL:         1       00121
# --------------------         -----   ----------------------
#
# 30 Mar 93	Rodney W. Grimes	Removed -lcompat from LDADD
#					and ${LIBCOMPAT} from DPADD
#

PROG=	msgs
DPADD=	${LIBTERM}
LDADD=	-ltermlib

.include <bsd.prog.mk>
