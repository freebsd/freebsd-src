# $FreeBSD$

.if exists(${.CURDIR}/../crypto) && !defined(NOCRYPT)
_random=	random
.endif

SUBDIR=	3dfx \
	accf_data \
	accf_http \
	agp \
	aha \
	aio \
	amr \
	an \
	aue \
	bge \
	bridge \
	ccd \
	coda \
	cue \
	dc \
	de \
	digi \
	dummynet \
	ed \
	fdescfs \
	fdc \
	firewire \
	fxp \
	gx \
	if_disc \
	if_ef \
	if_gif \
	if_gre \
	if_faith \
	if_ppp \
	if_sl \
	if_stf \
	if_tap \
	if_tun \
	if_vlan \
	ip6fw \
	ip_mroute_mod \
	ipfw \
	ispfw \
	joy \
	kue \
	lge \
	libiconv \
	libmchain \
	lnc \
	lpt \
	mac_biba \
	mac_bsdextended \
	mac_ifoff \
	mac_mls \
	mac_none \
	mac_seeotheruids \
	mac_test \
	md \
	mii \
	mlx \
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
	${_random} \
	rl \
	rp \
	sem \
	sf \
	sis \
	sk \
	sn \
	snp \
	ste \
	sym \
	syscons \
	sysvipc \
	ti \
	tl \
	twe \
	tx \
	txp \
	ucom \
	udbp \
	ufm \
	udf \
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

.if !defined(NO_IPFILTER) && ${MACHINE_ARCH} != "ia64"
SUBDIR+=ipfilter
.endif

#removed while KSE settles in:
#	ncp \
#	nwfs \
# XXX some of these can move to the general case when de-i386'ed
# XXX some of these can move now, but are untested on other architectures.
.if ${MACHINE} == "i386"
SUBDIR+=aac \
	acpi \
	aic \
	aout \
	ar \
	arcnet \
	apm \
	asr \
	atspeaker \
	bktr \
	cam \
	cardbus \
	cbb \
	cd9660 \
	ciss \
	cm \
	coff \
	drm \
	el \
	em \
	ep \
	exca \
	fe \
	fpu \
	gnufpu \
	hea \
	hfa \
	ibcs2 \
	iir \
	linprocfs \
	linux \
	lomac \
	ncv \
	nsp \
	mly \
	netgraph \
	oltr \
	pecoff \
	pccard \
	ray \
	s3 \
	sbni \
	scsi_low \
	smbfs \
	sound \
	splash \
	sppp \
	sr \
	stg \
	streams \
	vesa \
	vinum \
	wi \
	xe
.endif

.if ${MACHINE} == "pc98"
SUBDIR+=aic \
	aout \
	ar \
	arcnet \
	apm \
	atspeaker \
	bktr \
	cam \
	cd9660 \
	coff \
	em \
	fe \
	fpu \
	gnufpu \
	hea \
	hfa \
	ibcs2 \
	linprocfs \
	linux \
	lomac \
	ncv \
	nsp \
	netgraph \
	oltr \
	pecoff \
	pmc \
	ray \
	sbni \
	scsi_low \
	smbfs \
	snc \
	sound \
	splash \
	sppp \
	sr \
	stg \
	streams \
	vinum \
	wi \
	xe
.endif

.if ${MACHINE_ARCH} == "ia64"
SUBDIR+=aic \
	arcnet \
	ciss \
	cm \
	coff \
	el \
	em \
	fe \
	iir \
	mly \
	ray \
	scsi_low \
	smbfs \
	splash \
	sr \
	streams \
	wi \
	xe
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=cam \
	cd9660 \
	linprocfs \
	linux \
	lomac \
	osf1 \
	sound \
	sppp \
	vinum
.endif

.if defined(WANT_EXT2FS_MODULE)
SUBDIR+=ext2fs
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
