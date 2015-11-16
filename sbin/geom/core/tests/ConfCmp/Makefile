# $FreeBSD$

BINDIR=	${TESTSBASE}/sbin/geom/core/ConfCmp

PROG=	ConfCmp
SRCS+=	ConfCmp.c
SRCS+=	subr_sbuf.c

.PATH: ${SRCTOP}/sys/kern

# Uncomment for ElectricFence
#LDADD+= -lefence -L/usr/local/lib

DPADD+=		${LIBBSDXML}
LDADD+=		-lbsdxml

LDFLAGS+=	-static

MAN=
CLEANFILES+=	 _*

# XXX: this should be in a test script
check: .PHONY
check: ${PROG}
	cd ${.CURDIR} && env PATH="$$PATH:${.OBJDIR}" ${MAKE} _check

_check: .PHONY
_check: ${PROG}
	rm -f _* *.core
	cd ${.CURDIR}; ${PROG} a1.conf a1.conf
	cd ${.CURDIR}; ${PROG} a1.conf a1a.conf
	cd ${.CURDIR}; if ${PROG} a1.conf a1b.conf > /dev/null 2>&1 ; then exit 1 ; fi
	cd ${.CURDIR}; if ${PROG} a1.conf a1c.conf > /dev/null 2>&1 ; then exit 1 ; fi
	cd ${.CURDIR}; if ${PROG} a1.conf a1d.conf > /dev/null 2>&1 ; then exit 1 ; fi
	cd ${.CURDIR}; ${PROG} a2.conf a2.conf
	cd ${.CURDIR}; ${PROG} a2.conf a2a.conf
	cd ${.CURDIR}; if ${PROG} a2.conf a2b.conf > /dev/null 2>&1 ; then exit 1 ; fi
	cd ${.CURDIR}; if ${PROG} a2.conf a2c.conf > /dev/null 2>&1 ; then exit 1 ; fi
	cd ${.CURDIR}; if ${PROG} a2.conf a2d.conf > /dev/null 2>&1 ; then exit 1 ; fi

WARNS?=	5

.include <bsd.prog.mk>
