PROG=	man
SRCS=	man.c manpath.c glob.c
MAN1=	man.1
LDADD+=	-L${.CURDIR}/../lib -lman

CFLAGS+= -I${.CURDIR}/../lib -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS -DALT_SYSTEMS

man.1: man.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	man.man > man.1

.include <bsd.prog.mk>
