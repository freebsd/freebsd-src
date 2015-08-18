# $FreeBSD$

SYSDIR?=${.CURDIR}/..
.include "${SYSDIR}/conf/kern.opts.mk"

SUBDIR_PARALLEL=

# Modules that include binary-only blobs of microcode should be selectable by
# MK_SOURCELESS_UCODE option (see below).

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
	aha \
	${_ahb} \
	ahci \
	${_aic} \
	aic7xxx \
	aio \
	alc \
	ale \
	alq \
	${_amdsbwd} \
	${_amdtemp} \
	amr \
	${_an} \
	${_aout} \
	${_apm} \
	${_arcmsr} \
	${_arcnet} \
	${_asmc} \
	ata \
	ath \
	ath_pci \
	${_autofs} \
	${_auxio} \
	${_bce} \
	bfe \
	bge \
	${_bxe} \
	${_bios} \
	${_bktr} \
	${_bm} \
	bridgestp \
	bwi \
	bwn \
	cam \
	${_canbepm} \
	${_canbus} \
	${_cardbus} \
	${_carp} \
	cas \
	${_cbb} \
	cc \
	cd9660 \
	cd9660_iconv \
	${_ce} \
	${_cfi} \
	${_ciss} \
	cloudabi \
	${_cloudabi64} \
	${_cm} \
	${_cmx} \
	${_coff} \
	${_coretemp} \
	${_cp} \
	${_cpsw} \
	${_cpuctl} \
	${_cpufreq} \
	${_crypto} \
	${_cryptodev} \
	${_cs} \
	${_ct} \
	${_ctau} \
	ctl \
	${_cxgb} \
	${_cxgbe} \
	dc \
	dcons \
	dcons_crom \
	de \
	${_dpms} \
	${_dpt} \
	${_drm} \
	${_drm2} \
	${_dtrace} \
	dummynet \
	${_ed} \
	${_elink} \
	${_em} \
	en \
	${_ep} \
	${_epic} \
	esp \
	${_et} \
	${_ex} \
	${_exca} \
	ext2fs \
	${_fatm} \
	fdc \
	fdescfs \
	${_fe} \
	${_filemon} \
	firewire \
	firmware \
	fuse \
	${_fxp} \
	gem \
	geom \
	${_glxiic} \
	${_glxsb} \
	hatm \
	hifn \
	hme \
	${_hpt27xx} \
	${_hptiop} \
	${_hptmv} \
	${_hptnr} \
	${_hptrr} \
	hwpmc \
	${_hyperv} \
	i2c \
        ${_ibcore} \
	${_ibcs2} \
	${_ichwd} \
	${_ida} \
	${_ie} \
	if_bridge \
	if_disc \
	if_edsc \
	if_epair \
	${_if_gif} \
	${_if_gre} \
	${_if_me} \
	if_lagg \
	${_if_ndis} \
	${_if_stf} \
	if_tap \
	if_tun \
	if_vlan \
	if_vxlan \
	${_igb} \
	${_iir} \
	imgact_binmisc \
	${_io} \
        ${_ipoib} \
	${_ipdivert} \
	${_ipfilter} \
	${_ipfw} \
	ipfw_nat \
	${_ipmi} \
	ip6_mroute_mod \
	ip_mroute_mod \
	${_ips} \
	${_ipw} \
	${_ipwfw} \
	${_isci} \
	isp \
	${_ispfw} \
	${_iwi} \
	${_iwifw} \
	${_iwm} \
	${_iwmfw} \
	${_iwn} \
	${_iwnfw} \
	${_ix} \
	${_ixv} \
	${_ixgb} \
	${_ixl} \
	${_ixlv} \
	jme \
	joy \
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
	libmbpool \
	libmchain \
	${_linprocfs} \
	${_linsysfs} \
	${_linux} \
	${_linux_common} \
	${_linux64} \
	${_linuxapi} \
	lmc \
	lpt \
	mac_biba \
	mac_bsdextended \
	mac_ifoff \
	mac_lomac \
	mac_mls \
	mac_none \
	mac_partition \
	mac_portacl \
	mac_seeotheruids \
	mac_stub \
	mac_test \
	malo \
	mcd \
	md \
	mem \
	mfi \
	mii \
	mlx \
	${_mlx4} \
	${_mlx4ib} \
	${_mlxen} \
	${_mly} \
	mmc \
	mmcsd \
	mpr \
	mps \
	mpt \
	mqueue \
	mrsas \
	msdosfs \
	msdosfs_iconv \
	${_mse} \
	msk \
	${_mthca} \
	mvs \
	mwl \
	${_mwlfw} \
	mxge \
	my \
	${_nandfs} \
	${_nandsim} \
	${_ncr} \
	${_ncv} \
	${_ndis} \
	netfpga10g \
	${_netgraph} \
	${_nfe} \
	nfscl \
	nfscommon \
	nfsd \
	nfslock \
	nfslockd \
	nfssvc \
	nge \
	nmdm \
	${_nsp} \
	nullfs \
	${_ntb} \
	${_nvd} \
	${_nvme} \
	${_nvram} \
	${_nxge} \
	${_opensolaris} \
	oce \
	${_padlock} \
	${_padlock_rng} \
	patm \
	${_pccard} \
	${_pcfclock} \
	pcn \
	${_pf} \
	${_pflog} \
	${_pfsync} \
	plip \
	${_pmc} \
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
	${_qlxge} \
	${_qlxgb} \
	${_qlxgbe} \
	ral \
	${_ralfw} \
	${_random_fortuna} \
	${_random_yarrow} \
	${_random_other} \
	rc4 \
	${_rdma} \
	${_rdrand_rng} \
	re \
	reiserfs \
	rl \
	${_s3} \
	${_safe} \
	${_sbni} \
	scc \
	scd \
	${_scsi_low} \
	sdhci \
	sdhci_pci \
	sem \
	send \
	${_sf} \
	${_sfxge} \
	sge \
	${_si} \
	siba_bwn \
	siftr \
	siis \
	sis \
	sk \
	smbfs \
	sn \
	${_snc} \
	snp \
	sound \
	${_speaker} \
	${_splash} \
	${_sppp} \
	ste \
	${_stg} \
	stge \
	${_streams} \
	${_svr4} \
	${_sym} \
	${_syscons} \
	sysvipc \
	${_ti} \
	tl \
	tmpfs \
	${_toecore} \
	${_tpm} \
	trm \
	${_twa} \
	twe \
	tws \
	tx \
	${_txp} \
	uart \
	ubsec \
	udf \
	udf_iconv \
	ufs \
	unionfs \
	usb \
	utopia \
	${_vesa} \
	${_virtio} \
	vge \
	${_viawd} \
	videomode \
	vkbd \
	${_vmm} \
	${_vmware} \
	${_vpo} \
	vr \
	vte \
	vx \
	${_vxge} \
	wb \
	${_wbwd} \
	${_wds} \
	${_wi} \
	${_wl} \
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
	${_xe} \
	xl \
	${_zfs} \
	zlib

