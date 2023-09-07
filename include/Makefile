#	@(#)Makefile	8.2 (Berkeley) 1/4/94
#
# Doing a "make install" builds /usr/include.

.include <src.opts.mk>

PACKAGE=runtime
CLEANFILES= osreldate.h version
SUBDIR= arpa protocols rpcsvc rpc xlocale
.if ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=		i386
INCLUDE_SUBDIRS+=	i386
.endif
.if ${MACHINE_CPUARCH} == "aarch64"
SUBDIR+=		arm
INCLUDE_SUBDIRS+=	arm
.endif
SUBDIR_PARALLEL=
INCS=	a.out.h ar.h assert.h bitstring.h byteswap.h \
	complex.h cpio.h _ctype.h ctype.h \
	db.h \
	dirent.h dlfcn.h elf.h elf-hints.h endian.h err.h fmtmsg.h fnmatch.h \
	fstab.h fts.h ftw.h getopt.h glob.h grp.h \
	ieeefp.h ifaddrs.h \
	inttypes.h iso646.h kenv.h langinfo.h libgen.h limits.h link.h \
	locale.h malloc.h malloc_np.h memory.h monetary.h mpool.h mqueue.h \
	ndbm.h netconfig.h \
	netdb.h nl_types.h nlist.h nss.h nsswitch.h paths.h \
	printf.h proc_service.h pthread.h \
	pthread_np.h pwd.h ranlib.h readpassphrase.h regex.h \
	res_update.h resolv.h runetype.h sched.h \
	search.h semaphore.h setjmp.h \
	signal.h spawn.h stab.h stdalign.h stdbool.h stdckdint.h stddef.h \
	stdnoreturn.h stdio.h stdlib.h string.h stringlist.h \
	strings.h sysexits.h tar.h termios.h tgmath.h \
	time.h timeconv.h timers.h ttyent.h \
	uchar.h ulimit.h unistd.h utime.h utmpx.h uuid.h varargs.h \
	wchar.h wctype.h wordexp.h xlocale.h

.PATH: ${SRCTOP}/contrib/libc-vis
INCS+=	vis.h

MHDRS=	float.h floatingpoint.h stdarg.h

PHDRS=	_semaphore.h

LHDRS=	aio.h errno.h fcntl.h linker_set.h poll.h stdatomic.h stdint.h \
	syslog.h ucontext.h

LDIRS=	geom net net80211 netgraph netinet netinet6 \
	netipsec netlink netsmb nfs nfsclient nfsserver sys vm

LSUBDIRS=	dev/acpica dev/agp dev/ciss dev/filemon dev/firewire \
	dev/hwpmc dev/hyperv \
	dev/ic dev/iicbus dev/io dev/mfi dev/mmc \
	dev/ofw dev/pbio dev/pci ${_dev_powermac_nvram} dev/ppbus dev/pwm \
	dev/smbus dev/speaker dev/tcp_log dev/veriexec dev/vkbd dev/wg \
	fs/devfs fs/fdescfs fs/msdosfs fs/nfs fs/nullfs \
	fs/procfs fs/smbfs fs/udf fs/unionfs \
	geom/cache geom/concat geom/eli geom/gate geom/journal geom/label \
	geom/mirror geom/mountver geom/multipath geom/nop \
	geom/raid geom/raid3 geom/shsec geom/stripe geom/union geom/virstor \
	net/altq \
	net/route \
	netgraph/netflow \
	netinet/cc \
	netinet/netdump \
	netinet/tcp_stacks \
	netlink/route \
	security/mac_biba security/mac_bsdextended security/mac_lomac \
	security/mac_mls security/mac_partition \
	security/mac_veriexec \
	sys/disk \
	ufs/ffs ufs/ufs

LSUBSUBDIRS=	dev/mpt/mpilib

.PATH: ${SRCTOP}/sys/dev/acpica
ACPICA=		acpi_hpet.h \
		acpiio.h
ACPICADIR=	${INCLUDEDIR}/dev/acpica

.PATH: ${SRCTOP}/sys/dev/agp
AGP=		agpreg.h
AGPDIR=		${INCLUDEDIR}/dev/agp

