# arm headers installed on arm64

.PATH: ${SRCTOP}/sys/arm/include ${SRCTOP}/lib/msun/arm

INCS=	_align.h \
	_inttypes.h \
	_limits.h \
	_stdint.h \
	_types.h \
	acle-compat.h \
	armreg.h \
	asm.h \
	atomic.h \
	bus.h \
	counter.h \
	cpu.h \
	cpufunc.h \
	cpuinfo.h \
	efi.h \
	elf.h \
	exec.h \
	float.h \
	frame.h \
	ieeefp.h \
	param.h \
	pcb.h \
	pcpu.h \
	pmap.h \
	proc.h \
	profile.h \
	pte.h \
	reg.h \
	reloc.h \
	resource.h \
	runq.h \
	setjmp.h \
	signal.h \
	sysarch.h \
	sysreg.h \
	tls.h \
	ucontext.h \
	vdso.h \
	vfp.h \
	vmparam.h
# These kernel-only headers are used by procstat's ZFS support.
# This should be fixed.
INCS+=	pcpu_aux.h \
	sf_buf.h
# from lib/msun/arm
INCS+=	fenv.h
INCSDIR=	${INCLUDEDIR}/arm

beforeinstall: armdir
META_TARGETS+=	armdir

armdir:
	${INSTALL} -d ${TAG_ARGS} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${DESTDIR}${INCLUDEDIR}/arm

.include <bsd.prog.mk>
