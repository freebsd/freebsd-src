#---------------------------------------------------------------------------
#
# $FreeBSD: src/usr.sbin/i4b/dtmfdecode/Makefile,v 1.6.2.2 2001/08/01 17:29:17 obrien Exp $
#
#	last edit-date: [Tue Dec 14 21:17:46 1999]
#
#---------------------------------------------------------------------------

PROG=	dtmfdecode

CFLAGS+= -DDEBUG

# libm is only necessary if USE_COS is defined in the source
#LDADD=	-lm

test:   ${PROG}
	uudecode -p < dtmfsounds.al.uu | ./${PROG}

.include <bsd.prog.mk>