.PATH: ${SRCTOP}/sys/bsm
BSM=		audit.h \
		audit_errno.h \
		audit_internal.h \
		audit_record.h \
		audit_domain.h \
		audit_fcntl.h \
		audit_kevents.h \
		audit_socket_type.h
BSMPACKAGE=	libbsm
BSMDIR=		${INCLUDEDIR}/bsm

.PATH: ${SRCTOP}/sys/security/audit
SECAUDIT=	audit.h \
		audit_ioctl.h \
		audit_private.h
SECAUDITPACKAGE=	libbsm
SECAUDITDIR=	${INCLUDEDIR}/security/audit

.PATH:	${SRCTOP}/sys/cam
CAM=	cam.h \
	cam_ccb.h \
	cam_compat.h \
	cam_debug.h \
	cam_iosched.h \
	cam_periph.h \
	cam_queue.h \
	cam_sim.h \
	cam_xpt.h \
	cam_xpt_internal.h \
	cam_xpt_periph.h \
	cam_xpt_sim.h
CAMDIR=	${INCLUDEDIR}/cam

.PATH: ${SRCTOP}/sys/cam/ata
CAMATA=		ata_all.h
CAMATADIR=	${INCLUDEDIR}/cam/ata

.PATH: ${SRCTOP}/sys/cam/mmc
CAMMMC=		mmc.h \
		mmc_bus.h \
		mmc_all.h
CAMMMCDIR=	${INCLUDEDIR}/cam/mmc

.PATH: ${SRCTOP}/sys/cam/nvme
CAMNVME=	nvme_all.h
CAMNVMEDIR=	${INCLUDEDIR}/cam/nvme

.PATH: ${SRCTOP}/sys/cam/scsi
CAMSCSI=	scsi_all.h \
		scsi_cd.h \
		scsi_ch.h \
		scsi_da.h \
		scsi_enc.h \
		scsi_enc_internal.h \
		scsi_iu.h \
		scsi_message.h \
		scsi_pass.h \
		scsi_pt.h \
		scsi_sa.h \
		scsi_ses.h \
		scsi_sg.h \
		scsi_targetio.h \
		smp_all.h
CAMSCSIDIR=	${INCLUDEDIR}/cam/scsi

.PATH: ${SRCTOP}/sys/fs/cd9660
FS9660=		cd9660_mount.h \
		cd9660_node.h \
		cd9660_rrip.h \
		iso.h \
		iso_rrip.h
FS9660DIR=	${INCLUDEDIR}/isofs/cd9660

.PATH: ${SRCTOP}/sys/dev/evdev
EVDEV=		input.h \
		input-event-codes.h \
		uinput.h
EVDEVDIR=	${INCLUDEDIR}/dev/evdev

.PATH: ${SRCTOP}/sys/dev/hid
HID=		hid.h \
		hidraw.h
HIDDIR=		${INCLUDEDIR}/dev/hid

.PATH: ${SRCTOP}/sys/dev/hyperv/include ${SRCTOP}/sys/dev/hyperv/utilities
HYPERV=		hv_snapshot.h \
		hyperv.h
HYPERVDIR=	${INCLUDEDIR}/dev/hyperv

.PATH: ${SRCTOP}/sys/opencrypto
OPENCRYPTO=	cryptodev.h
OPENCRYPTODIR=	${INCLUDEDIR}/crypto

.PATH: ${SRCTOP}/sys/dev/nvme
NVME=		nvme.h
NVMEDIR=	${INCLUDEDIR}/dev/nvme

.PATH: ${SRCTOP}/sys/dev/pci
PCI=		pcireg.h
PCIDIR=		${INCLUDEDIR}/dev/pci

.PATH: ${SRCTOP}/sys/dev/veriexec
VERIEXEC=	veriexec_ioctl.h
VERIEXECDIR=	${INCLUDEDIR}/dev/veriexec

.PATH: ${SRCTOP}/sys/netpfil/ipfilter/netinet
IPFILTER=	ip_auth.h \
		ip_compat.h \
		ip_dstlist.h \
		ip_fil.h \
		ip_frag.h \
		ip_htable.h \
		ip_lookup.h \
		ip_nat.h \
		ip_pool.h \
		ip_proxy.h \
		ip_rules.h \
		ip_scan.h \
		ip_state.h \
		ip_sync.h \
		ipf_rb.h \
		ipl.h \
		radix_ipf.h
