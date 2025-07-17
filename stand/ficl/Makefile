#

.include <bsd.init.mk>
.include "${BOOTSRC}/ficl.mk"

.PATH: ${FICLSRC} ${FICLSRC}/${FICL_CPUARCH}

BASE_SRCS=	dict.c ficl.c fileaccess.c float.c loader.c math64.c \
		prefix.c search.c stack.c tools.c vm.c words.c

SRCS=		${BASE_SRCS} sysdep.c softcore.c
CLEANFILES+=	softcore.c testmain testmain.o

CFLAGS.gfx_loader.c += -I${SRCTOP}/sys/teken
CFLAGS.gfx_loader.c += -I${SRCTOP}/contrib/pnglite
.ifmake testmain
CFLAGS=	-DTESTMAIN -D_TESTMAIN
CFLAGS+=	-I${FICLSRC} -I${FICLSRC}/${FICL_CPUARCH} -I${LDRSRC}
SRCS+=		testmain.c
PROG=		testmain
.include <bsd.prog.mk>
.else
LIB=		ficl
BASE_SRCS+=	gfx_loader.c	# Not TESTMAINable
.include <bsd.lib.mk>
.endif

# Standard softwords
.PATH: ${FICLSRC}/softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr

softcore.c: ${SOFTWORDS} softcore.awk
	(cd ${FICLSRC}/softwords; cat ${SOFTWORDS} \
	    | awk -f softcore.awk -v datestamp="`LC_ALL=C date`") > ${.TARGET}
