
.PATH: ${.CURDIR:H}/common

OBJS+=		crtsavres.o
CFLAGS+=	-I${.CURDIR} \
		-mlongcall -DCRT_IRELOC_RELA

CLEANFILES+=	crtsavres.S

# On powerpc64 crtsavres is an empty file
crtsavres.S:
	touch ${.TARGET}

.include <bsd.lib.mk>
