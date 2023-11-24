# i386 headers installed on amd64

.PATH: ${SRCTOP}/sys/i386/include
INCS=	\
	asm.h \
	asmacros.h \
	atomic.h \
	cpufunc.h \
	pmap.h \
	proc.h \
	profile.h \
	segments.h \
	vmparam.h
# These kernel-only headers are used by procstat's ZFS support.
# This should be fixed.
INCS+=	\
	counter.h \
	md_var.h \
	pcpu.h \
	pcpu_aux.h
INCSDIR=	${INCLUDEDIR}/i386

beforeinstall: i386dir
META_TARGETS+=	i386dir

i386dir:
	${INSTALL} -d ${TAG_ARGS} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${DESTDIR}${INCLUDEDIR}/i386


.include <bsd.prog.mk>
