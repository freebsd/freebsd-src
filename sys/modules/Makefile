# $FreeBSD$

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

SUBDIR_PARALLEL=

# Modules that include binary-only blobs of microcode should be selectable by
# MK_SOURCELESS_UCODE option (see below).

.include "${SYSDIR}/conf/config.mk"

.if defined(MODULES_OVERRIDE) && !defined(ALL_MODULES)
SUBDIR=${MODULES_OVERRIDE}
.else
SUBDIR=	\
	${_3dfx} \
	${_3dfx_linux} \
	${_aac} \
	${_aacraid} \
	accf_data \
	accf_dns \
	accf_http \
	acl_nfs4 \
	acl_posix1e \
	${_acpi} \
	ae \
	${_aesni} \
	age \
	${_agp} \
	ahci \
	aic7xxx \
	alc \
	ale \
	alq \
	${_amd_ecc_inject} \
	${_amdgpio} \
	${_amdsbwd} \
	${_amdsmn} \
	${_amdtemp} \
	amr \
	${_an} \
	${_aout} \
	${_arcmsr} \
	${_allwinner} \
	${_armv8crypto} \
	${_asmc} \
	ata \
	ath \
	ath_dfs \
	ath_hal \
	ath_hal_ar5210 \
	ath_hal_ar5211 \
	ath_hal_ar5212 \
	ath_hal_ar5416 \
	ath_hal_ar9300 \
	ath_main \
	ath_rate \
	ath_pci \
	${_autofs} \
	axgbe \
	backlight \
	${_bce} \
	${_bcm283x_clkman} \
	${_bcm283x_pwm} \
	bfe \
	bge \
	bhnd \
	${_bxe} \
	${_bios} \
	${_blake2} \
	bnxt \
	bridgestp \
	bwi \
	bwn \
	${_bytgpio} \
	${_chvgpio} \
	cam \
	${_cardbus} \
	${_carp} \
	cas \
	${_cbb} \
	cc \
	${_ccp} \
	cd9660 \
	cd9660_iconv \
	${_ce} \
	${_cfi} \
	${_chromebook_platform} \
	${_ciss} \
	cloudabi \
	${_cloudabi32} \
	${_cloudabi64} \
	${_coretemp} \
	${_cp} \
	${_cpsw} \
	${_cpuctl} \
	${_cpufreq} \
	${_crypto} \
	${_cryptodev} \
	ctl \
	${_cxgb} \
	${_cxgbe} \
	dc \
	dcons \
	dcons_crom \
	${_dpms} \
	dummynet \
	${_efirt} \
	${_em} \
	${_ena} \
	esp \
	${_et} \
	evdev \
	${_exca} \
	ext2fs \
	fdc \
	fdescfs \
	${_ffec} \
	filemon \
	firewire \
	firmware \
	${_ftwd} \
	fusefs \
	${_fxp} \
	gem \
	geom \
	${_glxiic} \
	${_glxsb} \
	gpio \
	hid \
	hifn \
	${_hpt27xx} \
	${_hptiop} \
	${_hptmv} \
	${_hptnr} \
	${_hptrr} \
	hwpmc \
	${_hwpmc_mips24k} \
	${_hwpmc_mips74k} \
	${_hyperv} \
	i2c \
	${_iavf} \
        ${_ibcore} \
	${_ichwd} \
	${_ice} \
	${_ice_ddp} \
	${_ida} \
	if_bridge \
	if_disc \
	if_edsc \
	${_if_enc} \
	if_epair \
	${_if_gif} \
	${_if_gre} \
	${_if_me} \
	if_infiniband \
	if_lagg \
	${_if_ndis} \
	${_if_stf} \
	if_tuntap \
	if_vlan \
	if_vxlan \
	if_wg \
	iflib \
	${_iir} \
	imgact_binmisc \
	${_intelspi} \
	${_io} \
	${_ioat} \
        ${_ipoib} \
	${_ipdivert} \
	${_ipfilter} \
	${_ipfw} \
	ipfw_nat \
	${_ipfw_nat64} \
	${_ipfw_nptv6} \
	${_ipfw_pmod} \
	${_ipmi} \
	ip6_mroute_mod \
	ip_mroute_mod \
	${_ips} \
	${_ipsec} \
	${_ipw} \
	${_ipwfw} \
	${_isci} \
	${_iser} \
	isp \
	${_ispfw} \
	${_itwd} \
	${_iwi} \
	${_iwifw} \
	${_iwm} \
	${_iwmfw} \
	${_iwn} \
	${_iwnfw} \
	${_ix} \
	${_ixv} \
	${_ixl} \
	jme \
	kbdmux \
	kgssapi \
	kgssapi_krb5 \
	khelp \
	krpc \
	ksyms \
	${_ktls_ocf} \
	le \
	lge \
	libalias \
	libiconv \
	libmchain \
	lindebugfs \
	linuxkpi \
	${_lio} \
	lpt \
	mac_biba \
	mac_bsdextended \
	mac_ifoff \
	mac_lomac \
	mac_mls \
	mac_none \
	mac_ntpd \
	mac_partition \
	mac_portacl \
	mac_seeotheruids \
	mac_stub \
	mac_test \
	${_malo} \
	md \
	mdio \
	mem \
	mfi \
	mii \
	mlx \
	mlxfw \
	${_mlx4} \
	${_mlx4ib} \
	${_mlx4en} \
	${_mlx5} \
	${_mlx5en} \
	${_mlx5ib} \
	${_mly} \
	mmc \
	mmcsd \
	${_mpr} \
	${_mps} \
	mpt \
	mqueue \
	mrsas \
	msdosfs \
	msdosfs_iconv \
	msk \
	${_mthca} \
	mvs \
	mwl \
	${_mwlfw} \
	mxge \
	my \
	${_nctgpio} \
	${_ndis} \
	${_netgraph} \
	${_nfe} \
	nfscl \
	nfscommon \
	nfsd \
	nfslockd \
	nfssvc \
	nge \
	nmdm \
	nullfs \
	${_ntb} \
	${_nvd} \
	${_nvdimm} \
	${_nvme} \
	${_nvram} \
	oce \
	${_ocs_fc} \
	${_ossl} \
	otus \
	${_otusfw} \
	ow \
	${_padlock} \
	${_padlock_rng} \
	${_pccard} \
	${_pchtherm} \
	${_pcfclock} \
	${_pf} \
	${_pflog} \
	${_pfsync} \
	plip \
	${_pms} \
	ppbus \
	ppc \
	ppi \
	pps \
	procfs \
	proto \
	pseudofs \
	${_pst} \
	pty  \
	puc \
	pwm \
	${_qat} \
	${_qatfw} \
	${_qlxge} \
	${_qlxgb} \
	${_qlxgbe} \
	${_qlnx} \
	ral \
	${_ralfw} \
	${_random_fortuna} \
	${_random_other} \
	rc4 \
	${_rdma} \
	${_rdrand_rng} \
	re \
	rl \
	${_rockchip} \
	rtsx \
	rtwn \
	rtwn_pci \
	rtwn_usb \
	${_rtwnfw} \
	${_s3} \
	${_safe} \
	safexcel \
	${_sbni} \
	scc \
	${_sctp} \
	sdhci \
	${_sdhci_acpi} \
	sdhci_pci \
	sdio \
	sem \
	send \
	${_sfxge} \
	sge \
	${_sgx} \
	${_sgx_linux} \
	siftr \
	siis \
	sis \
	sk \
	${_smartpqi} \
	smbfs \
	snp \
	sound \
	${_speaker} \
	spi \
	${_splash} \
	${_sppp} \
	ste \
	stge \
	${_sume} \
	${_superio} \
	${_sym} \
	${_syscons} \
	sysvipc \
	tcp \
	${_ti} \
	tmpfs \
	${_toecore} \
	${_tpm} \
	${_twa} \
	twe \
	tws \
	uart \
	udf \
	udf_iconv \
	ufs \
	uinput \
	unionfs \
	usb \
	${_vesa} \
	${_virtio} \
	vge \
	${_viawd} \
	videomode \
	vkbd \
	${_vmd} \
	${_vmm} \
	${_vmware} \
	vr \
	vte \
	${_wbwd} \
	wlan \
	wlan_acl \
	wlan_amrr \
	wlan_ccmp \
	wlan_rssadapt \
	wlan_tkip \
	wlan_wep \
	wlan_xauth \
	${_wpi} \
	${_wpifw} \
	${_x86bios} \
	xdr \
	xl \
	xz \
	zlib

