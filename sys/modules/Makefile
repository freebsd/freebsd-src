# $FreeBSD$

.if exists(${.CURDIR}/../opencrypto) && !defined(NOCRYPT)
_crypto=	crypto
_cryptodev=	cryptodev
.endif
.if exists(${.CURDIR}/../crypto) && !defined(NOCRYPT)
_random=	random
.endif

# Removed while KSE settles in:
#	ncp \
#	nwfs \

SUBDIR=	accf_data \
	accf_http \
	agp \
	aha \
	aic7xxx \
	aio \
	amr \
	an \
	aue \
	bge \
	bridge \
	cam \
	ccd \
	cd9660 \
	coda \
	${_crypto} \
	${_cryptodev} \
	cue \
	dc \
	de \
	digi \
	dummynet \
	fdc \
	fdescfs \
	firewire \
	fxp \
	gx \
	hifn \
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
	ip6fw \
	ip_mroute_mod \
	ipfw \
	isp \
	ispfw \
	joy \
	kue \
	lge \
	libiconv \
	libmchain \
	lpt \
	mac_biba \
	mac_bsdextended \
	mac_ifoff \
	mac_lomac \
	mac_mls \
	mac_none \
	mac_partition \
	mac_seeotheruids \
	mac_test \
	mcd \
	md \
	mii \
	mlx \
	mpt \
	msdosfs \
	nfsclient \
	nfsserver \
	nge \
	nmdm \
	ntfs \
	nullfs \
	pcn \
	plip \
	portalfs \
	ppbus \
	ppi \
	pps \
	procfs \
	pseudofs \
	raidframe \
	${_random} \
	rc \
	rl \
	rp \
	sf \
	sis \
	sk \
	sn \
	snp \
	ste \
	sym \
	sysvipc \
	ti \
	tl \
	trm \
	twe \
	tx \
	txp \
	ubsa \
	ubsec \
	ucom \
	udbp \
	udf \
	ufm \
	uftdi \
	ugen \
	uhid \
	ukbd \
	ulpt \
	umapfs \
	umass \
	umodem \
	ums \
	unionfs \
	uplcom \
	urio \
	usb \
	uscanner \
	uvisor \
	uvscom \
	vpo \
	vr \
	vx \
	wb \
	xl

.if defined(WANT_EXT2FS_MODULE)
SUBDIR+=ext2fs
.endif

.if !defined(NO_IPFILTER)
SUBDIR+=ipfilter
.endif

.if ${MACHINE_ARCH} != "sparc64"
SUBDIR+=syscons
.endif

# XXX some of these can move to the general case when de-i386'ed
# XXX some of these can move now, but are untested on other architectures.
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=3dfx \
	aic \
	aout \
	apm \
	ar \
	arcnet \
	awi \
	bktr \
	coff \
	ed \
	em \
	ep \
	fe \
	fpu \
	gnufpu \
	hea \
	hfa \
	ibcs2 \
	linprocfs \
	linux \
	lnc \
	ncv \
	netgraph \
	nsp \
	oltr \
	pccard \
	pecoff \
	ray \
	sbni \
	scsi_low \
	smbfs \
	sound \
	splash \
	sppp \
	sr \
	stg \
	streams \
	vinum \
	wi \
	xe

.if ${MACHINE} == "i386"
SUBDIR+=aac \
	acpi \
	asr \
	atspeaker \
	cardbus \
	cbb \
	ciss \
	cm \
	drm \
	el \
	exca \
	iir \
	mly \
	s3 \
	vesa
	
.elif ${MACHINE} == "pc98"
SUBDIR+=pcspeaker \
	pmc \
	snc
.endif
.endif

.if ${MACHINE_ARCH} == "ia64"
# Modules not enabled on ia64 (as compared to i386) include:
#	aac acpi aout apm atspeaker drm fpu gnufpu ibcs2 linprocfs linux ncv
#	nsp oltr pecoff s3 sbni stg vesa
SUBDIR+=aic \
	ar \
	arcnet \
	asr \
	bktr \
	cardbus \
	cbb \
	ciss \
	cm \
	coff \
	el \
	em \
	ep \
	exca \
	fe \
	hea \
	hfa \
	iir \
	mly \
	netgraph \
	pccard \
	ray \
	rc \
	scsi_low \
	smbfs \
	sound \
	splash \
	sppp \
	sr \
	streams \
	vinum \
	wi \
	xe
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=linprocfs \
	linux \
	osf1 \
	sound \
	sppp \
	vinum
.endif

.if defined(MODULES_OVERRIDE) && !defined(ALL_MODULES)
SUBDIR=${MODULES_OVERRIDE}
.endif

# Calling kldxref(8) for each module is expensive.
.if !defined(NO_XREF)
.MAKEFLAGS:=	${.MAKEFLAGS} -DNO_XREF
afterinstall:
	@if type kldxref >/dev/null 2>&1; then \
		${ECHO} kldxref ${DESTDIR}${KMODDIR}; \
		kldxref ${DESTDIR}${KMODDIR}; \
	fi
.endif

.include <bsd.subdir.mk>
