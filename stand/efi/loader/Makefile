LOADER_NET_SUPPORT?=	yes
LOADER_MSDOS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no

.include <bsd.init.mk>

.if ${MACHINE} == "amd64" && ${DO32:U0} == 1
__arch=	i386
LOADER?=	loader_ia32
.else
__arch=	${MACHINE}
LOADER?=	loader_${LOADER_INTERP}
.endif
PROG=		${LOADER}.sym
INTERNALPROG=
WARNS?=		3

# architecture-specific loader code
SRCS=	autoload.c \
	bootinfo.c \
	conf.c \
	copy.c \
	efi_main.c \
	framebuffer.c \
	main.c \
	self_reloc.c \
	vers.c \
	gfx_fb.c \
	8x16.c

# ACPICA Loader Initialization
SRCS+= acpi_detect.c
.PATH: ${.CURDIR}/../acpica

.if ${MACHINE_ARCH} == "amd64"
SRCS+= OsdMemory.c osunixxf.c

.PATH: ${.CURDIR}/../acpica/amd64/Osd/
.PATH: ${.CURDIR}/../contrib/dev/acpica/os_specific/service_layers

CFLAGS+=	-DACPI_SINGLE_THREADED \
		-DACPI_USE_NATIVE_RSDP_POINTER \
		-DACPI_USE_NATIVE_MEMORY_MAPPING \
		-DACPI_REDUCED_HARDWARE \
		-DACPI_DEBUG_OUTPUT

SRCS+=	libefi.c utinit.c utglobal.c utresrc.c utlock.c utownerid.c \
	utcopy.c uthex.c utmath.c utstrsuppt.c utstrtoul64.c utpredef.c \
	uterror.c uteval.c utids.c utbuffer.c utmisc.c utalloc.c utxferror.c \
	utexcep.c utaddress.c utascii.c utcache.c utcksum.c utdebug.c \
	utdecode.c utdelete.c utmutex.c utobject.c utosi.c utstring.c \
	utxfinit.c utnonansi.c utstate.c tbdata.c tbxfload.c tbfadt.c \
	tbinstal.c tbprint.c tbutils.c tbxface.c tbxfroot.c tbfind.c \
	nsxfobj.c nsxfeval.c nsinit.c nsparse.c nsload.c nsrepair2.c \
	nsconvert.c nsrepair.c nsprepkg.c nsarguments.c nseval.c nsxfname.c \
	nsdump.c nswalk.c nssearch.c nsnames.c nsutils.c nsaccess.c nsalloc.c \
	nsobject.c nspredef.c dsinit.c dsfield.c dspkginit.c dsopcode.c \
	dsdebug.c dsmethod.c dscontrol.c dsobject.c dsutils.c dswexec.c \
	dswload2.c dswload.c dsmthdat.c dswstate.c dsargs.c dswscope.c \
	exregion.c exfldio.c exserial.c exstorob.c exstoren.c exdebug.c \
	exmutex.c exconcat.c exmisc.c exconfig.c excreate.c exoparg6.c \
	exoparg3.c exoparg2.c exoparg1.c exdump.c exprep.c exresop.c \
	exsystem.c exnames.c exresolv.c exstore.c exfield.c exconvrt.c \
	exresnte.c exutils.c extrace.c hwpci.c hwregs.c hwxface.c hwvalid.c \
	hwacpi.c evmisc.c evregion.c evrgnini.c evxface.c evevent.c evxfevnt.c \
	evhandler.c evxfregn.c psobject.c psloop.c pstree.c pswalk.c psopinfo.c \
	psscope.c psutils.c psargs.c psparse.c psxface.c psopcode.c init_acpi.c

.PATH: ${.CURDIR}/../libefi
.PATH: ${SYSDIR}/contrib/dev/acpica/os_specific/service_layers
.PATH: ${SYSDIR}/contrib/dev/acpica/components/tables
.PATH: ${SYSDIR}/contrib/dev/acpica/components/namespace
.PATH: ${SYSDIR}/contrib/dev/acpica/components/utilities
.PATH: ${SYSDIR}/contrib/dev/acpica/components/dispatcher
.PATH: ${SYSDIR}/contrib/dev/acpica/components/executer
.PATH: ${SYSDIR}/contrib/dev/acpica/components/hardware
.PATH: ${SYSDIR}/contrib/dev/acpica/components/events
.PATH: ${SYSDIR}/contrib/dev/acpica/components/parser
.endif # ACPICA

CFLAGS+=	-I${.CURDIR}/../loader
.if ${MK_LOADER_ZFS} != "no"
CFLAGS+=	-I${ZFSSRC}
CFLAGS+=        -I${SYSDIR}/contrib/openzfs/include
CFLAGS+=        -I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
CFLAGS+=	-DEFI_ZFS_BOOT
HAVE_ZFS=	yes
.endif