.if ${MK_AUTOFS} != "no" || defined(ALL_MODULES)
_autofs=	autofs
.endif

.if ${MK_CDDL} != "no" || defined(ALL_MODULES)
.if (${MACHINE_CPUARCH} != "arm" || ${MACHINE_ARCH:Marmv[67]*} != "") && \
	${MACHINE_CPUARCH} != "mips"
.if ${KERN_OPTS:MKDTRACE_HOOKS}
SUBDIR+=	dtrace
.endif
.endif
SUBDIR+=	opensolaris
.endif

.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
.if exists(${SRCTOP}/sys/opencrypto)
_crypto=	crypto
_cryptodev=	cryptodev
_random_fortuna=random_fortuna
_random_other=	random_other
_ktls_ocf=	ktls_ocf
.endif
.endif

.if ${MK_CUSE} != "no" || defined(ALL_MODULES)
SUBDIR+=	cuse
.endif

.if ${MK_EFI} != "no"
.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64"
_efirt=		efirt
.endif
.endif

.if (${MK_INET_SUPPORT} != "no" || ${MK_INET6_SUPPORT} != "no") || \
	defined(ALL_MODULES)
_carp=		carp
_toecore=	toecore
_if_enc=	if_enc
_if_gif=	if_gif
_if_gre=	if_gre
_ipfw_pmod=	ipfw_pmod
.if ${KERN_OPTS:MIPSEC_SUPPORT} && !${KERN_OPTS:MIPSEC}
_ipsec=		ipsec
.endif
.if ${KERN_OPTS:MSCTP_SUPPORT} || ${KERN_OPTS:MSCTP}
_sctp=		sctp
.endif
.endif

