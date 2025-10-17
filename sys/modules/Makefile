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
	accf_tls \
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
	${_amdsmu} \
	${_amdsbwd} \
	${_amdsmn} \
	${_amdtemp} \
	${_aout} \
	${_arcmsr} \
	${_allwinner} \
	${_armv8crypto} \
	${_armv8_rng} \
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
	${_bnxt} \
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
	${_cfi} \
	${_chromebook_platform} \
	${_ciss} \
	${_coretemp} \
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
	${_dpaa2} \
	${_dpdk_lpm4} \
	${_dpdk_lpm6} \
	${_dpms} \
	dummymbuf \
	dummynet \
	${_dwwdt} \
	e6000sw \
	${_efirt} \
	${_em} \
	${_ena} \
	${_enic} \
	${_enetc} \
	${_et} \
	etherswitch \
	evdev \
	${_exca} \
	ext2fs \
	fdc \
	fdescfs \
	${_felix} \
	${_ffec} \
	${_fib_dxr} \
	filemon \
	firewire \
	firmware \
	flash \
	${_ftgpio} \
	${_ftwd} \
	fusefs \
	${_fxp} \
	gem \
	geom \
	${_glxiic} \
	${_glxsb} \
	gpio \
	${_gve} \
	hid \
	hifn \
	${_hpt27xx} \
	${_hptiop} \
	${_hptmv} \
	${_hptnr} \
	${_hptrr} \
	hwpmc \
	${_hwt} \
	${_hyperv} \
	i2c \
	${_iavf} \
        ${_ibcore} \
	${_ichwd} \
	${_ice} \
	${_ice_ddp} \
	${_irdma} \
	${_ida} \
	if_bridge \
	${_if_cgem} \
	if_disc \
	if_edsc \
	${_if_enc} \
	if_epair \
	${_genet} \
	${_if_gif} \
	${_if_gre} \
	${_if_me} \
	if_infiniband \
	if_lagg \
	if_ovpn \
	${_if_stf} \
	if_tuntap \
	if_vlan \
	if_vxlan \
	${_if_wg} \
	iflib \
	${_igc} \
	imgact_binmisc \
	${_intelspi} \
	${_io} \
	${_ioat} \
        ${_ipoib} \
	ipdivert \
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
	${_iwlwifi} \
	${_iwm} \
	${_iwn} \
	${_iwnfw} \
	${_iwx} \
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
	le \
	lge \
	libalias \
	libiconv \
	libmchain \
	lindebugfs \
	linuxkpi \
	linuxkpi_hdmi \
	linuxkpi_video \
	linuxkpi_wlan \
	${_lio} \
	lpt \
	${_mac_biba} \
	${_mac_bsdextended} \
	${_mac_ddb} \
	${_mac_do} \
	${_mac_ifoff} \
	${_mac_ipacl} \
	${_mac_lomac} \
	${_mac_mls} \
	${_mac_none} \
	${_mac_ntpd} \
	${_mac_partition} \
	${_mac_pimd} \
	${_mac_portacl} \
	${_mac_priority} \
	${_mac_seeotheruids} \
	${_mac_stub} \
	${_mac_test} \
	${_mac_veriexec} \
	${_mac_veriexec_sha1} \
	${_mac_veriexec_sha256} \
	${_mac_veriexec_sha384} \
	${_mac_veriexec_sha512} \
	${_malo} \
	${_mana} \
	md \
	mdio \
	${_mgb} \
	mem \
	mfi \
	mii \
	miiproxy \
	mlx \
	mlxfw \
	${_mlx4} \
	${_mlx4ib} \
	${_mlx4en} \
	${_mlx5} \
	${_mlx5en} \
	${_mlx5ib} \
	mmc \
	mmcsd \
	${_mpi3mr} \
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
	${_ncthwm} \
	${_neta} \
	netlink \
	${_netgraph} \
	${_nfe} \
	nfscl \
	nfscommon \
	nfsd \
	nfslockd \
	nfssvc \
	nlsysevent \
	nge \
	nmdm \
	nullfs \
	${_ntb} \
	nvd \
	${_nvdimm} \
	nvme \
	nvmf \
	${_nvram} \
	oce \
	${_ocs_fc} \
	${_ossl} \
	otus \
	${_otusfw} \
	ow \
	p9fs \
	${_padlock} \
	${_padlock_rng} \
	${_pchtherm} \
	${_pcfclock} \
	${_pf} \
	${_pflog} \
	${_pflow} \
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
	${_pt} \
	pty  \
	puc \
	pwm \
	${_qat} \
	${_qatfw} \
	${_qat_c2xxx} \
	${_qat_c2xxxfw} \
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
	${_rdseed_rng} \
	re \
	rl \
	${_rockchip} \
	rtsx \
	${_rtw88} \
	${_rtw89} \
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
	${_sdhci_fdt} \
	sdhci_pci \
	sdio \
	${_sff} \
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
	ste \
	stge \
	${_sume} \
	${_superio} \
	${_p2sb} \
	sym \
	${_syscons} \
	sysvipc \
	tarfs \
	tcp \
	${_ti} \
	tmpfs \
	${_toecore} \
	tpm \
	tws \
	uart \
	udf \
	udf_iconv \
	ufs \
	${_ufshci} \
	uinput \
	unionfs \
	usb \
	${_vesa} \
	${_vf_i2c} \
	virtio \
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
	${_wdatwd} \
	wlan \
	wlan_acl \
	wlan_amrr \
	wlan_ccmp \
	wlan_gcmp \
	wlan_rssadapt \
	wlan_tkip \
	wlan_wep \
	wlan_xauth \
	${_wpi} \
	${_wpifw} \
	${_wtap} \
	${_x86bios} \
	xdr \
	xl \
	xz \
	zlib