CFLAGS.bootinfo.c += -I$(SRCTOP)/sys/teken
CFLAGS.bootinfo.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.framebuffer.c += -I$(SRCTOP)/sys/teken
CFLAGS.framebuffer.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.main.c += -I$(SRCTOP)/sys/teken
CFLAGS.main.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.gfx_fb.c += -I$(SRCTOP)/sys/teken
CFLAGS.gfx_fb.c += -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4
CFLAGS.gfx_fb.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.gfx_fb.c += -DHAVE_MEMCPY -I${SRCTOP}/sys/contrib/zlib

# We implement a slightly non-standard %S in that it always takes a
# CHAR16 that's common in UEFI-land instead of a wchar_t. This only
# seems to matter on arm64 where wchar_t defaults to an int instead
# of a short. There's no good cast to use here so just ignore the
# warnings for now.
CWARNFLAGS.main.c+=	-Wno-format

.PATH: ${.CURDIR}/../loader
.PATH: ${.CURDIR}/../loader/arch/${__arch}
.include "${.CURDIR}/../loader/arch/${__arch}/Makefile.inc"

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/arch/${__arch}
CFLAGS+=	-I${EFISRC}/include
CFLAGS+=	-I${EFISRC}/include/${__arch}
CFLAGS+=	-I${SYSDIR}/contrib/dev/acpica/include
CFLAGS+=	-I${EFISRC}/acpica/include
CFLAGS+=	-I${BOOTSRC}/i386/libi386
CFLAGS+=	-DEFI

.if defined(HAVE_FDT) && ${MK_FDT} != "no"
.include	"${BOOTSRC}/fdt.mk"
LIBEFI_FDT=	${BOOTOBJ}/efi/fdt/libefi_fdt.a
HELP_FILES+=	${FDTSRC}/help.fdt
.endif

# Include bcache code.
HAVE_BCACHE=    yes

.if defined(EFI_STAGING_SIZE)
CFLAGS+=	-DEFI_STAGING_SIZE=${EFI_STAGING_SIZE}
.endif

.if ${MK_LOADER_EFI_SECUREBOOT} != "no"
CFLAGS+= -DEFI_SECUREBOOT
.endif

NEWVERSWHAT?=	"EFI loader" ${MACHINE}
VERSION_FILE?=	${.CURDIR}/../loader/version
HELP_FILENAME=	loader.help.efi

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"

CLEANFILES+=	8x16.c

8x16.c:		${SRCTOP}/contrib/terminus/ter-u16b.bdf
	vtfontcvt -f compressed-source -o ${.TARGET} ${.ALLSRC}

FILES+=	${LOADER}.efi
FILESMODE_${LOADER}.efi=	${BINMODE}

.if ${LOADER_INTERP} == ${LOADER_DEFAULT_INTERP} && ${__arch} != "i386"
LINKS+=		${BINDIR}/${LOADER}.efi ${BINDIR}/loader.efi
.endif

LDSCRIPT=	${.CURDIR}/../loader/arch/${__arch}/${__arch}.ldscript
LDFLAGS+=	-Wl,-T${LDSCRIPT},-Bsymbolic,-znotext -pie
.if ${LINKER_TYPE} == "bfd" && ${LINKER_VERSION} >= 23400
LDFLAGS+=	-Wl,--no-dynamic-linker
.endif

.include <bsd.linker.mk>

.if ${LINKER_TYPE} == "lld" && ${LINKER_FREEBSD_VERSION} < 1500001
# When lld is using multiple threads, which it does by default, it can
# result in non-reproducible output with the custom linker script. Work
# around this by disabling threading.
LDFLAGS+=	-Wl,--threads=1
.endif

CLEANFILES+=	${LOADER}.efi

${LOADER}.efi: ${PROG}
	@if ${NM} ${.ALLSRC} | grep ' U '; then \
		echo "Undefined symbols in ${.ALLSRC}"; \
		exit 1; \
	fi
	SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH} \
	${EFI_OBJCOPY} -j .peheader -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .rel.dyn \
		-j .rela.dyn -j .reloc -j .eh_frame -j set_Xcommand_set \
		-j set_X${LOADER_INTERP}_compile_set \
		--output-target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

LIBEFI=		${BOOTOBJ}/efi/libefi/libefi.a
LIBEFI32=	${BOOTOBJ}/efi/libefi32/libefi.a

.if ${__arch} == "i386"
DPADD=		${LDR_INTERP32} ${LIBEFI32} ${LIBSA32} ${LDSCRIPT}
LDADD=		${LDR_INTERP32} ${LIBEFI32} ${LIBSA32}
.else
DPADD=		${LDR_INTERP} ${LIBEFI} ${LIBSAFDT} ${LIBEFI_FDT} ${LIBSA} ${LDSCRIPT}
LDADD=		${LDR_INTERP} ${LIBEFI} ${LIBSAFDT} ${LIBEFI_FDT} ${LIBSA}
.endif

.include <bsd.prog.mk>