.if ${MK_AUTOFS} != "no" || defined(ALL_MODULES)
_autofs=	autofs
.endif

.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
.if exists(${.CURDIR}/../opencrypto)
_crypto=	crypto
_cryptodev=	cryptodev
_random_fortuna=random_fortuna
_random_yarrow=	random_yarrow
_random_other=	random_other
.endif
.endif

.if ${MK_CUSE} != "no" || defined(ALL_MODULES)
SUBDIR+=	cuse
.endif

.if (${MK_INET_SUPPORT} != "no" || ${MK_INET6_SUPPORT} != "no") || \
	defined(ALL_MODULES)
_carp=		carp
_toecore=	toecore
_if_gif=	if_gif
_if_gre=	if_gre
.endif

.if (${MK_INET_SUPPORT} != "no" && ${MK_INET6_SUPPORT} != "no") || \
	defined(ALL_MODULES)
_if_stf=	if_stf
.endif

.if ${MK_INET_SUPPORT} != "no" || defined(ALL_MODULES)
_if_me=		if_me
_ipdivert=	ipdivert
_ipfw=		ipfw
.endif

.if ${MK_IPFILTER} != "no" || defined(ALL_MODULES)
_ipfilter=	ipfilter
.endif

.if ${MK_ISCSI} != "no" || defined(ALL_MODULES)
SUBDIR+=	iscsi
SUBDIR+=	iscsi_initiator
.endif