.if ${MK_AUTOFS} != "no" || defined(ALL_MODULES)
_autofs=	autofs
.endif

.if ${MK_DTRACE} != "no" || defined(ALL_MODULES)
.if ${KERN_OPTS:MKDTRACE_HOOKS}
SUBDIR+=	dtrace
.endif
SUBDIR+=	opensolaris
.endif

.if !${MACHINE_ABI:Mlong32}
_bnxt=		bnxt
.endif

.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
.if exists(${SRCTOP}/sys/opencrypto)
_crypto=	crypto
_cryptodev=	cryptodev
_random_fortuna=random_fortuna
_random_other=	random_other
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
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_if_wg=		if_wg
.endif
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

.if (${KERN_OPTS:MINET} && ${MK_INET_SUPPORT} != "no") || defined(ALL_MODULES)
_if_me=		if_me
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

.if ${MK_INET_SUPPORT} != "no" && ${KERN_OPTS:MFIB_ALGO} && ${KERN_OPTS:MINET}
_dpdk_lpm4=	dpdk_lpm4
_fib_dxr=	fib_dxr
.endif

.if ${MK_INET6_SUPPORT} != "no" && ${KERN_OPTS:MFIB_ALGO}
_dpdk_lpm6=	dpdk_lpm6
.endif

.if ${MK_ISCSI} != "no" || defined(ALL_MODULES)
SUBDIR+=	cfiscsi
SUBDIR+=	iscsi
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

.if ${MACHINE_CPUARCH} != "arm"
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_ibcore=	ibcore
_ipoib=		ipoib
_iser=		iser
_mthca=		mthca
_rdma=		rdma
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386" || ${MACHINE_ARCH:Mpowerpc64*} != ""
_ipmi=		ipmi
_mlx4=		mlx4
_mlx5=		mlx5
.if (${MK_INET_SUPPORT} != "no" && ${MK_INET6_SUPPORT} != "no") || \
	defined(ALL_MODULES)
_mlx4en=	mlx4en
_mlx5en=	mlx5en
.endif
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_mlx4ib=	mlx4ib
_mlx5ib=	mlx5ib
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
_ena=		ena
_gve=		gve
_iwlwifi=	iwlwifi
_rtw88=		rtw88
_rtw89=		rtw89
_vmware=	vmware
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386" || ${MACHINE_ARCH} == "armv7" || \
	${MACHINE_ARCH:Mpowerpc64*} != ""
_ossl=		ossl
.endif

