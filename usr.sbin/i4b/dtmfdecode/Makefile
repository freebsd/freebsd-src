#---------------------------------------------------------------------------
#
# $FreeBSD$
#
#	last edit-date: [Tue Dec 14 21:17:46 1999]
#
#---------------------------------------------------------------------------

PROG            =       dtmfdecode
SRC             =       dtmfdecode.c

# libm is only necessary if USE_COS is defined in the source
#LDADD          +=      -lm

CFLAGS          +=      -Wall -g -DDEBUG
MAN1		=	dtmfdecode.1

test:   ${PROG}
	uudecode -p < dtmfsounds.al.uu | ./${PROG}

.include <bsd.prog.mk>
