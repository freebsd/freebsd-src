# $FreeBSD$

# pcic -- currently broken and being worked on out of tree.
# oldcard -- specialized use for debugging only.
# owi -- totally unsupported for debugging only.

SUBDIR=	${_3dfx} \
	${_aac} \
	accf_data \
	accf_http \
	${_acpi} \
	${_agp} \
	aha \
	${_ahb} \
	${_aic} \
	aic7xxx \
	aio \
	${_amd} \
	amr \
	an \
	${_aout} \
	${_apm} \
	${_ar} \
	${_arcnet} \
	${_arl} \
	${_asr} \
	ath \
	${_ath_hal} \
	ath_rate_amrr \
	ath_rate_onoe \
	aue \
	${_auxio} \
	${_awi} \
	axe \
	bfe \
	bge \
	${_bios} \
	${_bktr} \
	bridge \
	cam \
	${_canbepm} \
	${_canbus} \
	${_cardbus} \
	${_cbb} \
	cd9660 \
	cd9660_iconv \
	${_ciss} \
	${_cm} \
	coda \
	coda5 \
	${_coff} \
	${_cp} \
	${_crypto} \
	${_cryptodev} \
	${_ctau} \
	cue \
	${_cx} \
	dc \
	dcons \
	dcons_crom \
	de \
	digi \
	${_dpt} \
	${_drm} \
	dummynet \
	${_ed} \
	${_el} \
	${_elink} \
	${_em} \
	en \
	${_ep} \
	${_ex} \
	${_exca} \
	${_ext2fs} \
	fatm \
	fdc \
	fdescfs \
	${_fe} \
	firewire \
	fxp \
	${_gem} \
	geom \
	harp \
	hatm \
	${_hfa} \
	hifn \
	hme \
	${_hptmv} \
	${_i2c} \
	${_ibcs2} \
	${_ichwd} \
	${_ida} \
	${_idt} \
	${_ie} \
	if_disc \
	if_ef \
	if_faith \
	if_gif \
	if_gre \
	if_ppp \
	if_sl \
	if_stf \
	if_tap \
	if_tun \
	if_vlan \
	${_iir} \
	${_io} \
	ip6fw \
	ipdivert \
	${_ipfilter} \
	ipfw \
	ip_mroute_mod \
	${_ips} \
	isp \
	ispfw \
	joy \
	kue \
	lge \
	libiconv \
	libmbpool \
	libmchain \
	${_linprocfs} \
	${_linux} \
	${_lnc} \
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
	mcd \
	md \
	mem \
	mii \
	mlx \
	${_mly} \
	mpt \
	${_mse} \
	msdosfs \
	msdosfs_iconv \
	my \
	${_ncp} \
	${_ncv} \
	${_ndis} \
	netgraph \
	nfsclient \
	nfsserver \
	nge \
	nmdm \
	${_nsp} \
	ntfs \
	ntfs_iconv \
	nullfs \
	${_nwfs} \
	${_oltr} \
	${_osf1} \
	patm \
	${_pccard} \
	${_pcfclock} \
	pcn \
	${_pecoff} \
	${_pf} \
	plip \
	${_pmc} \
	portalfs \
	ppbus \
	ppi \
	pps \
	procfs \
	pseudofs \
	pst \
	${_random} \
	${_ray} \
	rc \
	rc4 \
	re \
	rl \
	rp \
	rue \
	${_s3} \
	${_safe} \
	${_sbni} \
	sbsh \
	scd \
	${_scsi_low} \
	sf \
	sis \
	sk \
	${_smbfs} \
	sn \
	${_snc} \
	snp \
	${_sound} \
	${_speaker} \
	${_splash} \
	${_sppp} \
	${_sr} \
	ste \
	${_stg} \
	${_streams} \
	sym \
	${_syscons} \
	sysvipc \
	ti \
	tl \
	trm \
	${_twa} \
	twe \
	tx \
	txp \
	uart \
	ubsa \
	ubsec \
	ubser \
	ucom \
	ucycom \
	udav \
	udbp \
	udf \
	udf_iconv \
	ufm \
	${_ufs} \
	uftdi \
	ugen \
	uhid \
	ukbd \
	ulpt \
	umass \
	umct \
	umodem \
	ums \
	unionfs \
	uplcom \
	urio \
	usb \
	uscanner \
	utopia \
	uvisor \
	uvscom \
	${_vesa} \
	vge \
	vkbd \
	vpo \
	vr \
	vx \
	wb \
	${_wi} \
	wlan \
	wlan_ccmp \
	wlan_tkip \
	wlan_wep \
	wlan_xauth \
	${_xe} \
	xl

.if ${MACHINE_ARCH} != "sparc64"
_syscons=	syscons
.endif

.if defined(ALL_MODULES)
_ufs=		ufs
.endif