# MAC framework
.if ${KERN_OPTS:MMAC} || defined(ALL_MODULES)
_mac_biba=	mac_biba
_mac_bsdextended= mac_bsdextended
.if ${KERN_OPTS:MDDB} || defined(ALL_MODULES)
_mac_ddb=	mac_ddb
.endif
_mac_do=	mac_do
_mac_ifoff=	mac_ifoff
_mac_ipacl=	mac_ipacl
_mac_lomac=	mac_lomac
_mac_mls=	mac_mls
_mac_none=	mac_none
_mac_ntpd=	mac_ntpd
_mac_partition=	mac_partition
_mac_pimd=	mac_pimd
_mac_portacl=	mac_portacl
_mac_priority=	mac_priority
_mac_seeotheruids= mac_seeotheruids
_mac_stub=	mac_stub
_mac_test=	mac_test
.if ${MK_VERIEXEC} != "no" || defined(ALL_MODULES)
_mac_veriexec=	mac_veriexec
_mac_veriexec_sha1= mac_veriexec_sha1
_mac_veriexec_sha256= mac_veriexec_sha256
_mac_veriexec_sha384= mac_veriexec_sha384
_mac_veriexec_sha512= mac_veriexec_sha512
.endif
.endif

.if ${MK_NETGRAPH} != "no" || defined(ALL_MODULES)
_netgraph=	netgraph
.endif

.if (${MK_PF} != "no" && (${MK_INET_SUPPORT} != "no" || \
	${MK_INET6_SUPPORT} != "no")) || defined(ALL_MODULES)
_pf=		pf
_pflog=		pflog
_pflow=		pflow
.if ${MK_INET_SUPPORT} != "no"
_pfsync=	pfsync
.endif
.endif

.if ${MK_SOURCELESS_UCODE} != "no"
_bce=		bce
_fxp=		fxp
_ispfw=		ispfw
_ti=		ti
_mwlfw=		mwlfw
_otusfw=	otusfw
_ralfw=		ralfw
_rtwnfw=	rtwnfw
.endif

.if ${MK_SOURCELESS_UCODE} != "no" && ${MACHINE_CPUARCH} != "arm" && \
	${MACHINE_ARCH} != "powerpc" && ${MACHINE_ARCH} != "powerpcspe" && \
	${MACHINE_CPUARCH} != "riscv"
_cxgbe=		cxgbe
.endif

# This has only been tested on amd64 and arm64
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "aarch64"
_mpi3mr=mpi3mr
.endif

# Specific to the Raspberry Pi.
.if ${MACHINE_CPUARCH} == "aarch64"
_genet=		genet
.endif

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "aarch64" || \
	${MACHINE_ARCH:Mpowerpc64*}
_ice=		ice
.if ${MK_SOURCELESS_UCODE} != "no"
_ice_ddp=	ice_ddp
.endif
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
.if ${MK_INET_SUPPORT} != "no" && ${MK_INET6_SUPPORT} != "no"
_irdma=		irdma
.endif
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm" || \
	${MACHINE_CPUARCH} == "riscv"
.if !empty(OPT_FDT)
_if_cgem=	if_cgem
.endif
.endif

# These rely on 64bit atomics
.if ${MACHINE_ARCH} != "powerpc" && ${MACHINE_ARCH} != "powerpcspe"
_mps=		mps
_mpr=		mpr
.endif

.if ${MK_TESTS} != "no" || defined(ALL_MODULES)
SUBDIR+=	ktest
SUBDIR+=	tests
.endif

.if ${MK_ZFS} != "no" || defined(ALL_MODULES)
SUBDIR+=	zfs
.endif

.if ${MK_SOURCELESS_UCODE} != "no"
_cxgb=		cxgb
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
_armv8crypto=	armv8crypto
_armv8_rng=	armv8_rng
_dpaa2=		dpaa2
_sff=		sff
_em=		em
_hyperv=  hyperv
_vf_i2c=	vf_i2c

.if !empty(OPT_FDT)
_allwinner=	allwinner
_dwwdt=		dwwdt
_enetc=		enetc
_felix=		felix
_rockchip=	rockchip
.endif
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm" || \
    ${MACHINE_CPUARCH} == "riscv"
