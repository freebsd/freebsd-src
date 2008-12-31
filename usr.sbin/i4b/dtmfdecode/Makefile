#---------------------------------------------------------------------------
#
# $FreeBSD: src/usr.sbin/i4b/dtmfdecode/Makefile,v 1.9.32.1 2008/11/25 02:59:29 kensmith Exp $
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