.if !defined(NO_CRYPT) || defined(ALL_MODULES)
.if exists(${.CURDIR}/../opencrypto)
_crypto=	crypto
_cryptodev=	cryptodev
.endif
.if exists(${.CURDIR}/../crypto)
_random=	random
.endif
.endif

.if !defined(NO_IPFILTER) || defined(ALL_MODULES)
_ipfilter=	ipfilter
.endif

.if !defined(NO_PF) || defined(ALL_MODULES)
_pf=		pf
.endif

.if ${MACHINE_ARCH} == "i386"
# XXX some of these can move to the general case when de-i386'ed
# XXX some of these can move now, but are untested on other architectures.
_3dfx=		3dfx
_agp=		agp
_aic=		aic
_amd=		amd
_aout=		aout
_apm=		apm
_ar=		ar
_arcnet=	arcnet
_ath_hal=	ath_hal
_awi=		awi
_bktr=		bktr
_coff=		coff
_cp=		cp
_drm=		drm
_ed=		ed
_elink=		elink
_em=		em
_ep=		ep
_ext2fs=	ext2fs
_fe=		fe
_hfa=		hfa
_i2c=		i2c
_ibcs2=		ibcs2
_ie=		ie
_io=		io
_linprocfs=	linprocfs
_linux=		linux
_lnc=		lnc
_mse=		mse
_ncp=		ncp
_ncv=		ncv
_ndis=		ndis
_nsp=		nsp
_nwfs=		nwfs
_oltr=		oltr
_pccard=	pccard
_pcfclock=	pcfclock
_pecoff=	pecoff
_ray=		ray
_safe=		safe
_sbni=		sbni
_scsi_low=	scsi_low
_smbfs=		smbfs
_sound=		sound
_speaker=	speaker
_splash=	splash
_sppp=		sppp
_sr=		sr
_stg=		stg
_streams=	streams
_wi=		wi
_xe=		xe
.if ${MACHINE} == "i386"
_aac=		aac
_acpi=		acpi
_ahb=		ahb
_arl=		arl
_asr=		asr
_bios=		bios
_cardbus=	cardbus
_cbb=		cbb
_ciss=		ciss
_cm=		cm
_ctau=		ctau
_cx=		cx
_dpt=		dpt
_el=		el
_ex=		ex
_exca=		exca
_hptmv=		hptmv
_ichwd=		ichwd
_ida=		ida
_idt=		idt
_iir=		iir
_ips=		ips
_mly=		mly
_s3=		s3
_twa=		twa
_vesa=		vesa
.elif ${MACHINE} == "pc98"
_canbepm=	canbepm
_canbus=	canbus
_pmc=		pmc
_snc=		snc
.endif
.endif

.if ${MACHINE_ARCH} == "alpha"
_agp=		agp
_ahb=		ahb
_ext2fs=	ext2fs
_linprocfs=	linprocfs
_linux=		linux
_osf1=		osf1
_sound=		sound
_sppp=		sppp
.endif

.if ${MACHINE_ARCH} == "amd64"
_aac=		aac
#_acpi=		acpi		# doesn't work on amd64 yet
_agp=		agp
_ciss=		ciss
_drm=		drm
_em=		em
_ext2fs=	ext2fs
_i2c=		i2c
_ida=		ida
_iir=		iir
_io=		io
_ips=		ips
_lnc=		lnc
_mly=		mly
#_ndis=		ndis
_safe=		safe
_scsi_low=	scsi_low
_smbfs=		smbfs
_sound=		sound
_sppp=		sppp
_twa=		twa
.endif

.if ${MACHINE_ARCH} == "ia64"
# Modules not enabled on ia64 (as compared to i386) include:
#	aac acpi aout apm atspeaker drm ibcs2 linprocfs linux ncv
#	nsp oltr pecoff s3 sbni stg vesa
# acpi is not enabled because it is broken as a module on ia64
_aic=		aic
_ar=		ar
_arcnet=	arcnet
_asr=		asr
_bktr=		bktr
_cardbus=	cardbus
_cbb=		cbb
_ciss=		ciss
_cm=		cm
_coff=		coff
_el=		el
_em=		em
_ep=		ep
_exca=		exca
_fe=		fe
_hfa=		hfa
_iir=		iir
_mly=		mly
_pccard=	pccard
_ray=		ray
_scsi_low=	scsi_low
_smbfs=		smbfs
_sound=		sound
_splash=	splash
_sppp=		sppp
_sr=		sr
_streams=	streams
_wi=		wi
_xe=		xe
.endif

.if ${MACHINE_ARCH} == "powerpc"
_gem=		gem
.endif

.if ${MACHINE_ARCH} == "sparc64"
_auxio=		auxio
_gem=		gem
_sound=		sound
.endif

.if defined(MODULES_OVERRIDE) && !defined(ALL_MODULES)
SUBDIR=${MODULES_OVERRIDE}
.endif

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

.include <bsd.subdir.mk>