.if (${MK_INET_SUPPORT} != "no" && ${MK_INET6_SUPPORT} != "no") || \
	defined(ALL_MODULES)
_if_stf=	if_stf
.endif

.if ${MK_INET_SUPPORT} != "no" || defined(ALL_MODULES)
_if_me=		if_me
_ipdivert=	ipdivert
_ipfw=		ipfw
.if ${MK_INET6_SUPPORT} != "no" || defined(ALL_MODULES)
_ipfw_nat64=	ipfw_nat64
.endif
.endif

.if ${MK_INET6_SUPPORT} != "no" || defined(ALL_MODULES)
_ipfw_nptv6=	ipfw_nptv6
.endif

.if ${MK_IPFILTER} != "no" || defined(ALL_MODULES)
_ipfilter=	ipfilter
.endif

.if ${MK_ISCSI} != "no" || defined(ALL_MODULES)
SUBDIR+=	cfiscsi
SUBDIR+=	iscsi
SUBDIR+=	iscsi_initiator
.endif

.if !empty(OPT_FDT)
SUBDIR+=	fdt
.endif

# Linuxulator
.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	linprocfs
SUBDIR+=	linsysfs
.endif
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	linux
.endif
.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	linux64
SUBDIR+=	linux_common
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
_ena=		ena
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_ibcore=	ibcore
_ipoib=		ipoib
_iser=		iser
.endif
_ipmi=		ipmi
_mlx4=		mlx4
_mlx5=		mlx5
.if (${MK_INET_SUPPORT} != "no" && ${MK_INET6_SUPPORT} != "no") || \
	defined(ALL_MODULES)
_mlx4en=	mlx4en
_mlx5en=	mlx5en
.endif
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_mthca=		mthca
_mlx4ib=	mlx4ib
_mlx5ib=	mlx5ib
.endif
_ossl=		ossl
_vmware=	vmware
.endif

.if ${MK_NETGRAPH} != "no" || defined(ALL_MODULES)
_netgraph=	netgraph
.endif

.if (${MK_PF} != "no" && (${MK_INET_SUPPORT} != "no" || \
	${MK_INET6_SUPPORT} != "no")) || defined(ALL_MODULES)
_pf=		pf
_pflog=		pflog
.if ${MK_INET_SUPPORT} != "no"
_pfsync=	pfsync
.endif
.endif

.if ${MK_SOURCELESS_UCODE} != "no"
_bce=		bce
_fxp=		fxp
_ispfw=		ispfw
_ti=		ti

.if ${MACHINE_CPUARCH} != "mips"
_mwlfw=		mwlfw
_otusfw=	otusfw
_ralfw=		ralfw
_rtwnfw=	rtwnfw
.endif
.endif

