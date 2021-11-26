# $FreeBSD$

MAN=	adb.4 \
	akbd.4 \
	abtn.4 \
	ams.4 \
	cuda.4 \
	dtsec.4 \
	llan.4 \
	ofw_console.4 \
	pmu.4 \
	powermac_nvram.4 \
	smu.4 \
	snd_ai2s.4 \
	snd_davbus.4 \
	tsec.4

# Link files to the architecture directory
_ARCH_SUBDIR=powerpc
.for _manpage _link in ${MLINKS}
MLINKS+=${_link} ${_ARCH_SUBDIR}/${_link}
.endfor
.for _manpage in ${MAN}
MLINKS+=${_manpage} ${_ARCH_SUBDIR}/${_manpage}
.endfor

.include <bsd.prog.mk>
