SUBDIR=	papers
PROGS=	refer addbib lookbib sortbib mkey inv hunt
SRCS.refer= glue1.c \
	refer1.c \
	refer2.c \
	refer4.c \
	refer5.c \
	refer6.c \
	mkey3.c \
	refer7.c \
	refer8.c \
	hunt2.c \
	hunt3.c \
	deliv2.c \
	hunt5.c \
	hunt6.c \
	hunt8.c \
	glue3.c \
	hunt7.c \
	hunt9.c \
	glue2.c \
	glue4.c \
	glue5.c \
	refer0.c \
	shell.c

SRCS.mkey=	mkey1.c \
		mkey2.c \
		mkey3.c \
		deliv2.c

SRCS.inv=	inv1.c \
		inv2.c \
		inv3.c \
		inv5.c \
		inv6.c \
		deliv2.c

SRCS.hunt=	hunt1.c \
		hunt2.c \
		hunt3.c \
		hunt5.c \
		hunt6.c \
		hunt7.c \
		glue5.c \
		refer3.c \
		hunt9.c \
		shell.c \
		deliv2.c \
		hunt8.c \
		glue4.c \
		tick.c

MAN.mkey=
MAN.inv=
MAN.hunt=

SCRIPTS=	roffbib indxbib
CLEANFILES=	roffbib indxbib

BINDIR.hunt=	${LIBDIR}/reftools
BINDIR.inv=	${LIBDIR}/reftools
BINDIR.mkey=	${LIBDIR}/reftools

CFLAGS+=	-DMACDIR='"${WORLDTMPDIR}${MACDIR}"' -DREFDIR='"${WORLDTMPDIR}${REFDIR}"'

.SUFFIXES:	.sh
.sh:
	echo "#!/bin/sh" > ${.TARGET}
	sed 's:@BINDIR@:$(BINDIR):g; s:@REFDIR@:${REFDIR}:g' ${.IMPSRC} >> ${.TARGET}

.include <bsd.progs.mk>

.PATH: ${DOCTOOLSDIR}/refer