IPFILTERDIR=	${INCLUDEDIR}/netinet

.PATH: ${SRCTOP}/sys/netpfil/pf
PF=		pf.h \
		pf_altq.h \
		pf_mtag.h
PFPACKAGE=	pf
PFDIR=	${INCLUDEDIR}/netpfil/pf

.PATH: ${SRCTOP}/sys/rpc
RPC=		rpcsec_tls.h \
		types.h
RPCDIR=		${INCLUDEDIR}/rpc

.PATH: ${SRCTOP}/sys/teken
TEKEN=		teken.h
TEKENDIR=	${INCLUDEDIR}/teken

.PATH: ${SRCTOP}/sys/contrib/openzfs/include/sys
NVPAIR=		nvpair.h
NVPAIRDIR=	${INCLUDEDIR}/sys

.PATH: ${SRCTOP}/sys/dev/mlx5
MLX5=		mlx5io.h
MLX5DIR=	${INCLUDEDIR}/dev/mlx5

.PATH: ${SRCTOP}/sys/dev/wg
WG=		if_wg.h
WGDIR=		${INCLUDEDIR}/dev/wg

INCSGROUPS=	INCS \
		ACPICA \
		AGP \
		BSM \
		CAM \
		CAMATA \
		CAMMMC \
		CAMNVME \
		CAMSCSI \
		CRYPTO \
		EVDEV \
		FS9660 \
		HID \
		HYPERV \
		OPENCRYPTO \
		NVME \
		PCI \
		RPC \
		SECAUDIT \
		TEKEN \
		VERIEXEC \
		WG

.if ${MK_IPFILTER} != "no"
INCSGROUPS+=	IPFILTER
.endif

.if ${MK_PF} != "no"
INCSGROUPS+=	PF
.endif

.if ${MK_CDDL} != "no"
INCSGROUPS+=	NVPAIR
.endif

.if ${MK_MLX5TOOL} != "no"
INCSGROUPS+=	MLX5
.endif

.if ${MK_BLUETOOTH} != "no"
LSUBSUBDIRS+=	netgraph/bluetooth/include
.endif

.if ${MK_CUSE} != "no"
LSUBDIRS+=	fs/cuse
.endif

.if ${MK_GSSAPI} != "no"
SUBDIR+=	gssapi
INCS+=		gssapi.h
.endif

.if ${MK_HESIOD} != "no"
INCS+=	hesiod.h
.endif

# Handle the #define aliases for libiconv
.if ${MK_ICONV} == "yes"
INCS+=		iconv.h
.endif

.if ${MK_USB} != "no"
LSUBDIRS+=	dev/usb
.endif

.if ${MACHINE_CPUARCH} == "powerpc" && ${MACHINE_ARCH} != "powerpcspe"
_dev_powermac_nvram=	dev/powermac_nvram
.endif

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies'').  ``symlinks'' is
# probably only useful for developers and should be avoided if you do not
# wish to tie your /usr/include and /usr/src together.
#SHARED=	symlinks
SHARED?=	copies

INCS+=	osreldate.h

SYSDIR=			${SRCTOP}/sys
NEWVERS_SH=		${SYSDIR}/conf/newvers.sh
PARAM_H=		${SYSDIR}/sys/param.h
MK_OSRELDATE_SH=	${.CURDIR}/mk-osreldate.sh

SYMLINKS+= ${LIBDIR:C,[^/]+,..,g:C,^/,,}${INCLUDEDIR} ${LIBDIR}/include

osreldate.h: ${NEWVERS_SH} ${PARAM_H} ${MK_OSRELDATE_SH}
	env NEWVERS_SH=${NEWVERS_SH} PARAMFILE=${PARAM_H} SYSDIR=${SYSDIR} \
	    sh ${MK_OSRELDATE_SH}