.if ${MK_SOURCELESS_UCODE} != "no" && ${MACHINE_CPUARCH} != "arm" && \
	${MACHINE_CPUARCH} != "mips" && \
	${MACHINE_ARCH} != "powerpc" && ${MACHINE_ARCH} != "powerpcspe" && \
	${MACHINE_CPUARCH} != "riscv"
_cxgbe=		cxgbe
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "arm64"
_ice=		ice
.if ${MK_SOURCELESS_UCODE} != "no"
_ice_ddp=	ice_ddp
.endif
.endif

# These rely on 64bit atomics
.if ${MACHINE_ARCH} != "powerpc" && ${MACHINE_ARCH} != "powerpcspe" && \
	${MACHINE_CPUARCH} != "mips"
_mps=		mps
_mpr=		mpr
.endif

.if ${MK_TESTS} != "no" || defined(ALL_MODULES)
SUBDIR+=	tests
.endif

.if ${MK_ZFS} != "no" || (defined(ALL_MODULES)  && ${MACHINE_CPUARCH} != "powerpc")
SUBDIR+=	zfs
.endif

.if (${MACHINE_CPUARCH} == "mips" && ${MACHINE_ARCH:Mmips64} == "")
_hwpmc_mips24k=	hwpmc_mips24k
_hwpmc_mips74k=	hwpmc_mips74k
.endif

.if ${MACHINE_CPUARCH} != "aarch64" && ${MACHINE_CPUARCH} != "arm" && \
	${MACHINE_CPUARCH} != "mips" && ${MACHINE_CPUARCH} != "powerpc" && \
	${MACHINE_CPUARCH} != "riscv"
_syscons=	syscons
.endif

.if ${MACHINE_CPUARCH} != "mips"
# no BUS_SPACE_UNSPECIFIED
# No barrier instruction support (specific to this driver)
_sym=		sym
# intr_disable() is a macro, causes problems
.if ${MK_SOURCELESS_UCODE} != "no"
_cxgb=		cxgb
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
_allwinner=	allwinner
_armv8crypto=	armv8crypto
_em=		em
_rockchip=	rockchip
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_agp=		agp
_an=		an
_aout=		aout
_bios=		bios
.if ${MK_SOURCELESS_UCODE} != "no"
_bxe=		bxe
.endif
_cardbus=	cardbus
_cbb=		cbb
_cpuctl=	cpuctl
_cpufreq=	cpufreq
_dpms=		dpms
_em=		em
_et=		et
_ftwd=		ftwd
_exca=		exca
_if_ndis=	if_ndis
_io=		io
_itwd=		itwd
_ix=		ix
_ixv=		ixv
.if ${MK_SOURCELESS_UCODE} != "no"
_lio=		lio
.endif
_nctgpio=	nctgpio
_ndis=		ndis
_ntb=		ntb
_ocs_fc=	ocs_fc
_pccard=	pccard
_qat=		qat
_qatfw=		qatfw
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_rdma=		rdma
.endif
_safe=		safe
_speaker=	speaker
_splash=	splash
_sppp=		sppp
_wbwd=		wbwd

_aac=		aac
_aacraid=	aacraid
_acpi=		acpi
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_aesni=		aesni
.endif
_amd_ecc_inject=amd_ecc_inject
_amdsbwd=	amdsbwd
_amdsmn=	amdsmn
_amdtemp=	amdtemp
_arcmsr=	arcmsr
_asmc=		asmc
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_blake2=	blake2
.endif
_bytgpio=	bytgpio
_chvgpio=	chvgpio
_ciss=		ciss
_chromebook_platform=	chromebook_platform
_coretemp=	coretemp
.if ${MK_SOURCELESS_HOST} != "no" && empty(KCSAN_ENABLED)
_hpt27xx=	hpt27xx
.endif
_hptiop=	hptiop
.if ${MK_SOURCELESS_HOST} != "no" && empty(KCSAN_ENABLED)
_hptmv=		hptmv
_hptnr=		hptnr
_hptrr=		hptrr
.endif
_hyperv=	hyperv
_ichwd=		ichwd
_ida=		ida
_iir=		iir
_intelspi=	intelspi
_ips=		ips
_isci=		isci
_ipw=		ipw
_iwi=		iwi
_iwm=		iwm
_iwn=		iwn
.if ${MK_SOURCELESS_UCODE} != "no"
_ipwfw=		ipwfw
_iwifw=		iwifw
_iwmfw=		iwmfw
_iwnfw=		iwnfw
.endif
_mly=		mly
_nfe=		nfe
_nvd=		nvd
_nvme=		nvme
_nvram=		nvram
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_padlock=	padlock
_padlock_rng=	padlock_rng
_rdrand_rng=	rdrand_rng
.endif
_pchtherm = pchtherm
_s3=		s3
_sdhci_acpi=	sdhci_acpi
_superio=	superio
_tpm=		tpm
_twa=		twa
_vesa=		vesa
_viawd=		viawd
_virtio=	virtio
_wpi=		wpi
.if ${MK_SOURCELESS_UCODE} != "no"
_wpifw=		wpifw
.endif
_x86bios=	x86bios
.endif

