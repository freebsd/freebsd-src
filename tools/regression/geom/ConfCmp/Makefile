# $FreeBSD$

PROG=	ConfCmp
SRCS+=	ConfCmp.c
SRCS+=	subr_sbuf.c
VPATH+= /sys/kern
NOOBJ=	youbet
WARNS=	2
CFLAGS+=	 -g -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -static
# Uncomment for ElectricFence
#LDADD += -lefence -L/usr/local/lib

# Stuff for XML
LDADD += -L/usr/local/lib -lexpat
CFLAGS += -I/usr/local/include/xml

NOMAN=	yeah
CLEANFILES	+= _*

.include <bsd.prog.mk>

test:	${PROG}
	rm -f _* *.core
	./${PROG} a1.conf a1a.conf
	if ./${PROG} a1.conf a1b.conf > /dev/null 2>&1 ; then exit 1 ; fi
	if ./${PROG} a1.conf a1c.conf > /dev/null 2>&1 ; then exit 1 ; fi
	if ./${PROG} a1.conf a1d.conf > /dev/null 2>&1 ; then exit 1 ; fi

ttest:	${PROG}
	rm -f _* *.core
	./${PROG} a1.conf a1a.conf
	./${PROG} a1.conf a1b.conf 
	./${PROG} a1.conf a1c.conf 
	./${PROG} a1.conf a1d.conf 