.for i in ${LHDRS}
INCSLINKS+=	sys/$i ${INCLUDEDIR}/$i
.endfor
.for i in ${MHDRS}
INCSLINKS+=	machine/$i ${INCLUDEDIR}/$i
.endfor
.for i in ${PHDRS}
INCSLINKS+=	sys/$i ${INCLUDEDIR}/$i
.endfor

.if ${MACHINE} != ${MACHINE_CPUARCH}
_MARCHS=	${MACHINE_CPUARCH}
.endif
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_MARCHS+=	x86
.endif

META_TARGETS+=	compat
stage_includes: ${SHARED}
SDESTDIR=	${SYSROOT:U${DESTDIR}}

# Take care of stale directory-level symlinks.
# Note: The "|| true" after find is needed in case one of the directories does
# not exist (yet).
compat:
	cd ${SDESTDIR}${INCLUDEDIR}; find ${LDIRS} ${LSUBDIRS} machine ${_MARCHS} \
	    crypto -maxdepth 0 -mindepth 0 -type l -print -delete || true
	mtree -deU ${NO_ROOT:D-W} ${MTREE_FOLLOWS_SYMLINKS} \
	    -f ${SRCTOP}/etc/mtree/BSD.include.dist \
	    -p ${SDESTDIR}${INCLUDEDIR} > /dev/null
.for d in ${INCLUDE_SUBDIRS}
	${INSTALL} -d ${TAG_ARGS} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${SDESTDIR}${INCLUDEDIR}/${d}
.endfor

copies: .PHONY .META
	cd ${SDESTDIR}${INCLUDEDIR}; find ${LDIRS} ${LSUBDIRS} ${LSUBSUBDIRS} crypto \
	    machine machine/pc ${_MARCHS} -maxdepth 1 -mindepth 1 -type l \
	    -name "*.h" -print -delete || true
