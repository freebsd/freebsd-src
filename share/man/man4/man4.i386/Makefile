
MAN=	apm.4 \
	CPU_ELAN.4 \
	glxiic.4 \
	glxsb.4 \
	longrun.4 \
	npx.4 \
	pae.4 \
	pbio.4 \
	perfmon.4 \
	pnp.4 \
	pnpbios.4 \
	sbni.4 \
	smapi.4 \
	vpd.4

MLINKS=	CPU_ELAN.4 CPU_SOEKRIS.4
MLINKS+=pae.4 PAE.4
MLINKS+=sbni.4 if_sbni.4

# Link files to the architecture directory
_ARCH_SUBDIR=i386
.for _manpage _link in ${MLINKS}
MLINKS+=${_link} ${_ARCH_SUBDIR}/${_link}
.endfor
.for _manpage in ${MAN}
MLINKS+=${_manpage} ${_ARCH_SUBDIR}/${_manpage}
.endfor

.include <bsd.prog.mk>