.if !empty(OPT_FDT)
_sdhci_fdt=	sdhci_fdt
.endif
_neta=		neta
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_agp=		agp
.if ${MACHINE_CPUARCH} == "i386" || !empty(COMPAT_FREEBSD32_ENABLED)
_aout=		aout
.endif
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
_ftgpio=	ftgpio
_ftwd=		ftwd
_exca=		exca
_igc=		igc
_io=		io
_itwd=		itwd
_ix=		ix
_ixv=		ixv
.if ${MK_SOURCELESS_UCODE} != "no"
_lio=		lio
.endif
_mana=		mana
_mgb=		mgb
_nctgpio=	nctgpio
_ncthwm=	ncthwm
_ntb=		ntb
_ocs_fc=	ocs_fc
_p2sb=		p2sb
_qat_c2xxx=	qat_c2xxx
_qat_c2xxxfw=	qat_c2xxxfw
_safe=		safe
_speaker=	speaker
_splash=	splash
_syscons=	syscons
_wbwd=		wbwd
_wdatwd=	wdatwd

_aac=		aac
_aacraid=	aacraid
_acpi=		acpi
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_aesni=		aesni
.endif
_amd_ecc_inject=amd_ecc_inject
_amdsmu=	amdsmu
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
_iwnfw=		iwnfw
.endif
_nfe=		nfe
_nvram=		nvram
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_padlock=	padlock
_padlock_rng=	padlock_rng
_rdrand_rng=	rdrand_rng
_rdseed_rng=	rdseed_rng
.endif
_pchtherm = pchtherm
_s3=		s3
_sdhci_acpi=	sdhci_acpi
_superio=	superio
_vesa=		vesa
_viawd=		viawd
_vmd=		vmd
_wpi=		wpi
.if ${MK_SOURCELESS_UCODE} != "no"
_wpifw=		wpifw
.endif
.if ${KERN_OPTS:MVIMAGE}
_wtap=		wtap
.endif
_x86bios=	x86bios
.endif

.if ${MACHINE_CPUARCH} == "amd64"
_amdgpio=	amdgpio
_ccp=		ccp
_enic=		enic
_iavf=		iavf
_ioat=		ioat
_iwx=		iwx
_ixl=		ixl
_nvdimm=	nvdimm
_pms=		pms
_pt=		pt
_qat=		qat
.if ${MK_SOURCELESS_UCODE} != "no"
_qatfw=		qatfw
.endif
_qlxge=		qlxge
_qlxgb=		qlxgb
_sume=		sume
.if ${MK_SOURCELESS_UCODE} != "no"
_qlxgbe=	qlxgbe
_qlnx=		qlnx
.endif
_sfxge=		sfxge
_sgx=		sgx
_sgx_linux=	sgx_linux
_smartpqi=	smartpqi
_p2sb=		p2sb
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64"
_hwt=		hwt
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "riscv"
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
.endif

.if ${MACHINE_ARCH:Mpowerpc64*} != ""
_ixl=		ixl
_nvram=		opal_nvram
.endif

.if ${MACHINE_CPUARCH} == "powerpc" && ${MACHINE_ARCH} != "powerpcspe"
# Don't build powermac_nvram for powerpcspe, it's never supported.
_nvram+=	powermac_nvram
.endif

.if ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "aarch64"
_bcm283x_clkman=  bcm283x_clkman
_bcm283x_pwm=  bcm283x_pwm
.endif

.if !(${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} < 110000)
# LLVM 10 crashes when building if_malo_pci.c, fixed in LLVM11:
# https://bugs.llvm.org/show_bug.cgi?id=44351
_malo=	malo
.endif

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "aarch64"
_ufshci=ufshci
.endif

SUBDIR+=${MODULES_EXTRA}

.for reject in ${WITHOUT_MODULES}
SUBDIR:= ${SUBDIR:N${reject}}
.endfor

.endif # MODULES_OVERRIDE -- Keep last

# Calling kldxref(8) for each module is expensive.
.if !defined(NO_XREF)
.MAKEFLAGS+=	-DNO_XREF
afterinstall: .PHONY
	${KLDXREF_CMD} ${DESTDIR}${KMODDIR}
.if defined(NO_ROOT) && defined(METALOG)
	echo ".${DISTBASE}${KMODDIR}/linker.hints type=file mode=0644 uname=root gname=wheel" | \
	    cat -l >> ${METALOG}
.endif
.endif

SUBDIR:= ${SUBDIR:u:O}

.include <bsd.subdir.mk>