.for i in ${LDIRS} ${LSUBDIRS:Ndev/agp:Ndev/acpica:Ndev/evdev:Ndev/hid:Ndev/hyperv:Ndev/pci:Ndev/veriexec:Ndev/wg} ${LSUBSUBDIRS}
	cd ${SRCTOP}/sys; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 444 $i/*.h \
	    ${SDESTDIR}${INCLUDEDIR}/$i
.endfor
	cd ${SRCTOP}/sys/${MACHINE}/include; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${SDESTDIR}${INCLUDEDIR}/machine
.if exists(${SRCTOP}/sys/${MACHINE}/include/pc)
	cd ${SRCTOP}/sys/${MACHINE}/include/pc; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${SDESTDIR}${INCLUDEDIR}/machine/pc
.endif
.for _MARCH in ${_MARCHS}
.if exists(${SRCTOP}/sys/${_MARCH}/include)
	${INSTALL} -d ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH}; \
	cd ${SRCTOP}/sys/${_MARCH}/include; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH}
.if exists(${SRCTOP}/sys/${_MARCH}/include/pc)
	${INSTALL} -d ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH}/pc; \
	cd ${SRCTOP}/sys/${_MARCH}/include/pc; \
	${INSTALL} -C ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH}/pc
.endif
.endif
.endfor

symlinks: .PHONY .META
	@${ECHO} "Setting up symlinks to kernel source tree..."
.for i in ${LDIRS}
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../%s ' sys/$i/*.h) ${SDESTDIR}${INCLUDEDIR}/$i
.endfor
.for i in ${LSUBDIRS:Ndev/agp:Ndev/acpica:Ndev/evdev:Ndev/hid:Ndev/hyperv:Ndev/pci:Ndev/veriexec:Ndev/wg}
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../%s ' sys/$i/*.h) ${SDESTDIR}${INCLUDEDIR}/$i
.endfor
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../sys/dev/acpica/%s ' acpiio.h acpi_hpet.h) \
	    ${SDESTDIR}${INCLUDEDIR}/dev/acpica; \
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} ../../../../sys/dev/agp/agpreg.h \
	    ${SDESTDIR}${INCLUDEDIR}/dev/agp; \
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../sys/dev/evdev/%s ' input.h input-event-codes.h uinput.h) \
	    ${SDESTDIR}${INCLUDEDIR}/dev/evdev;
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../sys/dev/hid/%s ' hid.h hidraw.h) \
	    ${SDESTDIR}${INCLUDEDIR}/dev/hid; \
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} ../../../../sys/dev/hyperv/include/hyperv.h \
	    ${SDESTDIR}${INCLUDEDIR}/dev/hyperv; \
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} ../../../../sys/dev/hyperv/utilities/hv_snapshot.h \
	    ${SDESTDIR}${INCLUDEDIR}/dev/hyperv; \
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} ../../../../sys/dev/pci/pcireg.h \
	    ${SDESTDIR}${INCLUDEDIR}/dev/pci; \
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} ../../../../sys/dev/veriexec/veriexec_ioctl.h \
	    ${SDESTDIR}${INCLUDEDIR}/dev/veriexec;
.for i in ${LSUBSUBDIRS}
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../../%s ' sys/$i/*.h) ${SDESTDIR}${INCLUDEDIR}/$i
.endfor
.if ${MK_IPFILTER} != "no"
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../%s ' sys/netpfil/ipfilter/netinet/*.h) \
	    ${SDESTDIR}${INCLUDEDIR}/netinet;
.endif
.if ${MK_PF} != "no"
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../%s ' sys/netpfil/pf/*.h) \
	    ${SDESTDIR}${INCLUDEDIR}/netpfil/pf;
.endif
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} ../../../sys/opencrypto/cryptodev.h \
	    ${SDESTDIR}${INCLUDEDIR}/crypto; \
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../%s ' sys/${MACHINE}/include/*.h) \
	    ${SDESTDIR}${INCLUDEDIR}/machine;
.if exists(${SRCTOP}/sys/${MACHINE}/include/pc)
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../%s ' sys/${MACHINE}/include/pc/*.h) \
	    ${SDESTDIR}${INCLUDEDIR}/machine/pc;
.endif
.for _MARCH in ${_MARCHS}
.if exists(${SRCTOP}/sys/${_MARCH}/include)
	${INSTALL} -d ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH}; \
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../%s ' sys/${_MARCH}/include/*.h) \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH};
.if exists(${SRCTOP}/sys/${_MARCH}/include/pc)
	${INSTALL} -d ${TAG_ARGS:D${TAG_ARGS},dev} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH}/pc; \
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../%s ' sys/${_MARCH}/include/pc/*.h) \
	    ${SDESTDIR}${INCLUDEDIR}/${_MARCH}/pc;
.endif
.endif
.endfor
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../../%s ' sys/fs/cd9660/*.h) \
	    ${SDESTDIR}${INCLUDEDIR}/isofs/cd9660; \
	cd ${SRCTOP}; ${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    $$(printf '../../../sys/rpc/%s ' rpcsec_tls.h types.h) \
	    ${SDESTDIR}${INCLUDEDIR}/rpc;
	cd ${SRCTOP}/sys/rpc;
.if ${MK_CDDL} != "no"
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} \
	    ../../../sys/contrib/openzfs/include/sys/nvpair.h \
	    ${SDESTDIR}${INCLUDEDIR}/sys
.endif
.if ${MK_MLX5TOOL} != "no"
	${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},dev} ../../../../sys/dev/mlx5/mlx5io.h \
	    ${SDESTDIR}${INCLUDEDIR}/dev/mlx5
.endif

.include <bsd.prog.mk>

installincludes: ${SHARED}
${SHARED}: compat

.if ${MACHINE} == "host" && !defined(_SKIP_BUILD)
# we're here because we are building a sysroot...
# we need MACHINE et al set correctly
HOST_MACHINE!= uname -m
HOST_MACHINE_ARCH!= uname -p
MACHINE:= ${HOST_MACHINE}
MACHINE_ARCH:= ${HOST_MACHINE_ARCH}
.endif

check-ldirs:
	missing_dirs=""; \
	for dir in ${LDIRS} ${LSUBDIRS} ${LSUBSUBDIRS}; do \
		if [ ! -d ${SRCTOP}/sys/$$dir ]; then \
			echo "${SRCTOP}/sys/$$dir"; \
			missing_dirs="$$missing_dirs $$dir"; \
		fi; \
	done; \
	if [ -n "$$missing_dirs" ]; then \
		echo "Nonexistant directories:" $$missing_dirs; \
		false; \
	fi
