#
# $FreeBSD$
#

_ALL!=	cd ${.CURDIR} && ls Doxyfile-*
ALL=	${_ALL:C/Doxyfile-//g}
PDF_ALL=${_ALL:C/Doxyfile/pdf/g}

TARGET_ARCH?=	${MACHINE_ARCH}
S?=/usr/src/sys
LOCALBASE?=/usr/local

MFILES+=dev/acpica/acpi_if.m
MFILES+=dev/ata/ata_if.m
MFILES+=dev/eisa/eisa_if.m
MFILES+=dev/iicbus/iicbb_if.m
MFILES+=dev/iicbus/iicbus_if.m
MFILES+=dev/mii/miibus_if.m
MFILES+=dev/mmc/mmcbr_if.m
MFILES+=dev/mmc/mmcbus_if.m
MFILES+=dev/ofw/ofw_bus_if.m
MFILES+=dev/pccard/card_if.m
MFILES+=dev/pccard/power_if.m
MFILES+=dev/pci/pci_if.m
MFILES+=dev/pci/pcib_if.m
MFILES+=dev/ppbus/ppbus_if.m
MFILES+=dev/scc/scc_if.m
MFILES+=dev/smbus/smbus_if.m
MFILES+=dev/sound/midi/mpu_if.m
MFILES+=dev/sound/midi/mpufoi_if.m
MFILES+=dev/sound/midi/synth_if.m
MFILES+=dev/sound/pcm/ac97_if.m
MFILES+=dev/sound/pcm/channel_if.m
MFILES+=dev/sound/pcm/feeder_if.m
MFILES+=dev/sound/pcm/mixer_if.m
MFILES+=dev/spibus/spibus_if.m
MFILES+=dev/uart/uart_if.m
MFILES+=dev/usb/usb_if.m
MFILES+=geom/part/g_part_if.m
MFILES+=isa/isa_if.m
MFILES+=kern/bus_if.m
MFILES+=kern/clock_if.m
MFILES+=kern/cpufreq_if.m
MFILES+=kern/device_if.m
MFILES+=kern/linker_if.m
MFILES+=kern/serdev_if.m
MFILES+=libkern/iconv_converter_if.m
MFILES+=opencrypto/cryptodev_if.m
MFILES+=pc98/pc98/canbus_if.m
MFILES+=pci/agp_if.m
MFILES+=powerpc/powerpc/mmu_if.m
MFILES+=powerpc/powerpc/pic_if.m
MFILES+=sparc64/pci/ofw_pci_if.m
MFILES+=sun4v/mdesc/mdesc_bus_if.m

HFILES=	${MFILES:T:S/.m$/.h/}
AWK?=	awk

.MAIN:	usage

usage:
	@echo "Possible targets are:"
.for entry in ${ALL}
	@echo "	${entry}"
	@echo "	pdf-${entry}"
	@echo "	clean-${entry}"
.endfor
	@echo
	@echo "	all"
	@echo "	pdf-all"
	@echo "	clean"

all:	${ALL}
pdf-all:${PDF_ALL}

mfiles: ${HFILES:S/^/${.OBJDIR}\//}

DOXYGEN_DEST_PATH=	${.OBJDIR}
DOXYGEN_LATEX_DEST_PATH=${.OBJDIR}
DOXYGEN_PDF_DEST_PATH=	${.OBJDIR}

.if exists{${S}/${TARGET_ARCH}/linux}
DOXYGEN_LINUX_PATH=	${S}/${TARGET_ARCH}/linux
.endif
.if exists{${S}/${TARGET_ARCH}/linux32}
DOXYGEN_LINUX_PATH+=	${S}/${TARGET_ARCH}/linux32
.endif

#
# generate the necessary targets
#
.for target in ${ALL}
${target}:	mfiles ${.OBJDIR}/${target}/${target}.tag

${.OBJDIR}/${target}/${target}.tag:
	@mkdir -p ${.OBJDIR}/${target}
	@cd ${.OBJDIR} && \
		env DOXYGEN_INCLUDE_PATH=${.CURDIR} \
		    DOXYGEN_SRC_PATH=${S}  \
		    DOXYGEN_DEST_PATH=${DOXYGEN_DEST_PATH} \
		    DOXYGEN_SRC_INCLUDE_PATH="${S}/sys ${S}/../include ${S}/${TARGET_ARCH}/include" \
		    DOXYGEN_TARGET_ARCH=${TARGET_ARCH} \
		    DOXYGEN_LINUX_PATH=${DOXYGEN_LINUX_PATH} \
		    NOTREVIEWED=${.CURDIR}/notreviewed.dox \
		    PATH=${LOCALBASE}/bin:${PATH} \
			doxygen ${.CURDIR}/Doxyfile-${target}
	@echo "API docs for ${target} are now available in ${.OBJDIR}/${target}/." | /usr/bin/fmt

pdf-${target}:	${.OBJDIR}/${target}/${target}.tag
	@cd ${DOXYGEN_LATEX_DEST_PATH}/${target}/latex && ${MAKE} refman.pdf && cp refman.pdf ${DOXYGEN_PDF_DEST_PATH}/${target}.pdf
	@echo "API docs for ${target} are now available in ${DOXYGEN_PDF_DEST_PATH}/." | /usr/bin/fmt

CLEANDIRS+=	${DOXYGEN_DEST_PATH}/${target}
CLEANDIRS+=	${.OBJDIR}/${target}/
CLEANFILES+=	${DOXYGEN_PDF_DEST_PATH}/${target}.pdf
clean-${target}:
	rm -rf ${DOXYGEN_DEST_PATH}/${target} ${.OBJDIR}/${target}
.endfor

.for file in ${MFILES}
CLEANDIRS+=     ${.OBJDIR}/${file:T:S/.m$/.h/}
${.OBJDIR}/${file:T:S/.m$/.h/}: ${S}/${file}
	cd ${.OBJDIR}; ${AWK} -f $S/tools/makeobjops.awk ${S}/${file} -h
.endfor

#
# update target for the config files {config syntax change}
#
update-doxys:
.for config_file in ${_ALL}
	@doxygen -u ${config_file}
.endfor

.include "Dependencies"
.include <bsd.obj.mk>
