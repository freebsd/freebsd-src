PROG=	manpath
MAN1=	manpath.1
SRCS=	manpath.c
LDADD+=	-L${.CURDIR}/../lib -lman

CFLAGS+= -I${.CURDIR}/../lib -DMAIN -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS -DALT_SYSTEMS

manpath.1: manpath.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	manpath.man > manpath.1

.include <bsd.prog.mk>