.if ${MK_NAND} != "no" || defined(ALL_MODULES)
_nandfs=	nandfs
_nandsim=	nandsim
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
_fatm=		fatm
_fxp=		fxp
_ispfw=		ispfw
_mwlfw=		mwlfw
_ralfw=		ralfw
_sf=		sf
_ti=		ti
_txp=		txp
.endif

.if ${MK_SOURCELESS_UCODE} != "no" && ${MACHINE_CPUARCH} != "arm" && \
	${MACHINE_ARCH:C/mips(el)?/mips/} != "mips" && \
	${MACHINE_ARCH} != "powerpc"
_cxgbe=		cxgbe
.endif

.if ${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "mips" && \
	${MACHINE_CPUARCH} != "powerpc"
_syscons=	syscons
_vpo=		vpo
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

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_agp=		agp
_an=		an
_aout=		aout
_bktr=		bktr
_bxe=		bxe
_cardbus=	cardbus
_cbb=		cbb
_cpuctl=	cpuctl
_cpufreq=	cpufreq
_cs=		cs
_dpms=		dpms
_drm=		drm
_drm2=		drm2
.if ${MK_CDDL} != "no" || defined(ALL_MODULES)
_dtrace=	dtrace
.endif
_ed=		ed
_em=		em
_ep=		ep
_et=		et
_exca=		exca
_fe=		fe
_filemon=	filemon
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_ibcore=        ibcore
.endif
_if_ndis=	if_ndis
_igb=		igb
_io=		io
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_ipoib=         ipoib
.endif
_ix=		ix
_ixv=		ixv
_linprocfs=	linprocfs
_linsysfs=	linsysfs
_linux=		linux
.if ${MK_OFED} != "no"
_linuxapi=	linuxapi
.endif
_ndis=		ndis
.if ${MK_CDDL} != "no" || defined(ALL_MODULES)
_opensolaris=	opensolaris
.endif
_pccard=	pccard
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_rdma=		rdma
.endif
_safe=		safe
_scsi_low=	scsi_low
_si=		si
_speaker=	speaker
_splash=	splash
_sppp=		sppp
_vmware=	vmware
_vxge=  	vxge
_wbwd=		wbwd
_wi=		wi
_xe=		xe
.if ${MK_ZFS} != "no" || defined(ALL_MODULES)
_zfs=		zfs
.endif
.if ${MACHINE} != "pc98"
_aac=		aac
_aacraid=	aacraid
_acpi=		acpi
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_aesni=		aesni
.endif
_amdsbwd=	amdsbwd
_amdtemp=	amdtemp
_arcmsr=	arcmsr
_asmc=		asmc
_ciss=		ciss
_cmx=		cmx
_coretemp=	coretemp
.if ${MK_SOURCELESS_HOST} != "no"
_hpt27xx=	hpt27xx
.endif
_hptiop=	hptiop
.if ${MK_SOURCELESS_HOST} != "no"
_hptmv=		hptmv
_hptnr=		hptnr
_hptrr=		hptrr
.endif
_hyperv=	hyperv
_ichwd=		ichwd
_ida=		ida
_iir=		iir
_ipmi=		ipmi
_ips=		ips
_isci=		isci
_ipw=		ipw
_iwi=		iwi
_iwm=		iwm
_iwn=		iwn
_ixgb=		ixgb
.if ${MK_SOURCELESS_UCODE} != "no"
_ipwfw=		ipwfw
_iwifw=		iwifw
_iwmfw=		iwmfw
_iwnfw=		iwnfw
.endif
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_mlx4=		mlx4
_mlx4ib=	mlx4ib
_mlxen=		mlxen
.endif
_mly=		mly
.if ${MK_OFED} != "no" || defined(ALL_MODULES)
_mthca=		mthca
.endif
_nfe=		nfe
_nvd=		nvd
_nvme=		nvme
_nvram=		nvram
_nxge=		nxge
.if ${MK_CRYPT} != "no" || defined(ALL_MODULES)
_padlock=	padlock
_padlock_rng=	padlock_rng
_rdrand_rng=	rdrand_rng
.endif
_s3=		s3
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
.endif

.if ${MACHINE_CPUARCH} == "amd64"
_cloudabi64=	cloudabi64
_ixl=		ixl
_ixlv=		ixlv
_linux64=	linux64
_linux_common=	linux_common
_ntb=		ntb
_pms=		pms
_qlxge=		qlxge
_qlxgb=		qlxgb
_qlxgbe=	qlxgbe
_sfxge=		sfxge

.if ${MK_BHYVE} != "no" || defined(ALL_MODULES)
_vmm=		vmm
.endif
.endif

.if ${MACHINE_CPUARCH} == "i386"
# XXX some of these can move to the general case when de-i386'ed
# XXX some of these can move now, but are untested on other architectures.
_3dfx=		3dfx
_3dfx_linux=	3dfx_linux
_aic=		aic
_apm=		apm
_arcnet=	arcnet
.if ${MK_SOURCELESS_UCODE} != "no"
_ce=		ce
.endif
_coff=		coff
.if ${MK_SOURCELESS_UCODE} != "no"
_cp=		cp
.endif
_elink=		elink
_glxiic=	glxiic
_glxsb=		glxsb
_ibcs2=		ibcs2
_ie=		ie
_mse=		mse
_ncr=		ncr
_ncv=		ncv
_nsp=		nsp
_pcfclock=	pcfclock
_pst=		pst
_sbni=		sbni
_streams=	streams
_stg=		stg
_svr4=		svr4
_wds=		wds
.if ${MACHINE} == "i386"
.if ${MK_EISA} != "no"
_ahb=		ahb
.endif
_bios=		bios
_cm=		cm
.if ${MK_SOURCELESS_UCODE} != "no"
_ctau=		ctau
.endif
_dpt=		dpt
_ex=		ex
_wl=		wl
.elif ${MACHINE} == "pc98"
_canbepm=	canbepm
_canbus=	canbus
_ct=		ct
_pmc=		pmc
_snc=		snc
.endif
.endif

.if ${MACHINE_CPUARCH} == "arm"
_cfi=		cfi
_cpsw=		cpsw
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_agp=		agp
_an=		an
_bm=		bm
_cardbus=	cardbus
_cbb=		cbb
_cfi=		cfi
_cpufreq=	cpufreq
_drm=		drm
.if ${MK_CDDL} != "no" || defined(ALL_MODULES)
_dtrace=	dtrace
.endif
_exca=		exca
_nvram=		powermac_nvram
.if ${MK_CDDL} != "no" || defined(ALL_MODULES)
_opensolaris=	opensolaris
.endif
_pccard=	pccard
_wi=		wi
.endif

.if ${MACHINE_ARCH} == "powerpc64"
_drm2=		drm2
.if ${MK_ZFS} != "no" || defined(ALL_MODULES)
_zfs=		zfs
.endif
.endif

.if ${MACHINE_CPUARCH} == "sparc64"
_auxio=		auxio
_em=		em
_epic=		epic
_igb=		igb
.if ${MK_CDDL} != "no" || defined(ALL_MODULES)
_opensolaris=	opensolaris
.endif
.if ${MK_ZFS} != "no" || defined(ALL_MODULES)
_zfs=		zfs
.endif
.endif

.endif

SUBDIR+=${MODULES_EXTRA}

.for reject in ${WITHOUT_MODULES}
SUBDIR:= ${SUBDIR:N${reject}}
.endfor

# Calling kldxref(8) for each module is expensive.
.if !defined(NO_XREF)
.MAKEFLAGS+=	-DNO_XREF
afterinstall:
	@if type kldxref >/dev/null 2>&1; then \
		${ECHO} kldxref ${DESTDIR}${KMODDIR}; \
		kldxref ${DESTDIR}${KMODDIR}; \
	fi
.endif

.include "${SYSDIR}/conf/config.mk"

SUBDIR:= ${SUBDIR:u:O}

.include <bsd.subdir.mk>