.if ${MACHINE_CPUARCH} == "amd64"
_amdgpio=	amdgpio
_ccp=		ccp
_iavf=		iavf
_ioat=		ioat
_ixl=		ixl
_nvdimm=	nvdimm
_pms=		pms
_qlxge=		qlxge
_qlxgb=		qlxgb
_sume=		sume
_vmd=		vmd
.if ${MK_SOURCELESS_UCODE} != "no"
_qlxgbe=	qlxgbe
_qlnx=		qlnx
.endif
_sfxge=		sfxge
_sgx=		sgx
_sgx_linux=	sgx_linux
_smartpqi=	smartpqi

.if ${MK_BHYVE} != "no" || defined(ALL_MODULES)
.if ${KERN_OPTS:MSMP}
_vmm=		vmm
.endif
.endif
.endif

.if ${MACHINE_CPUARCH} == "i386"
# XXX some of these can move to the general case when de-i386'ed
# XXX some of these can move now, but are untested on other architectures.
_3dfx=		3dfx
_3dfx_linux=	3dfx_linux
.if ${MK_SOURCELESS_HOST} != "no"
_ce=		ce
.endif
.if ${MK_SOURCELESS_HOST} != "no"
_cp=		cp
.endif
_glxiic=	glxiic
_glxsb=		glxsb
_pcfclock=	pcfclock
_pst=		pst
_sbni=		sbni
.endif

.if ${MACHINE_ARCH} == "armv7"
_cfi=		cfi
_cpsw=		cpsw
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_aacraid=	aacraid
_agp=		agp
_an=		an
_cardbus=	cardbus
_cbb=		cbb
_cfi=		cfi
_cpufreq=	cpufreq
_exca=		exca
_ffec=		ffec
_nvd=		nvd
_nvme=		nvme
_pccard=	pccard
_virtio=	virtio
.endif

.if ${MACHINE_ARCH:Mpowerpc64*} != ""
_ipmi=		ipmi
_ixl=		ixl
_nvram=		opal_nvram
.endif
.if ${MACHINE_CPUARCH} == "powerpc" && ${MACHINE_ARCH} != "powerpcspe"
# Don't build powermac_nvram for powerpcspe, it's never supported.
_nvram+=	powermac_nvram
.endif

.if (${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
     ${MACHINE_ARCH:Marmv[67]*} != "" || ${MACHINE_CPUARCH} == "i386")
_cloudabi32=	cloudabi32
.endif
.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64"
_cloudabi64=	cloudabi64
.endif

.endif

.if ${MACHINE_ARCH:Marmv[67]*} != "" || ${MACHINE_CPUARCH} == "aarch64"
_bcm283x_clkman=  bcm283x_clkman
_bcm283x_pwm=  bcm283x_pwm
.endif

.if !(${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} < 110000)
# LLVM 10 crashes when building if_malo_pci.c, fixed in LLVM11:
# https://bugs.llvm.org/show_bug.cgi?id=44351
_malo=	malo
.endif

SUBDIR+=${MODULES_EXTRA}

.for reject in ${WITHOUT_MODULES}
SUBDIR:= ${SUBDIR:N${reject}}
.endfor

# Calling kldxref(8) for each module is expensive.
.if !defined(NO_XREF)
.MAKEFLAGS+=	-DNO_XREF
afterinstall: .PHONY
	@if type kldxref >/dev/null 2>&1; then \
		${ECHO} ${KLDXREF_CMD} ${DESTDIR}${KMODDIR}; \
		${KLDXREF_CMD} ${DESTDIR}${KMODDIR}; \
	fi
.endif

SUBDIR:= ${SUBDIR:u:O}

.include <bsd.subdir.mk>
